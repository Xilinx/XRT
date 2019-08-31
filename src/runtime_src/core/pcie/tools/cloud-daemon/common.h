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

/* Declaring interfaces to helper functions for all daemons. */

#ifndef COMMON_H
#define COMMON_H

#include <string>
#include "pciefunc.h"
#include "sw_msg.h"
#include "core/pcie/driver/linux/include/mailbox_proto.h"

// Callback function for processing SW channel msg. The original msg
// is passed in. The output would be a new msg ready to pass to either
// local mailbox or remote socket for further handling. The return value
// will indicate where to pass.
using msgHandler = int(*)(pcieFunc& dev, std::shared_ptr<sw_msg>&,
    std::shared_ptr<sw_msg>&);
#define FOR_REMOTE 0
#define FOR_LOCAL  1

enum MSG_TYPE {
    LOCAL_MSG = 0,
    REMOTE_MSG,
    ILLEGAL_MSG,
};

struct queue_msg {
    int localFd;
    int remoteFd;
    msgHandler cb;
    std::shared_ptr<sw_msg> data;
    enum MSG_TYPE type;
};

std::string str_trim(const std::string &str);
int splitLine(std::string line, std::string& key, std::string& value,
       const std::string& delim = "=");
sw_chan *allocmsg(pcieFunc& dev, size_t payloadSize);
void freemsg(sw_chan *msg);
size_t getSockMsgSize(pcieFunc& dev, int sockfd);
size_t getMailboxMsgSize(pcieFunc& dev, int mbxfd);
int readMsg(pcieFunc& dev, int fd, sw_chan *sc);
int sendMsg(pcieFunc& dev, int fd, sw_chan *sc);
int waitForMsg(pcieFunc& dev, int localfd, int remotefd, long interval, int retfd[]);
std::shared_ptr<sw_msg> getLocalMsg(pcieFunc& dev, int localfd);
std::shared_ptr<sw_msg> getRemoteMsg(pcieFunc& dev, int remotefd);
int handleMsg(pcieFunc& dev, struct queue_msg& msg);

#endif    // COMMON_H
