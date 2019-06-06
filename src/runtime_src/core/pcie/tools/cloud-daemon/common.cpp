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
#include "sw_msg.h"

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

/* Retrieve size for the next msg from socket fd. */
size_t getSockMsgSize(pcieFunc& dev, int sockfd)
{
    std::shared_ptr<sw_msg> swmsg = std::make_shared<sw_msg>(0);

    if (recv(sockfd, swmsg->data(), swmsg->size(), MSG_PEEK) !=
        static_cast<ssize_t>(swmsg->size())) {
        dev.log(LOG_ERR, "can't receive sw_chan from socket, %m");
        return 0;
    }

    dev.log(LOG_INFO, "retrieved msg size from socket: %d bytes",
        swmsg->payloadSize());
    return swmsg->payloadSize();
}

/* Retrieve size for the next msg from mailbox fd. */
size_t getMailboxMsgSize(pcieFunc& dev, int mbxfd)
{
    std::shared_ptr<sw_msg> swmsg = std::make_shared<sw_msg>(0);

    // This read is expected to fail w/ errno == EMSGSIZE
    // However, the real msg size should be filled out by driver.
    if (read(mbxfd, swmsg->data(), swmsg->size()) >= 0 || errno != EMSGSIZE) {
        dev.log(LOG_ERR, "can't read sw_chan from mailbox, %m");
        return 0;
    }

    dev.log(LOG_INFO, "retrieved msg size from mailbox: %d bytes",
        swmsg->payloadSize());
    return swmsg->payloadSize();
}

/* Read a sw channel msg from fd (can be a socket or mailbox one). */
bool readMsg(pcieFunc& dev, int fd, sw_msg *swmsg)
{
    ssize_t total = swmsg->size();
    ssize_t cur = 0;
    char *buf = swmsg->data();

    while (cur < total) {
        ssize_t ret = read(fd, buf + cur, total - cur);
        if (ret <= 0)
            break;
        cur += ret;
    }

    dev.log(LOG_INFO, "read %d bytes out of %d bytes from fd %d, valid: %d",
        cur, total, fd, swmsg->valid());
    return (cur == total && swmsg->valid());
}

/* Write a sw channel msg to fd (can be a socket or mailbox one). */
bool sendMsg(pcieFunc& dev, int fd, sw_msg *swmsg)
{
    ssize_t total = swmsg->size();
    ssize_t cur = 0;
    char *buf = swmsg->data();

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
 * to socket fd or by the callback.
 */
int processLocalMsg(pcieFunc& dev, int localfd, int remotefd, msgHandler cb)
{
    size_t msgsz = getMailboxMsgSize(dev, localfd);
    if (msgsz == 0)
        return -EINVAL;

    std::shared_ptr<sw_msg> swmsg = std::make_shared<sw_msg>(msgsz);
    if (swmsg == nullptr)
        return -ENOMEM;

    if (!readMsg(dev, localfd, swmsg.get()))
        return -EINVAL;

    int pass;
    std::shared_ptr<sw_msg> swmsgProcessed;
    if (!cb) {
        // Continue passing received msg to local mailbox.
        swmsgProcessed = swmsg;
        pass = PROCESSED_FOR_REMOTE;
    } else {
        pass = (*cb)(dev, swmsg, swmsgProcessed);
    }

    bool sent;
    if (pass == PROCESSED_FOR_LOCAL)
        sent = sendMsg(dev, localfd, swmsgProcessed.get());
    else if (pass == PROCESSED_FOR_REMOTE)
        sent = sendMsg(dev, remotefd, swmsgProcessed.get());
    else // Error occured
        return pass;

    return sent ? 0 : -EINVAL;
}

/*
 * Fetch sw channel msg from remote socket fd, process it by passing it through
 * to local mailbox fd or by the callback.
 */
int processRemoteMsg(pcieFunc& dev, int localfd, int remotefd, msgHandler cb)
{
    size_t msgsz = getSockMsgSize(dev, remotefd);
    if (msgsz == 0)
        return -EAGAIN;

    if (msgsz > 1024 * 1024 * 1024)
        return -EMSGSIZE;

    std::shared_ptr<sw_msg> swmsg = std::make_shared<sw_msg>(msgsz);
    if (swmsg == nullptr)
        return -ENOMEM;

    if (!readMsg(dev, remotefd, swmsg.get()))
        return -EAGAIN;

    int pass;
    std::shared_ptr<sw_msg> swmsgProcessed;
    if (!cb) {
        // Continue passing received msg to local mailbox.
        swmsgProcessed = swmsg;
        pass = PROCESSED_FOR_LOCAL;
    } else {
        pass = (*cb)(dev, swmsg, swmsgProcessed);
    }

    bool sent;
    if (pass == PROCESSED_FOR_LOCAL)
        sent = sendMsg(dev, localfd, swmsgProcessed.get());
    else if (pass == PROCESSED_FOR_REMOTE)
        sent = sendMsg(dev, remotefd, swmsgProcessed.get());
    else // Error occured
        return pass;

    return sent ? 0 : -EINVAL;
}
