#include <unistd.h>
#include <iostream>
#include <thread>
#include <cassert>
#include <cstring>
#include <climits>

#include "mailbox_proto.h"
#include "xclhal2.h"

#define INIT_BUF_SIZE   8

void mailbox_daemon(int src_fd, int tgt_fd, std::string name)
{
    char init_buf[sizeof (struct sw_chan) + INIT_BUF_SIZE];
    struct sw_chan *buf = (struct sw_chan *)init_buf;
    int ret = 0;
    int xferCount = 0;

    std::cout << name << ": started" << std::endl;

    buf->sz = INIT_BUF_SIZE;
    for( ;; ) {
        size_t bufsz = sizeof(struct sw_chan) + buf->sz;

        // Retrieve msg for peer.
        std::cout << name << ": reading with buf size: " << bufsz << std::endl;

        ret = read(src_fd, (void *)buf, bufsz);
        if (ret == 0) {
            // Race with another thread?
            std::cout << name << ": read failed: empty msg" << std::endl;
            break;
        } else if (ret < 0) {
            if(errno == EMSGSIZE) {
                size_t newsz = buf->sz;

                std::cout << name << ": read failed: need bigger buffer: "
                    << sizeof(struct sw_chan) + newsz << std::endl;

                assert(newsz > INIT_BUF_SIZE);
                assert(buf == (struct sw_chan *)init_buf);

                // Alloc a big enough buffer
                buf = (struct sw_chan *)malloc(sizeof(struct sw_chan) + newsz);
                buf->sz = newsz;
                assert(buf != NULL);
                continue;
            } else {
                std::cout << name << ": read failed: " << errno << " ("
                    << std::strerror(errno) << ")" << std::endl;
                break;
            }
        }
        std::cout << name << ": read OK: " << ret << " bytes" << std::endl;

        // Successfully got a msg, pass through to peer
        bufsz = sizeof(struct sw_chan) + buf->sz;
        std::cout << name << ": writing with buf size: " << bufsz << std::endl;

        ret = write(tgt_fd, (void *)buf, bufsz);
        if (ret != bufsz) {
            if (ret < 0) {
                std::cout << name << ": write failed: " << errno << " ("
                    << std::strerror(errno) << ")" << std::endl;
            } else {
                std::cout << name << ": write failed: short write: "
                    << ret << std::endl;
            }
            break;
        }
        std::cout << name << ": write OK: " << ret << " bytes" << std::endl;

        xferCount++;
        std::cout << name << ": " << xferCount << " msg delivered" << std::endl;

        if (buf != (struct sw_chan *)init_buf) {
            free(buf);
            buf = (struct sw_chan *)init_buf;
        }
        buf->sz = INIT_BUF_SIZE;
    }

    std::cout << name << ": ended" << std::endl;
}

int str2index(const char *arg, unsigned& index)
{
    std::string devStr(arg);
    unsigned long i;
    char *endptr;

    i = std::strtoul(arg, &endptr, 0);
    if (*endptr != '\0' || i >= UINT_MAX) {
        std::cout << "ERROR: " << devStr << " is not a valid card index."
            << std::endl;
        return -EINVAL;
    }
    index = i;

    return 0;
}

int main(int argc, char *argv[])
{
    int user_fd, mgmt_fd;
    unsigned int idx = 0;
    int c;
    std::string usage("Options: -d <index>");

    while ((c = getopt(argc, argv, "d:")) != -1)
    {
        switch (c) {
        case 'd':
            if (str2index(optarg, idx) != 0)
                return -EINVAL;
            break;

        default:
            std::cout << usage << std::endl;
            return -EINVAL;
        }
    }

    std::cout << "Launching SW mailbox daemon on board " << idx << std::endl;

    user_fd = xclMailbox(idx);
    mgmt_fd = xclMailboxMgmt(idx);
    if (user_fd < 0 || mgmt_fd < 0) {
        std::cout << "Can't open mailbox for board " << idx << std::endl;
        return -EINVAL;
    }

    std::thread mpd(mailbox_daemon, user_fd, mgmt_fd, "[MPD]");
    std::thread msd(mailbox_daemon, mgmt_fd, user_fd, "[MSD]");

    mpd.join();
    msd.join();

    return 0;
}

