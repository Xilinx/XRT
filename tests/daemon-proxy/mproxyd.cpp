/*
 * ryan.radjabi@xilinx.com
 *
 * Reference for daemonization: https://gist.github.com/alexdlaird/3100f8c7c96871c5b94e
 */
#include <dirent.h>
#include <iterator>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
#include <vector>
#include <stdbool.h>
#include <stdint.h>
#include <stdexcept>
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <libdrm/drm.h>
#include <pthread.h>
#include <time.h>
#include <getopt.h>

#include "xclhal2.h"

#define INIT_BUF_SZ 64

pthread_t mpd_id;
pthread_t msd_id;

struct s_handles {
        int userfd;
        int mgmtfd;
};

struct drm_xocl_sw_mailbox {
    size_t sz;
    uint64_t flags;
    bool is_tx;
    uint64_t id;
    uint32_t *data;
};

int resize_buffer( uint32_t *&buf, const size_t new_sz )
{
    if( buf != NULL ) {
        free(buf);
        buf = NULL;
    }
    buf = (uint32_t *)malloc( new_sz );
    if( buf == NULL ) {
        std::cout << "alloc failed \n";
        return -1;
    }

    return 0;
}

void *mpd(void *handle_ptr)
{
    int xferCount = 0;
    int ret;
    struct s_handles *h = (struct s_handles *)handle_ptr;
    size_t prev_sz = INIT_BUF_SZ;
    struct drm_xocl_sw_mailbox args = { prev_sz, 0, true, 0, 0 };
    args.data = (uint32_t *)malloc(prev_sz);

    std::cout << "[XOCL->XCLMGMT Intercept ON (HAL)]\n";
    for( ;; ) {
        args.is_tx = true;
        args.sz = prev_sz;
        ret = read( h->userfd, &args, (sizeof(struct drm_xocl_sw_mailbox) + args.sz) );
        std::cout << "MPD: read() ret: " << ret << ", errno: " << errno << std::endl;
        if( ret <= 0 ) {
            std::cout << "MPD: debug #0 read() ret: " << ret << ", errno: " << errno << std::endl;
            // sw channel xfer error 
            if( errno != EMSGSIZE ) {
                std::cout << "MPD: debug #1\n";
                std::cout << "MPD: transfer failed for other reason\n";
                exit(1);
            }
            std::cout << "MPD: debug #2\n";
            // buffer was of insufficient size, resizing
            if( resize_buffer( args.data, args.sz ) != 0 ) {
                std::cout << "MPD: debug #3\n";
                std::cout << "MPD: resize_buffer() failed...exiting\n";
                exit(1);
            }
            std::cout << "MPD: debug #4\n";
            prev_sz = args.sz; // store the newly alloc'd size
            ret = read( h->userfd, &args, (sizeof(struct drm_xocl_sw_mailbox) + args.sz) );
            std::cout << "MPD: debug #5 read() ret: " << ret << ", errno: " << errno << std::endl;
            if( ret < 0 ) {
                std::cout << "MPD: second transfer failed, exiting.\n";
                exit(1);
            }
        }
        std::cout << "[MPD-TX]\n";

        args.is_tx = false;
        ret = write( h->mgmtfd, &args, (sizeof(struct drm_xocl_sw_mailbox) + args.sz) );
        std::cout << "MPD: write() ret: " << ret << std::endl;
        if( ret <= 0 ) {
            std::cout << "MSD: transfer error: " << strerror(errno) << std::endl;
            exit(1);
        }
        std::cout << "[MSD-RX]: " << xferCount << std::endl;
        xferCount++;
    }
    std::cout << "Exit thread XOCL->XCLMGMT\n";
}

void *msd(void *handle_ptr)
{
    int xferCount = 0;
    int ret;
    struct s_handles *h = (struct s_handles *)handle_ptr;
    size_t prev_sz = INIT_BUF_SZ;
    struct drm_xocl_sw_mailbox args = { prev_sz, 0, true, 0, 0 };
    args.data = (uint32_t *)malloc(prev_sz);

    std::cout << "               [XCLMGMT->XOCL Intercept ON (HAL)]\n";
    for( ;; ) {
        args.is_tx = true;
        args.sz = prev_sz;
        ret = read( h->mgmtfd, &args, (sizeof(struct drm_xocl_sw_mailbox) + args.sz) );
        std::cout << "MSD: read() ret: " << ret << std::endl;
        if( ret <= 0 ) {
            // sw channel xfer error 
            if( errno != EMSGSIZE ) {
                std::cout << "              MSD: transfer failed for other reason\n";
                exit(1);
            }
            // buffer was of insufficient size, resizing
            if( resize_buffer( args.data, args.sz ) != 0 ) {
                std::cout << "              MSD: resize_buffer() failed...exiting\n";
                exit(1);
            }
            prev_sz = args.sz; // store the newly alloc'd size
            ret = read( h->mgmtfd, &args, (sizeof(struct drm_xocl_sw_mailbox) + args.sz) );
            std::cout << "MSD: read() ret: " << ret << std::endl;
            if( ret < 0 ) {
                std::cout << "MPD: second transfer failed, exiting.\n";
                exit(1);
            }
        }
        std::cout << "                [MSD-TX]\n";

        args.is_tx = false;
        ret = write( h->userfd, &args, (sizeof(struct drm_xocl_sw_mailbox) + args.sz) );
        std::cout << "MSD: write() ret: " << ret << std::endl;
        if( ret <= 0 ) {
            std::cout << "                    MPD: transfer error: " << strerror(errno) << std::endl;
            exit(1);
        }
        std::cout << "                [MPD-RX] " << xferCount << std::endl;
        xferCount++;
    }
    std::cout << "Exit thread XCLMGMT->XOCL\n";
}

int main(void)
{

    struct s_handles handles;

    const char mgmt_name[] = "/dev/mailbox.m2560";
    if ((handles.mgmtfd = open(mgmt_name,O_RDWR)) == -1) {
        perror("open");
        exit(1);
    }
    const char user_name[] = "/dev/mailbox.u2561";
    if ((handles.userfd = open(user_name,O_RDWR)) == -1) {
        perror("open");
        exit(1);
    }

    pthread_create(&mpd_id, NULL, mpd, &handles);
    pthread_create(&msd_id, NULL, msd, &handles);
    
    pthread_join(mpd_id, NULL);
    pthread_join(msd_id, NULL);

    return 0;
}

