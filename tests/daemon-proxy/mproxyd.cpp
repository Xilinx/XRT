/*
 * ryan.radjabi@xilinx.com
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <sys/ioctl.h>
#include <libdrm/drm.h>
#include <pthread.h>
#include <time.h>
#include <iostream>

#include "xclhal2.h"

#define INIT_BUF_SZ 39523893//4923871//128

/*
 * struct drm_xocl_sw_mailbox *args
 **/
struct drm_xocl_sw_mailbox {
    uint64_t flags;
    uint32_t *pData;
    bool isTx;
    size_t sz;
    uint64_t id;
};

struct s_handle {
    xclDeviceHandle uDevHandle;
    xclDeviceHandle mDevHandle;
};


int resize_buffer( uint32_t *&buf, const size_t new_sz )
{
    std::cout << "Desired buffer size " << new_sz << std::endl;
    std::cout << "Trying to free: " << buf << std::endl;
    free(buf);
    buf = (uint32_t *)malloc( new_sz );
    if( buf == NULL ) {
        std::cout << "calloc failed \n";
        return -1;
    }

    std::cout << "resize_buffer Success: new size=" << new_sz << std::endl;

    return 0;
}

void *xcl_sw_chan_xocl_to_xclmgmt(void *s_handle_ptr)
{
    int xferCount = 0;
    int ret;
    uint64_t flags = 0;
    uint64_t id = 0;
    struct s_handle *handle = (struct s_handle*)s_handle_ptr;
    size_t prev_sz = INIT_BUF_SZ;
    uint32_t *gBuf1;
    gBuf1 = (uint32_t *)malloc(prev_sz);
    struct drm_xocl_sw_mailbox args = { flags, gBuf1, true, prev_sz, id };

    printf( "[XOCL->XCLMGMT Intercept ON (HAL)]\n" );
    for( ;; ) {
        args.isTx = true;
        args.sz = prev_sz;
        ret = xclDaemonUserpf(handle->uDevHandle, &args);
        if( ret != 0 ) {
            std::cout << "sw channel xfer errno=" << errno << " (" << strerror(errno) << ")\n";
            if( errno == EMSGSIZE ) {
                std::cout << "buffer was of insufficient size, resizing from: " << prev_sz << " to: " << args.sz;
                if( resize_buffer( gBuf1, args.sz ) != 0 ) {
                    std::cout << "resize_buffer() failed...exiting\n";
                    break;
                }
                prev_sz = args.sz;
                ret = xclDaemonUserpf(handle->uDevHandle, &args);
            } else {
                std::cout << "user-transfer failed for other reason\n";
                break;
            }
            if( ret != 0 ) {
                std::cout << "second transfer failed, exiting.\n";
                exit(1);
            }
        }
        printf("[XOCL-PKT-TX]\n");

        args.isTx = false;
        ret = xclDaemonMgmtpf(handle->uDevHandle, &args);
        if( ret != 0 ) {
            printf("mgmt-transfer Errno: %s\n", strerror(errno));
            break;
        }
        printf("[MGMT-PKT-RX]: %i\n", xferCount);
        xferCount++;
    }
    printf("Exit thread XOCL->XCLMGMT\n");
}

void *xcl_sw_chan_xclmgmt_to_xocl(void *s_handle_ptr)
{
    int xferCount = 0;
    int ret;
    uint64_t flags = 0;
    uint64_t id = 0;
    struct s_handle *handle = (struct s_handle*)s_handle_ptr;
    size_t prev_sz = INIT_BUF_SZ;
    uint32_t *gBuf2;
    gBuf2 = (uint32_t *)malloc(prev_sz);
    struct drm_xocl_sw_mailbox args = { flags, gBuf2, true, prev_sz, id };

    printf( "[XCLMGMT->XOCL Intercept ON (HAL)]\n" );
    for( ;; ) {
        args.isTx = true;
        args.sz = prev_sz;
        ret = xclDaemonMgmtpf(handle->uDevHandle, &args);
        if( ret != 0 ) {
            std::cout << "sw channel xfer errno=" << errno << " (" << strerror(errno) << ")\n";
            if( errno == EMSGSIZE ) {
                std::cout << "buffer was of insufficient size, resizing from: " << prev_sz << " to: " << args.sz;
                if( resize_buffer( gBuf2, args.sz ) != 0 ) {
                    std::cout << "resize_buffer() failed...exiting\n";
                    break;
                }
                prev_sz = args.sz;
            } else {
                std::cout << "              mgmt-transfer failed for other reason\n";
                break;
            }
        }

        printf("                [MGMT-PKT-TX]\n");

        args.isTx = false;
        ret = xclDaemonUserpf(handle->uDevHandle, &args);
        if( ret != 0 ) {
            printf("                    user-transfer Errno: %s\n", strerror(errno));
            break;
        }
        printf("                [XOCL-PKT-RX] %i\n", xferCount);
        xferCount++;
    }

    printf("Exit thread XCLMGMT->XOCL\n");
}


int main(void)
{
    xclDeviceInfo2 deviceInfo;
    unsigned deviceIndex = 0;
    xclDeviceHandle uHandle;
    xclDeviceHandle mHandle; // never init'd



    deviceIndex = 0;
    if( deviceIndex >= xclProbe() ) {
        throw std::runtime_error("Cannot find specified device index");
        return -ENODEV;
    }

    uHandle = xclOpen(deviceIndex, NULL, XCL_INFO);

    struct s_handle devHandle = { uHandle, mHandle };

    if( xclGetDeviceInfo2(uHandle, &deviceInfo) ) {
        throw std::runtime_error("Unable to obtain device information");
        return -EBUSY;
    }

    pthread_t xcl_xocl_to_xclmgmt_id;
    pthread_create(&xcl_xocl_to_xclmgmt_id, NULL, xcl_sw_chan_xocl_to_xclmgmt, &devHandle);

    pthread_t xcl_xclmgmt_to_xocl_id;
    pthread_create(&xcl_xclmgmt_to_xocl_id, NULL, xcl_sw_chan_xclmgmt_to_xocl, &devHandle);

    pthread_join(xcl_xocl_to_xclmgmt_id, NULL);
    pthread_join(xcl_xclmgmt_to_xocl_id, NULL);

    xclClose(uHandle);

    return 0;
}
