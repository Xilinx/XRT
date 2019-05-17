/*
 * ryan.radjabi@xilinx.com
 *
 * Private Cloud Management Service Daemon
 */
#include <iostream>
#include <string>
#include <wordexp.h>
#include <vector>
#include <sstream>

#include "xclhal2.h"
#include "common.h"

#define MAX_TOKEN_LEN 32

std::string host_ip;
std::string host_port;
std::string mbx_switch;
std::vector<std::string> boards;

/*
 * parse configuration file
 */
int parse_cfg(std::string filename)
{
    std::ifstream file(filename);
    std::string line;
    int i = 0;
    while (std::getline(file, line))
    {
        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string value;
            if (std::getline(is_line, value)) {
                std::cout << "key: " << key << std::endl;
                std::cout << "value: " << value << std::endl;
                if (key == "board") {
                    std::cout << "board[" << i << "]: " << value << std::endl;
                    if (value.size() > MAX_TOKEN_LEN) {
                        std::cout << "board token is too long, please reconfigure, maxlen: " 
                                  << MAX_TOKEN_LEN << std::endl;
                        return -EINVAL;
                    }
                    boards.push_back(value);
                    i++;
                    continue;
                }
                if (key == "ip") {
                    host_ip = value;
                    continue;
                }
                if (key == "port") {
                    host_port = value;
                    continue;
                }
                if (key == "switch") { 
                    mbx_switch = value;
                    continue;
                }
            } else {
                std::cout << "comment: " << key << std::endl;
            }
        }
    }

    if (i == 0) {
        std::cout << "Invalid configuration file -- no device found.\n";
        return -ENODEV;
    }
        
    return i;
}

/*
 * Lookup token and get index
 */
int lookup_board(std::string val)
{
    size_t found = val.find_last_not_of('\0');
    if (found != std::string::npos)
        val.erase(found+1);

    std::vector<std::string>::iterator it = std::find(boards.begin(), boards.end(), val);
    if (it == boards.end()) {
        std::cout << "Device not found.\n";
        return -ENODEV;
    }
    std::cout << "Device found.\n";
    return std::distance(boards.begin(), it);
}

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
            perror("server accept failed...");
            continue;
        } else {
            printf("server accept the client...\n");
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

    if (parse_cfg(config_path) <= 0)
        return -EINVAL;

    // Write to config_mailbox_comm_id in format "127.0.0.1,12345,abc123", where 'abc123' is the cloud token
    unsigned numDevs;
    for (numDevs = 0; numDevs < boards.size(); numDevs++) {
        if (xclMailboxMgmtPutID(numDevs, std::string(host_ip+","+host_port+","+boards.at(numDevs)+";").c_str(), mbx_switch.c_str())) {
            std::cout << "xclMailboxMgmtPutID(): " << errno << std::endl;
            return errno;
        }
    }

    // blocks waiting for connection, then forks child process from here
    msd_comm_init(&comm_fd); 

    // handshake with MPD to identify device
    std::string cloud_token;
    uint32_t dataLength;
    recv(comm_fd, &dataLength, sizeof(uint32_t), 0); // Receive the message length
    dataLength = ntohl(dataLength);                  // Ensure host system byte order
    std::vector<char> buf(MAX_TOKEN_LEN, '\0');      // Allocate a receive buffer
    recv(comm_fd, &(buf[0]), dataLength, 0);         // Receive the string data
    cloud_token.append( buf.cbegin(), buf.cend() );

    std::cout << "cloud token received: " << cloud_token << std::endl;

    int devIdx = lookup_board(cloud_token);
    if (devIdx < 0)
        return devIdx;

    std::cout << "device index of token: " << devIdx << std::endl;

    local_fd = xclMailboxMgmt(devIdx);
    if (local_fd < 0) {
        std::cout << "xclMailboxMgmt(): " << errno << std::endl;
        return errno;
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

