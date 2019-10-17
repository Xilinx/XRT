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

#include <syslog.h>
#include <unistd.h>
#include <stdarg.h>
#include <sstream>
#include <cstring>
#include <random>
#include "pciefunc.h"
#include "common.h"

std::string pcieFunc::getHost()
{
    std::lock_guard<std::mutex> l(lock);
    return host;
}

uint16_t pcieFunc::getPort()
{
    std::lock_guard<std::mutex> l(lock);
    return port;
}

int pcieFunc::getId()
{
    std::lock_guard<std::mutex> l(lock);
    return devId;
}

int pcieFunc::getMailbox()
{
    std::lock_guard<std::mutex> l(lock);

    if (mbxfd == -1)
        mbxfd = mailboxOpen();
    return mbxfd;
}

uint64_t pcieFunc::getSwitch()
{
    std::lock_guard<std::mutex> l(lock);
    return chanSwitch;
}

int pcieFunc::getIndex() const
{
    return index;
}

std::shared_ptr<pcidev::pci_device> pcieFunc::getDev() const
{
    return dev;
}

bool pcieFunc::validConf()
{
    return (!host.empty() && port && devId);
}

void pcieFunc::clearConf()
{
    host.clear();
    port = 0;
    devId = 0;
    chanSwitch = 0;
}

bool pcieFunc::loadConf()
{
    std::lock_guard<std::mutex> l(lock);
    std::vector<std::string> config;
    std::string err;

    dev->sysfs_get("", "config_mailbox_channel_switch", err, chanSwitch);
    if (!err.empty()) {
        log(LOG_ERR, "failed to get channel switch: %s", err.c_str());
        return false;
    }

    // Config is a string consists of name-value pairs separated by '\n'
    // which can be retrieved as an array of multiple strings
    dev->sysfs_get("", "config_mailbox_comm_id", err, config);
    if (!err.empty()) {
        log(LOG_ERR, "failed to obtain config: %s", err.c_str());
        return false;
    }

    for (auto s : config) {
        const char *sp = s.c_str();
        if (sp[0] == '\0')
            continue;
        std::string key, value;
        if (splitLine(s, key, value) != 0) {
            log(LOG_WARNING, "bad config line %s", s.c_str());
            continue;
        }

        if (key.compare("host") == 0)
            host = value;
        else if (key.compare("port") == 0)
            port = stoi(value, nullptr, 0);
        else if (key.compare("id") == 0)
            devId = stol(value, nullptr, 0);
        else // ignore unknown key, but don't fail
            log(LOG_WARNING, "unknown config key %s", key.c_str());
    }

    if (!validConf()) {
        // Make sure config stays in known state on error.
        clearConf();
        log(LOG_ERR, "no config found");
    } else {
        log(LOG_INFO, "config switch=0x%llx, host=%s, port=%d, id=0x%x",
            chanSwitch, host.c_str(), port, devId);
    }

    return validConf();
}

void pcieFunc::log(int priority, const char *format, ...) const
{
    va_list args;
    std::ostringstream ss;

    va_start(args, format);

    ss << std::hex << "[" << dev->domain << ":" <<
        dev->bus << ":" << dev->dev << "." << dev->func << "] ";

    vsyslog(priority, (ss.str() + format).c_str(), args);

    va_end(args);
}

pcieFunc::pcieFunc(size_t index, bool user) : index(index)
{
    dev = pcidev::get_dev(index, user);
}

pcieFunc::~pcieFunc()
{
    clearConf();
    dev->close(mbxfd);
    mbxfd = -1;
}

int pcieFunc::updateConf(std::string hostname, uint16_t hostport, uint64_t swch)
{
    std::lock_guard<std::mutex> l(lock);
    std::string config;
    std::string err;
    std::random_device rd;
    std::mt19937 gen(rd());
    int id = gen();

    config += "host=" + hostname + "\n";
    config += "port=" + std::to_string(hostport) + "\n";
    std::stringstream ss;
    ss << std::hex << id;
    config += "id=0x" + ss.str();
    dev->sysfs_put("", "config_mailbox_comm_id", err, config);
    if (!err.empty()) {
        log(LOG_ERR, "failed to push config: %s", err.c_str());
        return -EINVAL;
    }

    dev->sysfs_put("", "config_mailbox_channel_switch", err,
        std::to_string(swch));
    if (!err.empty()) {
        log(LOG_ERR, "failed to push channel switch: %s", err.c_str());
        return -EINVAL;
    }

    host = hostname;
    port = hostport;
    devId = id;
    chanSwitch = swch;
    log(LOG_INFO, "pushed switch: 0x%llx, config: %s", swch, config.c_str());
    return 0;
}

int pcieFunc::mailboxOpen()
{
    const int fd = dev->open("mailbox", O_RDWR);
    if (fd == -1) {
        log(LOG_ERR, "failed to open mailbox: %m");
        return -1;
    }

    return fd;
}
