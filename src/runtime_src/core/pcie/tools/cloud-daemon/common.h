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
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include "pciefunc.h"
#include "sw_msg.h"
#include "core/pcie/driver/linux/include/mailbox_proto.h"

// Callback function for processing SW channel msg. The original msg
// is passed in. The output would be a new msg ready to pass to either
// local mailbox or remote socket for further handling. The return value
// will indicate where to pass.
using msgHandler = int(*)(const pcieFunc& dev, std::unique_ptr<sw_msg>&,
    std::unique_ptr<sw_msg>&);
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
    std::unique_ptr<sw_msg> data;
    enum MSG_TYPE type;
};

struct Msgq {
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<queue_msg> q;
};

std::string str_trim(const std::string &str);
int splitLine(const std::string &line, std::string& key,
    std::string& value, const std::string& delim = "=");
int waitForMsg(const pcieFunc& dev, int localfd, int remotefd, long interval,
    int retfd[2]);
std::unique_ptr<sw_msg> getLocalMsg(const pcieFunc& dev, int localfd);
std::unique_ptr<sw_msg> getRemoteMsg(const pcieFunc& dev, int remotefd);
int handleMsg(const pcieFunc& dev, queue_msg &msg);
size_t getSockMsgSize(const pcieFunc& dev, int sockfd);
size_t getMailboxMsgSize(const pcieFunc& dev, int mbxfd);
bool readMsg(const pcieFunc& dev, int fd, sw_msg *swmsg);
bool sendMsg(const pcieFunc& dev, int fd, sw_msg *swmsg);

class Common;

class Common
{
public:
    Common(const std::string &name, const std::string &plugin_path, bool for_user);
    ~Common();
    void *plugin_handle;
    size_t total;
    void preStart();
    void postStop();
    virtual void start() = 0;
    virtual void run() = 0;
    virtual void stop() = 0;

private:
    std::string name;
    std::string plugin_path;
};

class Sw_mb_container
{
public:
    Sw_mb_container(size_t respLen, uint64_t respID);
    ~Sw_mb_container();
    std::unique_ptr<sw_msg> get_response();
    char* get_payload_buf();
    void set_hook(std::function<void()> hook);
private:
    std::unique_ptr<sw_msg> processed_;
    std::function<void()> hook_;
};
#endif // COMMON_H
