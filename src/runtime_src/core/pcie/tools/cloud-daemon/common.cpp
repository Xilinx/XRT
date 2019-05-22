/**
 * Copyright (C) 2019 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

/*
 * In this file, we provide helper functions for all daemons.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>
#include <strings.h>
#include <algorithm>

#include "common.h"

/* Parse name value pair in format as "key=value". */
int splitLine(std::string line, std::string& key, std::string& value)
{
    auto pos = line.find('=', 0);
    if (pos == std::string::npos)
        return -EINVAL;

    key = line.substr(0, pos);
    value = line.substr(pos + 1);
    return 0;
}

/* Allocate a zero-initialized sw channel msg for playloadSize msg. */
sw_chan *allocmsg(pcieFunc& dev, size_t payloadSize)
{
    size_t total = sizeof(sw_chan) + payloadSize;
    void *p = malloc(total);

    if (p == nullptr) {
        dev.log(LOG_ERR, "failed to alloc msg, size=%d", payloadSize);
        return nullptr;
    }

    bzero(p, total);
    sw_chan *sc = (sw_chan *)p;
    sc->sz = payloadSize;
    dev.log(LOG_INFO, "alloc'ed msg (%d + %d = %d bytes): %p",
        sizeof(sw_chan), payloadSize, total, sc);
    return sc;
}

/* Free a sw channel msg allocated by allocmsg(). */
void freemsg(pcieFunc& dev, sw_chan *msg)
{
    free(msg);
    dev.log(LOG_INFO, "freed msg: %p", msg);
}

/* Retrieve size for the next msg from socket fd. */
size_t getSockMsgSize(pcieFunc& dev, int sockfd)
{
    sw_chan sc = { 0 };

    if (recv(sockfd, (void *)&sc, sizeof(sc), MSG_PEEK) != sizeof(sc)) {
        dev.log(LOG_ERR, "can't receive sw_chan from socket, %m");
        return 0;
    }

    dev.log(LOG_INFO, "retrieved msg size from socket: %d bytes", sc.sz);
    return sc.sz;
}

/* Retrieve size for the next msg from mailbox fd. */
size_t getMailboxMsgSize(pcieFunc& dev, int mbxfd)
{
    sw_chan sc = { 0 };

    // This read is expected to fail w/ errno == EMSGSIZE
    if (read(mbxfd, (void *)&sc, sizeof(sc)) >= 0 || errno != EMSGSIZE) {
        dev.log(LOG_ERR, "can't read sw_chan from mailbox, %m");
        return 0;
    }

    dev.log(LOG_INFO, "retrieved msg size from mailbox: %d bytes", sc.sz);
    return sc.sz;
}

/* Read a sw channel msg from fd (can be a socket or mailbox one). */
bool readMsg(pcieFunc& dev, int fd, sw_chan *sc)
{
    ssize_t total = sizeof(*sc) + sc->sz;
    ssize_t cur = 0;
    char *buf = reinterpret_cast<char *>(sc);

    while (cur < total) {
        ssize_t ret = read(fd, buf + cur, total - cur);
        if (ret <= 0)
            break;
        cur += ret;
    }

    dev.log(LOG_INFO, "read %d bytes out of %d bytes from fd %d",
        cur, total, fd);
    return (cur == total);
}

/* Write a sw channel msg to fd (can be a socket or mailbox one). */
bool sendMsg(pcieFunc& dev, int fd, sw_chan *sc)
{
    ssize_t total = sizeof(*sc) + sc->sz;
    ssize_t cur = 0;
    char *buf = reinterpret_cast<char *>(sc);

    while (cur < total) {
        ssize_t ret = write(fd, buf + cur, total - cur);
        if (ret <= 0)
            break;
        cur += ret;
    }

    dev.log(LOG_INFO, "write %d bytes out of %d bytes to fd %d",
        cur, total, fd);
    return (cur == total);
}

/*
 * Wait for incoming msg from either socket or mailbox fd.
 * The fd with incoming msg is returned.
 */
int waitForMsg(pcieFunc& dev, int localfd, int remotefd, long interval)
{
    fd_set fds;
    int retfd = -1;
    int ret = 0;
    struct timeval timeout = { interval, 0 };

    FD_ZERO(&fds);
    if (localfd >= 0)
        FD_SET(localfd, &fds);
    if (remotefd >= 0)
        FD_SET(remotefd, &fds);

    if (interval == 0) {
        ret = select(std::max(localfd, remotefd) + 1, &fds, NULL, NULL, NULL);
    } else {
        ret = select(std::max(localfd, remotefd) + 1, &fds,
            NULL, NULL, &timeout);
    }

    if (ret == -1) {
        dev.log(LOG_ERR, "failed to select: %m");
        return -EINVAL; // failed
    }
    if (ret == 0)
        return -EAGAIN; // time'd tout

    if (FD_ISSET(localfd, &fds)) {
        retfd = localfd;
        dev.log(LOG_INFO, "msg arrived on mailbox fd %d", retfd);
    } else {
        retfd = remotefd;
        dev.log(LOG_INFO, "msg arrived on remote fd %d", retfd);
    }
    return retfd;
}

/*
 * Fetch sw channel msg from local mailbox fd, process it by passing it through
 * to socket fd.
 */
int localToRemote(pcieFunc& dev, int localfd, int remotefd)
{
    int ret = -EINVAL;
    sw_chan *sc = nullptr;

    size_t msgsz = getMailboxMsgSize(dev, localfd);
    if (msgsz == 0)
        goto done;

    sc = allocmsg(dev, msgsz);
    if (sc == nullptr)
        goto done;

    if (!readMsg(dev, localfd, sc))
        goto done;

    if (!sendMsg(dev, remotefd, sc)) {
        ret = -EAGAIN;
        goto done;
    }

    ret = 0;

done:
    freemsg(dev, sc);
    return ret;
}

/*
 * Fetch sw channel msg from remote socket fd, process it by passing it through
 * to local mailbox fd.
 */
int remoteToLocal(pcieFunc& dev, int localfd, int remotefd)
{
    int ret = -EINVAL;
    sw_chan *sc = nullptr;

    size_t msgsz = getSockMsgSize(dev, remotefd);
    if (msgsz == 0) {
        ret = -EAGAIN;
        goto done;
    }

    if (msgsz > 1024 * 1024 * 1024)
        goto done;

    sc = allocmsg(dev, msgsz);
    if (sc == nullptr)
        goto done;

    if (!readMsg(dev, remotefd, sc)) {
        ret = -EAGAIN;
        goto done;
    }

    if (!sendMsg(dev, localfd, sc))
        goto done;

    ret = 0;

done:
    freemsg(dev, sc);
    return ret;
}
