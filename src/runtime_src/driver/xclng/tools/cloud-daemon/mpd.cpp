/*
 * ryan.radjabi@xilinx.com
 *
 * Private Cloud Management Proxy Daemon
 */
#include <iostream>
#include <string>
#include "xclhal2.h"
#include "common.h"

std::string host_ip;
std::string host_port;
std::string host_id;

/*
 * Example code to setup communication channel between vm and host.
 * TCP is being used here for example.
 * Cloud vendor should implement this function.
 */
static void mpd_comm_init(int *handle)
{
    int sockfd;
    struct sockaddr_in servaddr;

    // socket create and varification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket creation failed...");
        exit(1);
    } else {
        printf("Socket successfully created..\n");
    }
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr( host_ip.c_str() );
    servaddr.sin_port = htons( std::stoi( host_port.c_str() ) );

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        perror("connection with the server failed...");
        exit(1);
    } else {
        printf("connected to the server..\n");
    }

    *handle = sockfd;
}
int main(void)
{
    int comm_fd, local_fd = -1;
    const int numDevs = xclProbe();

    if (numDevs <= 0)
        return -ENODEV;

    for (int i = 0; i < numDevs; i++) {
        int ret = fork();
        if (ret < 0) {
            std::cout << "Failed to create child process: " << errno << std::endl;
            exit(errno);
        }
        if (ret == 0) { // child
            /* Ugly way to get host_ip, host_port, and cloud token(host_id) */
            char c_id[256];
            xclMailboxUserGetID(i, c_id);
            std::string s_id = std::string( c_id );
            size_t pos =  s_id.find(",");
            std::string rem = s_id.substr( pos+1, s_id.length() );
            host_ip = s_id.substr( 0, pos );
            pos =  rem.find(",");
            host_port = rem.substr( 0, pos );
            rem = rem.substr( pos+1, rem.length()-1 );
            host_id = rem.substr( 0, rem.length()-1 );

            mpd_comm_init(&comm_fd);

            /* handshake to MSD by sending cloud token id */
            uint32_t dataLength = htonl(host_id.size());
            send(comm_fd, &dataLength, sizeof(uint32_t), MSG_CONFIRM);
            send(comm_fd, host_id.c_str(), host_id.size(), MSG_CONFIRM);

            local_fd = xclMailbox( i );
            if (local_fd < 0) {
                std::cout << "xclMailbox(): " << errno << std::endl;
                return errno;
            }
            break;
        }
        // parent continues but will never create thread, and eventually exit
        std::cout << "New child process: " << ret << std::endl;
    }

    if ((comm_fd > 0) && (local_fd > 0)) {
        // run until daemon is killed
        mailbox_daemon(local_fd, comm_fd, "[MPD]");

        // cleanup when stopped
        close(comm_fd);
        close(local_fd);
    }
    return 0;
}

