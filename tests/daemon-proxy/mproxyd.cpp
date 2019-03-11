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

xclDeviceHandle uHandle;
pthread_t mpd_id;
pthread_t msd_id;

struct s_handle {
        xclDeviceHandle uDevHandle;
};

struct drm_xocl_sw_mailbox {
    uint64_t flags;
    uint32_t *data;
    bool is_tx;
    size_t sz;
    uint64_t id;
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
    struct s_handle *s_handle_ptr = (struct s_handle *)handle_ptr;
    xclDeviceHandle handle = s_handle_ptr->uDevHandle;
    size_t prev_sz = INIT_BUF_SZ;
    struct drm_xocl_sw_mailbox args = { 0, 0, true, prev_sz, 0 };
    args.data = (uint32_t *)malloc(prev_sz);

    std::cout << "[XOCL->XCLMGMT Intercept ON (HAL)]\n";
    for( ;; ) {
        args.is_tx = true;
        args.sz = prev_sz;
        ret = xclMPD(handle, &args);
        if( ret != 0 ) {
            // sw channel xfer error 
            if( errno == EMSGSIZE ) {
                // buffer was of insufficient size, resizing
                if( resize_buffer( args.data, args.sz ) != 0 ) {
                    std::cout << "MPD: resize_buffer() failed...exiting\n";
                    exit(1);
                }
                prev_sz = args.sz; // store the newly alloc'd size
                ret = xclMPD(handle, &args);
            } else {
                std::cout << "MPD: transfer failed for other reason\n";
                exit(1);
            }

            if( ret != 0 ) {
                std::cout << "MPD: second transfer failed, exiting.\n";
                exit(1);
            }
        }
        std::cout << "[MPD-TX]\n";

        args.is_tx = false;
        ret = xclMSD(handle, &args);
        if( ret != 0 ) {
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
    struct s_handle *s_handle_ptr = (struct s_handle *)handle_ptr;
    xclDeviceHandle handle = s_handle_ptr->uDevHandle;
    size_t prev_sz = INIT_BUF_SZ;
    struct drm_xocl_sw_mailbox args = { 0, 0, true, prev_sz, 0 };
    args.data = (uint32_t *)malloc(prev_sz);

    std::cout << "               [XCLMGMT->XOCL Intercept ON (HAL)]\n";
    for( ;; ) {
        args.is_tx = true;
        args.sz = prev_sz;
        ret = xclMSD(handle, &args);
        if( ret != 0 ) {
            // sw channel xfer error 
            if( errno == EMSGSIZE ) {
                // buffer was of insufficient size, resizing
                if( resize_buffer( args.data, args.sz ) != 0 ) {
                    std::cout << "              MSD: resize_buffer() failed...exiting\n";
                    exit(1);
                }
                prev_sz = args.sz; // store the newly alloc'd size
                ret = xclMSD(handle, &args);
            } else {
                std::cout << "              MSD: transfer failed for other reason\n";
                exit(1);
            }
        }

        std::cout << "                [MSD-TX]\n";

        args.is_tx = false;
        ret = xclMPD(handle, &args);
        if( ret != 0 ) {
            std::cout << "                    MPD: transfer error: " << strerror(errno) << std::endl;
            exit(1);
        }
        std::cout << "                [MPD-RX] " << xferCount << std::endl;
        xferCount++;
    }

    std::cout << "Exit thread XCLMGMT->XOCL\n";
}

int init( unsigned idx )
{
    xclDeviceInfo2 deviceInfo;
    unsigned deviceIndex = idx;

    if( deviceIndex >= xclProbe() ) {
        throw std::runtime_error("Cannot find specified device index");
        return -ENODEV;
    }

    uHandle = xclOpen(deviceIndex, NULL, XCL_INFO);
    struct s_handle devHandle = { uHandle };

    if( xclGetDeviceInfo2(uHandle, &deviceInfo) ) {
        throw std::runtime_error("Unable to obtain device information");
        return -EBUSY;
    }

    pthread_create(&mpd_id, NULL, mpd, &devHandle);
    pthread_create(&msd_id, NULL, msd, &devHandle);
}
void printHelp( void )
{
    std::cout << "Usage: <daemon_name> -d <device_index>" << std::endl;
    std::cout << "      '-d' is optional and will default to '0'" <<std::endl;
}

// For security purposes, we don't allow any arguments to be passed into the daemon
int main(int argc, char *argv[])
{
    unsigned index = 0;
    int c;

    while ((c = getopt(argc, argv, "d:h:")) != -1)
    {
        switch (c)
        {
        case 'd':
            index = std::atoi(optarg);
            break;
        case 'h':
            printHelp();
            return 0;
        default:
            printHelp();
            return -1;
        }
    }
 
    // Define variables
    pid_t pid, sid;
 
    // Fork the current process
    pid = fork();
    // The parent process continues with a process ID greater than 0
    if(pid > 0)
    {
        exit(EXIT_SUCCESS);
    }
    // A process ID lower than 0 indicates a failure in either process
    else if(pid < 0)
    {
        exit(EXIT_FAILURE);
    }
    // The parent process has now terminated, and the forked child process will continue
    // (the pid of the child process was 0)
 
    // Since the child process is a daemon, the umask needs to be set so files and logs can be written
    umask(0);
 
    // Open system logs for the child process
    openlog("daemon-named", LOG_NOWAIT | LOG_PID, LOG_USER);
    syslog(LOG_NOTICE, "Successfully started daemon-name");
 
    // Generate a session ID for the child process
    sid = setsid();
    // Ensure a valid SID for the child process
    if(sid < 0)
    {
        // Log failure and exit
        syslog(LOG_ERR, "Could not generate session ID for child process");
 
        // If a new session ID could not be generated, we must terminate the child process
        // or it will be orphaned
        exit(EXIT_FAILURE);
    }
 
    // Change the current working directory to a directory guaranteed to exist
    if((chdir("/")) < 0)
    {
        // Log failure and exit
        syslog(LOG_ERR, "Could not change working directory to /");
 
        // If our guaranteed directory does not exist, terminate the child process to ensure
        // the daemon has not been hijacked
        exit(EXIT_FAILURE);
    }
 
    // A daemon cannot use the terminal, so close standard file descriptors for security reasons
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
 
    // Daemon-specific intialization should go here
    const int SLEEP_INTERVAL = 5;

    init(index);
  
    // Enter daemon loop
    pthread_join(mpd_id, NULL);
    pthread_join(msd_id, NULL);

    // Close system logs for the child process
    xclClose(uHandle);
    syslog(LOG_NOTICE, "Stopping daemon-name");
    closelog();
 
    // Terminate the child process when the daemon completes
    exit(EXIT_SUCCESS);
}

