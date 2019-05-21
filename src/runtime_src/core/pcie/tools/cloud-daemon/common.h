#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <sys/wait.h>
#include <cassert>
#include <errno.h>
#include "core/pcie/driver/linux/include/mailbox_proto.h"

#define INIT_BUF_SIZE 8

#define SA struct sockaddr

/*
 * Worker thread that is common between msd and mpd.
 */
static void mailbox_daemon(int local_fd, int comm_fd, std::string name)
{
    char init_buf[sizeof(struct sw_chan) + INIT_BUF_SIZE];
    struct sw_chan *buf = (struct sw_chan *)init_buf;
    int ret = 0;

    std::cout << name << ": started\n";

    fd_set rfds;
    FD_ZERO(&rfds);
#define max(a,b) (a>b?a:b)

    buf->sz = INIT_BUF_SIZE;
    for (;;) {
        size_t alloc_buf_sz = sizeof(struct sw_chan) + buf->sz;

        FD_SET(local_fd, &rfds);
        FD_SET(comm_fd, &rfds);
        ret = select((max(comm_fd, local_fd) + 1), &rfds, NULL, NULL, NULL);

        if (ret == -1) {
            break;
        }

        bool singleshot = true; // ensure we only read from local_fd or comm_fd in each pass

        if (FD_ISSET(local_fd, &rfds) && singleshot) {
            /* local read */
            static int tx_count = 0;
            std::cout << name << ": local read start\n";
            std::cout << name << ": reading with buf size: " << alloc_buf_sz << std::endl;

            ret = read(local_fd, (void *)buf, alloc_buf_sz);
            if (ret <= 0) {
                // sw channel xfer error
                if (errno != EMSGSIZE) {
                    std::cout << name << ": read failed: " << errno << " ("
                              << strerror(errno) << ")\n";
                    exit(errno);
                }

                size_t new_payload_sz = buf->sz; // store size

                std::cout << name << ": read failed: need bigger buffer: "
                          << sizeof(struct sw_chan) + new_payload_sz << std::endl;

                // Sanity checks
                assert(new_payload_sz > INIT_BUF_SIZE);
                assert(buf == (struct sw_chan *)init_buf);

                // Alloc a big enough buffer
                buf = (struct sw_chan *)malloc(sizeof(struct sw_chan) + new_payload_sz);
                buf->sz = new_payload_sz;
                assert(buf != NULL);

                alloc_buf_sz = sizeof(struct sw_chan) + buf->sz;

                ret = read(local_fd, (void *)buf, alloc_buf_sz);
                if (ret < 0) {
                    std::cout << name << ": read failed: " << errno << " ("
                              << strerror(errno) << ")\n";
                    exit(errno);
                }
            }

            std::cout << name << ": local read complete: " << ret << " bytes\n";

            /* comm write */
            std::cout << name << ": comm write start\n";
            size_t write_sz = ret;
            unsigned char *pbuf = reinterpret_cast<unsigned char*>(buf);

            while (write_sz > 0) {
                int num = write(comm_fd, pbuf, write_sz);
                pbuf += num;
                write_sz -= num;
            }

            std::cout << name << ": comm write complete: " << ret << " bytes\n";

            tx_count++;
            std::cout << name << ": TX: " << tx_count << " msg delivered\n";

            singleshot = false;
        }

        if (FD_ISSET(comm_fd, &rfds) && singleshot) {
            std::cout << name << ": comm read start\n";
            static int rx_count = 0;
            /* comm read */
            int num = 0;
            num = recv(comm_fd, (void *)buf, sizeof(struct sw_chan), MSG_PEEK);

            std::cout << name << ": recv(MSG_PEEK): " << num << ", sz: " << buf->sz << std::endl;
            size_t new_payload_sz = buf->sz;

            if ((sizeof(struct sw_chan) + buf->sz) > alloc_buf_sz) {
                std::cout << name << ": read failed: need bigger buffer: "
                          << sizeof(struct sw_chan) + new_payload_sz << std::endl;

                // Sanity checks
                assert(new_payload_sz > INIT_BUF_SIZE);
                assert(buf == (struct sw_chan *)init_buf);

                // Alloc a big enough buffer
                buf = (struct sw_chan *)malloc(sizeof(struct sw_chan) + new_payload_sz);
                buf->sz = new_payload_sz;
                alloc_buf_sz = sizeof(struct sw_chan) + buf->sz;
                assert(buf != NULL);
            }

            unsigned char *pbuf = reinterpret_cast<unsigned char*>(buf);

            int remaining_buflen = (sizeof(struct sw_chan) + new_payload_sz);
            unsigned total_read = 0;
            while (remaining_buflen > 0) {
                num = read(comm_fd, pbuf, remaining_buflen);

                if (num == 0)
                    exit(1);

                if (num < 0) {
                    std::cout << "Error, failed to read: " << num << ", errno: " << errno << " errstr: " << strerror(errno) << std::endl;
                    exit(num);
                }
                pbuf += num;
                remaining_buflen -= num;
                std::cout << name << ": remaining_buflen=" << remaining_buflen << ", num=" << num << std::endl;
                total_read += num;
            }

            std::cout << name << ": comm read complete: " << total_read << " bytes\n";

            /* local write */
            std::cout << name << ": local write start\n";

            assert(total_read == (sizeof(struct sw_chan) + buf->sz));

            ret = write(local_fd, (void *)buf, total_read);
            std::cout << name << ": write returns: " << ret << std::endl;
            if (ret != (int)total_read) {
                if (ret < 0) {
                    std::cout << name << ": write failed: " << errno << " ("
                              << strerror(errno) << ")\n";
                } else {
                    std::cout << name << ": write failed: short write: "
                              << errno << " (" << strerror(errno) << ")" << std::endl;
                }
                exit(errno);
            }
            std::cout << name << ": local write complete: " << ret << " bytes\n";

            rx_count++;
            std::cout << name << ": RX: " << rx_count << " msg delivered\n";

            singleshot = false;
        }

        // Free the larger buffer if alloc'd and use original buffer
        if (buf != (struct sw_chan *)init_buf) {
            std::cout << name << ": freeing buffer of size : "
                      << alloc_buf_sz << " bytes\n";
            free(buf);
            buf = (struct sw_chan *)init_buf;
        }
        buf->sz = INIT_BUF_SIZE;
    }

    std::cout << name << ": ended\n";
}
