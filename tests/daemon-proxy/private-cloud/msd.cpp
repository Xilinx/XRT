/*
 * ryan.radjabi@xilinx.com
 *
 * Private Cloud Management Service Daemon
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
#include "common.h"

#define INIT_BUF_SZ 64

// example code to setup communication channel between vm and host
// tcp is being used here as example.
// cloud vendor should implements this function
static void msd_comm_init(int *handle)
{
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket creation failed...");
        exit(1);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // get PORT from filesystem
    std::ifstream file( "/var/lib/libvirt/filesystem_passthrough/host_port" );
    std::string host_port;
    std::getline(file, host_port);
    file.close();

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons( std::stoi( host_port.c_str() ) );

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        perror("socket bind failed...");
        exit(1);
    }
    else
        printf("Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        perror("Listen failed...");
        exit(1);
    }
    else
        printf("Server listening..\n");
    len = sizeof(cli);

    while (1) {
        // Accept the data packet from client and verification
        connfd = accept(sockfd, (SA*)&cli, (socklen_t*)&len);
        if (connfd < 0) {
            perror("server acccept failed...");
            continue;
        } else {
            printf("server acccept the client...\n");
        }

        //In case there are multiple VMs created on the same host,
        //there should be hust one msd running on host, and multiple mpds
        //each of which runs on a VM. So there would be multiple tcp
        //connections established. each child here handles one connection
        //If we use udp, no children processes are required.
        if (!fork()) { //child
            close(sockfd);
            *handle = connfd;
            return;
        }
        //parent
        close(connfd);
        while(waitpid(-1,NULL,WNOHANG) > 0); /* clean up child processes */
    }
    //assume the server never exit.
    printf("Never happen!!\n");
    exit(100);
}

static void run(int local_fd, int comm_fd)
{
    size_t prev_sz = INIT_BUF_SZ;
    struct drm_xocl_sw_mailbox args = { prev_sz, 0, true, 0, 0 };
    args.data = (uint32_t *)malloc(prev_sz);
    char client_message[MSG_SZ];
    uint32_t *pdata = args.data;

    fd_set rfds;
    FD_ZERO(&rfds);
    int ret = 0;
#define max(a,b) (a>b?a:b)
    for (;;) {
        FD_SET(local_fd, &rfds);
        FD_SET(comm_fd, &rfds);
        ret = select(max(comm_fd, local_fd)+1, &rfds, NULL, NULL, NULL);

        if(ret == -1) {
            break;
        }

        if(FD_ISSET(local_fd, &rfds)) {
            static int msd_tx_count = 0;
            std::cout << "              [MSD-TX]:" << msd_tx_count << ".1 MSD TX IOCTL \n";
            if( local_read( local_fd, &args, prev_sz ) )
                exit(errno);

            std::cout << "              [MSD-TX]:" << msd_tx_count << ".2 send args over socket \n";
            comm_write_args( comm_fd, &args );

            std::cout << "              [MSD-TX]:" << msd_tx_count << ".3 send data over socket \n";
            comm_write_data( comm_fd, args.data, args.sz );

            std::cout << "              [MSD-TX]:" << msd_tx_count << " complete.\n";
            msd_tx_count++;
        }

        if(FD_ISSET(comm_fd, &rfds)) {
            static int msd_rx_count = 0;
            std::cout << "              [MSD-RX]:" << msd_rx_count << ".1 recv_args\n";
            if( comm_read_args( comm_fd, client_message, &args ) <= 0 )
                break;

            args.data = pdata;  // must do this after receive args
            args.is_tx = false; // must do this after receive args

            std::cout << "              [MSD-RX]:" << msd_rx_count << ".2 resize buffer\n";
            if( args.sz > prev_sz ) {
                std::cout << "args.sz(" << args.sz << ") > prev_sz(" << prev_sz << ") \n";
                resize_buffer( args.data, args.sz );
                prev_sz = args.sz;
            } else {
                std::cout << "don't need to resize buffer\n";
            }

            std::cout << "              [MSD-RX]:" << msd_rx_count << ".3 recv_data\n";
            if( comm_read_data( comm_fd, args.data, args.sz ) != 0 ) {
                std::cout << "bad retval from recv_data(), exiting.\n";
                exit(1);
            }

            std::cout << "              [MSD-RX]:" << msd_rx_count << ".4 xclMSD \n";
            if( local_write( local_fd, &args ) )
                exit(errno);
            std::cout << "              [MSD-RX]:" << msd_rx_count << " complete.\n";

            msd_rx_count++;
        }
    }
}

int main( void )
{
    int comm_fd, local_fd = -1;

    /* is there an alternative to xclProbe() to get num devs? */
    const int numDevs = 1;

    if (numDevs <= 0)
        return -ENODEV;

    for (int i = 0; i < numDevs; i++) {
        msd_comm_init(&comm_fd); // blocks waiting for connection, then forks

        // only reached by child process
        local_fd = xclMailboxMgmt(i);
        break;
    }

    if ((comm_fd >= 0) && (local_fd >= 0)) {
        // run until daemon is killed
        run(local_fd, comm_fd);

        // cleanup when stopped
        comm_fini(comm_fd);
        close(local_fd);
    } else {
        std::cout << "Invalid comm_fd and local_fd : " << comm_fd << ", " << local_fd << std::endl;
        return -1;
    }

    return 0;
}

