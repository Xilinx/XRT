/*
 * ryan.radjabi@xilinx.com
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
    uint32_t *pData;
    bool isTx;
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
    uint64_t flags = 0;
    uint64_t id = 0;
    struct s_handle *s_handle_ptr = (struct s_handle *)handle_ptr;
    xclDeviceHandle handle = s_handle_ptr->uDevHandle;
    size_t prev_sz = INIT_BUF_SZ;
    uint32_t *buf1;
    buf1 = (uint32_t *)malloc(prev_sz);
    struct drm_xocl_sw_mailbox args = { flags, buf1, true, prev_sz, id };

    printf( "[XOCL->XCLMGMT Intercept ON (HAL)]\n" );
    for( ;; ) {
        args.isTx = true;
        args.sz = prev_sz;
        ret = xclMbxUserDaemon(handle, &args);
        if( ret != 0 ) {
            //std::cout << "sw channel xfer errno=" << errno << " (" << strerror(errno) << ")\n";
            if( errno == EMSGSIZE ) {
                //std::cout << "buffer was of insufficient size, resizing from: " << prev_sz << " to: " << args.sz;
                if( resize_buffer( buf1, args.sz ) != 0 ) {
                    std::cout << "user: resize_buffer() failed...exiting\n";
                    exit(1);
                }
                prev_sz = args.sz;
                args.pData = buf1;
                ret = xclMbxUserDaemon(handle, &args);
            } else {
                std::cout << "user: transfer failed for other reason\n";
                exit(1);
            }

            if( ret != 0 ) {
                std::cout << "user: second transfer failed, exiting.\n";
                exit(1);
            }
        }
        printf("[XOCL-PKT-TX]\n");

        args.isTx = false;
        ret = xclMbxMgmtDaemon(handle, &args);
        if( ret != 0 ) {
            std::cout << "mgmt: transfer error: " << strerror(errno) << std::endl;
            break;
        }
        std::cout << "[MGMT-PKT-RX]: " << xferCount << std::endl;
        xferCount++;
    }
    std::cout << "Exit thread XOCL->XCLMGMT\n";
}

void *msd(void *handle_ptr)
{
    int xferCount = 0;
    int ret;
    uint64_t flags = 0;
    uint64_t id = 0;
    struct s_handle *s_handle_ptr = (struct s_handle *)handle_ptr;
    xclDeviceHandle handle = s_handle_ptr->uDevHandle;
    size_t prev_sz = INIT_BUF_SZ;
    uint32_t *buf2;
    buf2 = (uint32_t *)malloc(prev_sz);
    struct drm_xocl_sw_mailbox args = { flags, buf2, true, prev_sz, id };

    std::cout << "               [XCLMGMT->XOCL Intercept ON (HAL)]\n";
    for( ;; ) {
        args.isTx = true;
        args.sz = prev_sz;
        ret = xclMbxMgmtDaemon(handle, &args);
        if( ret != 0 ) {
            //std::cout << "sw channel xfer errno=" << errno << " (" << strerror(errno) << ")\n";
            if( errno == EMSGSIZE ) {
                //std::cout << "buffer was of insufficient size, resizing from: " << prev_sz << " to: " << args.sz;
                if( resize_buffer( buf2, args.sz ) != 0 ) {
                    std::cout << "              mgmt: resize_buffer() failed...exiting\n";
                    exit(1);
                }
                prev_sz = args.sz;
                args.pData = buf2;
                ret = xclMbxMgmtDaemon(handle, &args);
            } else {
                std::cout << "              mgmt: transfer failed for other reason\n";
                exit(1);
            }
        }

        printf("                [MGMT-PKT-TX]\n");

        args.isTx = false;
        ret = xclMbxUserDaemon(handle, &args);
        if( ret != 0 ) {
            std::cout << "                    user: transfer error: " << strerror(errno) << std::endl;
            exit(1);
        }
        std::cout << "                [XOCL-PKT-RX] " << xferCount << std::endl;
        xferCount++;
    }

    std::cout << "Exit thread XCLMGMT->XOCL\n";
}

int init(void)
{
    xclDeviceInfo2 deviceInfo;
    unsigned deviceIndex = 0;

    deviceIndex = 0;
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

// For security purposes, we don't allow any arguments to be passed into the daemon
int main(void)
{
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

   init();

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

