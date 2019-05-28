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
 * Xilinx Management Service Daemon (MSD) for cloud.
 */

#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <fstream>
#include <vector>
#include <thread>
#include <cstdlib>
#include <csignal>

#include "pciefunc.h"
#include "common.h"

static const std::string configFile("/etc/msd.conf");
bool quit = false;
// We'd like to only handle below request through daemons.
uint64_t chanSwitch = (1UL<<MAILBOX_REQ_TEST_READY) |
                      (1UL<<MAILBOX_REQ_TEST_READ)  |
                      (1UL<<MAILBOX_REQ_LOAD_XCLBIN);

// Get host configured in config file
static std::string getHost()
{
    std::ifstream cfile(configFile);
    if (!cfile.good()) {
        syslog(LOG_ERR, "failed to open config file: %s", configFile.c_str());
        return "";
    }

    for (std::string line; std::getline(cfile, line);) {
        std::string key, value;
        int ret = splitLine(line, key, value);
        if (ret != 0)
            break;
        if (key.compare("host") == 0)
            return value;
    }

    syslog(LOG_ERR, "failed to read hostname from: %s", configFile.c_str());
    return "";
}

static void createSocket(pcieFunc& dev, int& sockfd, uint16_t& port)
{
    struct sockaddr_in saddr = { 0 };

    // A non-block socket would allow us to quit gracefully.
    sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sockfd < 0) {
        dev.log(LOG_ERR, "failed to create socket: %m");
        return;
    }

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(port);
    if ((bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr))) < 0) {
        dev.log(LOG_ERR, "failed to bind socket: %m");
        close(sockfd);
        return;
    }

    int backlog = 50; // shouldn't have more than 50 boards in one host
    if ((listen(sockfd, backlog)) != 0) {
        dev.log(LOG_ERR, "failed to listen: %m");
        close(sockfd);
        return;
    }

    socklen_t slen = sizeof(saddr);
    if (getsockname(sockfd, (struct sockaddr *)&saddr, &slen) < 0) {
        dev.log(LOG_ERR, "failed to obtain port: %m");
        close(sockfd);
        return;
    }
    port = ntohs(saddr.sin_port); // Retrieve allocated port by kernel
}

static int verifyMpd(pcieFunc& dev, int mpdfd, int id)
{
    int mpdid;

    if (read(mpdfd, &mpdid, sizeof(mpdid)) != sizeof(mpdid)) {
        dev.log(LOG_ERR, "short read mpd id");
        return -EINVAL;
    }

    mpdid = ntohl(mpdid);
    if (mpdid != id) {
        dev.log(LOG_ERR, "bad mpd id: 0x%x", mpdid);
        return -EINVAL;
    }

    int ret = 0;
    if (write(mpdfd, &ret, sizeof(ret)) != sizeof(ret)) {
        dev.log(LOG_ERR, "failed to send reply to identification, %m");
        return -EINVAL;
    }

    return 0;
}

static int connectMpd(pcieFunc& dev, int sockfd, int id, int& mpdfd)
{
    struct sockaddr_in mpdaddr = { 0 };

    mpdfd = -1;

    socklen_t len = sizeof(mpdaddr);
    mpdfd = accept(sockfd, (struct sockaddr *)&mpdaddr, &len);
    if (mpdfd < 0) {
        if (errno != EWOULDBLOCK)
            dev.log(LOG_ERR, "failed to accept, %m");
        return -errno;
    }

    if (verifyMpd(dev, mpdfd, id) != 0) {
        dev.log(LOG_ERR, "failed to verify mpd");
        close(mpdfd);
        mpdfd = -1;
        return -EINVAL;
    }

    dev.log(LOG_INFO, "successfully connected to mpd");
    return 0;
}

// Server serving MPD. Any error from socket fd, re-accept, don't quit.
// Will quit on any error from local mailbox fd.
static void msd(std::shared_ptr<pcidev::pci_device> d, std::string host)
{
    uint16_t port;
    int sockfd = -1, mpdfd = -1, mbxfd = -1;
    int ret;

    pcieFunc dev(d);

    mbxfd = dev.getMailbox();
    if (mbxfd == -1)
        goto done;

    // Create socket and obtain port.
    port = dev.getPort();
    createSocket(dev, sockfd, port);
    if (sockfd < 0 || port == 0)
        goto done;

    // Update config, if the existing one is not the same.
    (void) dev.loadConf();
    if (host != dev.getHost() || port != dev.getPort() ||
        chanSwitch != dev.getSwitch()) {
        if (dev.updateConf(host, port, chanSwitch) != 0)
            goto done;
    }

    while (!quit) {
        // Connect to mpd.
        if (mpdfd == -1) {
            ret = connectMpd(dev, sockfd, dev.getId(), mpdfd);
            if (ret == -EWOULDBLOCK) {
                sleep(1);
                mpdfd = -1;
                continue; // MPD is not ready yet, retry.
            } else if (ret != 0) {
                break;
            }
        }

        // Waiting for msg to show up, interval is 3 seconds.
        ret = waitForMsg(dev, mbxfd, mpdfd, 3);
        if (ret < 0) {
            if (ret == -EAGAIN) // MPD has been quiet, retry.
                continue;
            else
                break;
        }

        // Process msg.
        if (ret == mbxfd)
            ret = localToRemote(dev, mbxfd, mpdfd);
        else
            ret = remoteToLocal(dev, mbxfd, mpdfd);
        if (ret == -EAGAIN) { // Socket connection was lost, retry
            close(mpdfd);
            mpdfd = -1;
            continue;
        } else if (ret != 0) {
            break;
        }
    }

done:
    dev.updateConf("", 0, 0); // Restore default config.
    close(mpdfd);
    close(sockfd);
}

void signalHandler(int signum)
{
    syslog(LOG_INFO, "Caught SIGTERM! Leaving!");
    quit = true;
}

int main(void)
{
    // Daemon has no connection to terminal.
    fcloseall();

    // Start logging ASAP.
    openlog("msd", LOG_PID|LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "started");

    // Fetching host name from config file.
    std::string host = getHost();
    if (host.empty())
        return 0;

    // Handle sigterm from systemd to shutdown gracefully.
    signal(SIGTERM, signalHandler);

    // Fire up one thread for each board.
    auto total = pcidev::get_dev_total(false);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < total; i++)
        threads.emplace_back(msd, pcidev::get_dev(i, false), host);

    // Wait for all threads to finish before quit.
    for (auto& t : threads)
        t.join();

    syslog(LOG_INFO, "ended");
    closelog();         
    return 0;
}
