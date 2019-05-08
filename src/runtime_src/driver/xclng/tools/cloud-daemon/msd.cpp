/*
 * ryan.radjabi@xilinx.com
 *
 * Private Cloud Management Service Daemon
 */
#include <iostream>
#include <string>
#include <wordexp.h>

#include "xclhal2.h"
#include "common.h"

std::string host_ip;
std::string host_port;
std::string mbx_switch;

/*
 * Example code to setup communication channel between vm and host.
 * TCP is being used here for example.
 * Cloud vendor should implement this function.
 */
static void msd_comm_init(int *handle)
{
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
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
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons( std::stoi( host_port.c_str() ) );

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        perror("socket bind failed...");
        exit(1);
    } else {
        printf("Socket successfully binded..\n");
    }

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        perror("Listen failed...");
        exit(1);
    } else {
        printf("Server listening..\n");
    }
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
        //there should be just one msd running on host, and multiple mpds
        //each of which runs on a VM. So there would be multiple tcp
        //connections established. each child here handles one connection
        if (!fork()) { //child
            close(sockfd);
            *handle = connfd;
            return;
        }
        //parent
        close(connfd);
        while(waitpid(-1,NULL,WNOHANG) > 0); /* clean up child processes */
    }
    //Assume the server never exits.
    printf("Never happen!!\n");
    exit(100);
}

int main( void )
{
    int comm_fd, local_fd = -1;

    // Read config file, store ip, port, and switch, later write to
    // mgmt sysf with xclMailboxMgmtPutID().
    wordexp_t p;
    char **w;
    wordexp( "$XILINX_XRT/etc/msd-host.config", &p, 0 );
    w = p.we_wordv;
    std::string config_path(*w);
    wordfree( &p );

    std::ifstream file( config_path );
    std::getline(file, host_ip);
    if (host_ip.length() <= std::string("ip=").length()) {
        std::cout << "Failed to parse config file: " << config_path << ", exiting.\n";
        file.close();
        return -EINVAL;
    }
    host_ip = host_ip.substr( std::string("ip=").length(), host_ip.length() );
    std::getline(file, host_port);
    host_port = host_port.substr( std::string("port=").length(), host_port.length() );
    std::getline(file, mbx_switch);
    mbx_switch = mbx_switch.substr( std::string("switch=").length(), mbx_switch.length() );
    file.close();

    // Write to config_mailbox_comm_id in format "127.0.0.1,12345,0", where 0 is the device index.
    int numDevs = 0;
    while (xclMailboxMgmtPutID(numDevs, std::string(host_ip+","+host_port+","+std::to_string(numDevs)+";").c_str(), mbx_switch.c_str()) != -ENODEV)
        numDevs++;

    for (int i = 0; i < numDevs; i++) {
        msd_comm_init(&comm_fd); // blocks waiting for connection, then forks

        // receive cloud token and map to device index
        int64_t cloud_token = -1;
        char *data = (char*)&cloud_token;
        recv( comm_fd, data, sizeof(cloud_token), 0 );
        std::cout << "cloud token received: " << cloud_token << std::endl;

        // only reached by child process
        local_fd = xclMailboxMgmt( cloud_token );
        if (local_fd < 0) {
            std::cout << "xclMailboxMgmt(): " << errno << std::endl;
            return errno;
        }
        break;
    }

    if ((comm_fd >= 0) && (local_fd >= 0)) {
        // run until daemon is killed
        mailbox_daemon(local_fd, comm_fd, "[MSD]");

        // cleanup when stopped
        close(comm_fd);
        close(local_fd);
    }

    return 0;
}

