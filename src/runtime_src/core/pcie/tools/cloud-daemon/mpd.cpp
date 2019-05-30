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
 * Xilinx Management Proxy Daemon (MPD) for cloud.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <fstream>
#include <vector>
#include <thread>
#include <cstdlib>

#include "pciefunc.h"
#include "common.h"

std::string getIP(std::string host)
{
    struct hostent *hp = gethostbyname(host.c_str());

    if (hp == NULL)
        return "";

    char dst[INET_ADDRSTRLEN + 1] = { 0 };
    const char *d = inet_ntop(AF_INET, (struct in_addr *)(hp->h_addr),
        dst, sizeof(dst));
    return d;
}

static int connectMsd(pcieFunc& dev, std::string ip, uint16_t port, int id)
{
    int msdfd;
    struct sockaddr_in msdaddr = { 0 };

    msdfd = socket(AF_INET, SOCK_STREAM, 0);
    if (msdfd < 0) {
        dev.log(LOG_ERR, "failed to create socket: %m");
        return -1;
    }

    msdaddr.sin_family = AF_INET;
    msdaddr.sin_addr.s_addr = inet_addr(ip.c_str());
    msdaddr.sin_port = htons(port);
    if (connect(msdfd, (struct sockaddr *)&msdaddr, sizeof(msdaddr)) != 0) {
        dev.log(LOG_ERR, "failed to connect to msd: %m");
        close(msdfd);
        return -1;
    }

    id = htonl(id);
    if (write(msdfd, &id, sizeof(id)) != sizeof(id)) {
        dev.log(LOG_ERR, "failed to send id to msd: %m");
        close(msdfd);
        return -1;
    }

    int ret = 0;
    if (read(msdfd, &ret, sizeof(ret)) != sizeof(ret) || ret) {
        dev.log(LOG_ERR, "id not recognized by msd");
        close(msdfd);
        return -1;
    }

    dev.log(LOG_INFO, "successfully connected to msd");
    return msdfd;
}

// Client of MSD. Will quit on any error from either local mailbox or socket fd.
// No retry is ever conducted.
static void mpd(std::shared_ptr<pcidev::pci_device> d)
{
    int msdfd = -1, mbxfd = -1;
    std::string ip;

    pcieFunc dev(d);

    mbxfd = dev.getMailbox();
    if (mbxfd == -1)
        return;

    if (!dev.loadConf())
        return;

    ip = getIP(dev.getHost());
    if (ip.empty()) {
        dev.log(LOG_ERR, "Can't find out IP from host: %s", dev.getHost());
        return;
    }

    dev.log(LOG_INFO, "peer msd ip=%s, port=%d, id=0x%x",
        ip.c_str(), dev.getPort(), dev.getId());

    if ((msdfd = connectMsd(dev, ip, dev.getPort(), dev.getId())) < 0)
        return;

    for ( ;; ) {
        int ret = waitForMsg(dev, mbxfd, msdfd, 0);
        if (ret < 0)
            break;

        if (ret == mbxfd) {
            if (processLocalMsg(dev, mbxfd, msdfd) != 0)
                break;
        } else {
            if (processRemoteMsg(dev, mbxfd, msdfd) != 0)
                break;
        }
    }

    close(msdfd);
}

int main(void)
{
    // Daemon has no connection to terminal.
    fcloseall();
        
    // Start logging ASAP.
    openlog("mpd", LOG_PID|LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "started");

    // Fire up one thread for each board.
    auto total = pcidev::get_dev_total();
    if (total == 0)
        syslog(LOG_INFO, "no device found");
    std::vector<std::thread> threads;
    for (size_t i = 0; i < total; i++)
        threads.emplace_back(mpd, pcidev::get_dev(i));

    // Wait for all threads to finish before quit.
    for (auto& t : threads)
        t.join();

    syslog(LOG_INFO, "ended");
    closelog();         
    return 0;
}
