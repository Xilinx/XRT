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
#include <cstring>
#include <exception>
#include <dlfcn.h>

#include "pciefunc.h"
#include "sw_msg.h"
#include "common.h"
#include "msd_plugin.h"
#include "xrt/detail/xclbin.h"
#include "core/pcie/driver/linux/include/mgmt-ioctl.h"

static bool quit = false;
static const std::string configFile("/etc/msd.conf");
// We'd like to only handle below request through daemons.
static uint64_t chanSwitch = (1UL<<XCL_MAILBOX_REQ_TEST_READY) |
                      (1UL<<XCL_MAILBOX_REQ_TEST_READ)  |
                      (1UL<<XCL_MAILBOX_REQ_LOAD_XCLBIN) |
                      (1UL<<XCL_MAILBOX_REQ_LOAD_SLOT_XCLBIN);
static struct msd_plugin_callbacks plugin_cbs;
#ifdef XRT_INSTALL_PREFIX
    #define MSD_PLUGIN_PATH XRT_INSTALL_PREFIX "/xrt/lib/libmsd_plugin.so"
#else
    #define MSD_PLUGIN_PATH "/opt/xilinx/xrt/lib/libmsd_plugin.so"
#endif
static const std::string plugin_path(MSD_PLUGIN_PATH);

class Msd : public Common
{
public:
    Msd(const std::string name, const std::string plugin_path, bool for_user) :
        Common(name, plugin_path, for_user), plugin_init(nullptr), plugin_fini(nullptr)
    {
    }

    ~Msd()
    {
    }

    void start();
    void run();
    void stop();
    static std::string getHost();
    static void createSocket(const pcieFunc& dev, int& sockfd, uint16_t& port);
    static int verifyMpd(const pcieFunc& dev, int mpdfd, int id);
    static int connectMpd(const pcieFunc& dev, int sockfd, int id, int& mpdfd);
    static void msd_thread(size_t index, std::string host);
    static int remoteMsgHandler(const pcieFunc& dev, std::unique_ptr<sw_msg>& orig,
        std::unique_ptr<sw_msg>& processed);
    static int download_xclbin(const pcieFunc& dev, char *xclbin, uint32_t slot_id = 0);

    init_fn plugin_init;
    fini_fn plugin_fini;
    std::vector<std::thread> threads;

private:
};


void Msd::start()
{
    if (plugin_handle != nullptr) {
        plugin_init = (init_fn) dlsym(plugin_handle, INIT_FN_NAME);
        plugin_fini = (fini_fn) dlsym(plugin_handle, FINI_FN_NAME);
        if (plugin_init == nullptr || plugin_fini == nullptr) {
            syslog(LOG_ERR, "failed to find init/fini symbols in mpd plugin");
            return;
        }
        int ret = (*plugin_init)(&plugin_cbs);
        if (ret != 0)
            syslog(LOG_ERR, "msd plugin_init failed: %d", ret);
    }
}

void Msd::run()
{
    // Fetching host name from config file.
    std::string host = getHost();
    if (host.empty()) {
        syslog(LOG_INFO, "msd: can't get host info");
        return;
    }

    // Fire one thread for each board.
    if (total == 0)
        syslog(LOG_INFO, "no device found");
    for (size_t i = 0; i < total; i++)
        threads.emplace_back(Msd::msd_thread, i, host);
}

void Msd::stop()
{
    // Wait for all threads to finish before quit.
    for (auto& t : threads)
        t.join();

    if (plugin_fini)
        (*plugin_fini)(plugin_cbs.mpc_cookie);
}

// Get host configured in config file
std::string Msd::getHost()
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

void Msd::createSocket(const pcieFunc& dev, int& sockfd, uint16_t& port)
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

int Msd::verifyMpd(const pcieFunc& dev, int mpdfd, int id)
{
    int mpdid;

    if (recv(mpdfd, &mpdid, sizeof(mpdid), MSG_WAITALL) != sizeof(mpdid)) {
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

int Msd::connectMpd(const pcieFunc& dev, int sockfd, int id, int& mpdfd)
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

int Msd::download_xclbin(const pcieFunc& dev, char *xclbin, uint32_t slot_id)
{
    retrieve_xclbin_fini_fn done = nullptr;
    void *done_arg = nullptr;
    char *newxclbin = nullptr;
    size_t newlen = 0;
    int ret = 0;

    xclmgmt_ioc_bitstream_axlf x = {reinterpret_cast<axlf *>(xclbin)};
    if (plugin_cbs.retrieve_xclbin) {
        ret = (*plugin_cbs.retrieve_xclbin)(xclbin,
            x.xclbin->m_header.m_length, &newxclbin, &newlen, &done, &done_arg);
        if (ret)
            return ret;
    } else {
        newxclbin = xclbin;
        newlen = x.xclbin->m_header.m_length;
        done = NULL;
    }

    if (newxclbin == nullptr || newlen == 0)
        return -EINVAL;

    xclmgmt_ioc_bitstream_axlf obj = {reinterpret_cast<axlf *>(newxclbin)};
    int fd = dev.getDev()->open("", O_RDWR);
    ret = dev.getDev()->ioctl(fd, XCLMGMT_IOCICAPDOWNLOAD_AXLF, &obj);
    dev.getDev()->close(fd);

    if (done)
        (*done)(done_arg, newxclbin, newlen);

    return ret;
}

int Msd::remoteMsgHandler(const pcieFunc& dev, std::unique_ptr<sw_msg>& orig,
    std::unique_ptr<sw_msg>& processed)
{
    int pass = FOR_LOCAL;
    xcl_mailbox_req *req = reinterpret_cast<xcl_mailbox_req *>(orig->payloadData());
    if (orig->payloadSize() < sizeof(xcl_mailbox_req)) {
        dev.log(LOG_ERR, "peer request dropped, wrong size");
        return -EINVAL;
    }
    size_t reqSize = orig->payloadSize() - sizeof(xcl_mailbox_req);
    
    switch (req->req) {
    case XCL_MAILBOX_REQ_LOAD_XCLBIN: {
        axlf *xclbin = reinterpret_cast<axlf *>(req->data);
        if (reqSize < sizeof(*xclbin)) {
            dev.log(LOG_ERR, "peer request dropped, wrong size");
            pass = -EINVAL;
            break;
        }
        uint64_t xclbinSize = xclbin->m_header.m_length;
        if (reqSize < xclbinSize) {
            dev.log(LOG_ERR, "peer request dropped, wrong size");
            pass = -EINVAL;
            break;
        }

        int ret = download_xclbin(dev, reinterpret_cast<char *>(req->data));
        dev.log(LOG_INFO, "xclbin download, ret=%d", ret);
        processed = std::make_unique<sw_msg>(&ret, sizeof(ret), orig->id(),
            XCL_MB_REQ_FLAG_RESPONSE);
        pass = FOR_REMOTE;
        break;
    }
    case XCL_MAILBOX_REQ_LOAD_SLOT_XCLBIN: {
        struct xcl_mailbox_bitstream_slot_xclbin *mb_xclbin =
                     (struct xcl_mailbox_bitstream_slot_xclbin *)req->data;
        uint32_t slot_id = mb_xclbin->slot_idx;
        axlf *xclbin = reinterpret_cast<axlf *>((uint64_t)req->data +
				     sizeof(struct xcl_mailbox_bitstream_slot_xclbin));
        uint64_t xclbinSize = xclbin->m_header.m_length;
        if (reqSize < xclbinSize) {
            dev.log(LOG_ERR, "peer request dropped, wrong size");
            pass = -EINVAL;
            break;
        }

        int ret = download_xclbin(dev, reinterpret_cast<char *>(xclbin), slot_id);
        dev.log(LOG_INFO, "xclbin download, ret=%d", ret);
        processed = std::make_unique<sw_msg>(&ret, sizeof(ret), orig->id(),
            XCL_MB_REQ_FLAG_RESPONSE);
        pass = FOR_REMOTE;
        break;
    }
    default:
        processed = std::move(orig);
        break;
    }
    return pass;
}

// Server serving MPD. Any error from socket fd, re-accept, don't quit.
// Will quit on any error from local mailbox fd.
void Msd::msd_thread(size_t index, std::string host)
{
    uint16_t port;
    int sockfd = -1, mpdfd = -1, mbxfd = -1;
    int retfd[2];
    int ret;
    const int interval = 2;

    pcieFunc dev(index, false);

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
            if (ret) {
                mpdfd = -1;
                sleep(interval);
                continue; // MPD is not ready yet, retry.
            }
        }

        retfd[0] = retfd[1] = -100;
        // Waiting for msg to show up, interval is 3 seconds.
        ret = waitForMsg(dev, mbxfd, mpdfd, interval, retfd);
        if (ret < 0) {
            if (ret == -EAGAIN) // MPD has been quiet, retry.
                continue;

            close(mpdfd);
            mpdfd = -1;
            continue;
        }

        // Process msg.
        for (int i = 0; i < 2; i++) {
            struct queue_msg msg = {0};
            if (retfd[i] == mbxfd) {
                msg.localFd = mbxfd;
                msg.remoteFd = -1;
                msg.type = LOCAL_MSG;
                msg.data = std::move(getLocalMsg(dev, mbxfd));
            } else if (retfd[i] == mpdfd) {
                msg.localFd = -1;
                msg.remoteFd = mpdfd;
                msg.type = REMOTE_MSG;
                msg.data = std::move(getRemoteMsg(dev, mpdfd));
                msg.cb = Msd::remoteMsgHandler;
            } else {
                continue;
            }

            ret = handleMsg(dev, msg);
            if (ret) { // Socket connection was lost, retry
                if (mpdfd >= 0)
                    close(mpdfd);
                mpdfd = -1;
                continue;
            }
        }
    }

done:
    dev.updateConf("", 0, 0); // Restore default config.
    if (mpdfd >= 0)
    	close(mpdfd);
    if (sockfd >= 0)
    	close(sockfd);
}

/*
 * daemon will gracefully exit(eg notify mailbox driver) when
 * 'kill -15' is sent. or 'crtl c' on the terminal for debug.
 * so far 'kill -9' is not handled.
 */
static void signalHandler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        syslog(LOG_INFO, "msd caught signal %d", signum);
        quit = true;
    }
}

int main(void)
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        Msd msd("msd", plugin_path, false);
        msd.preStart();
        msd.start();
        msd.run();
        msd.stop();
        msd.postStop();
    } catch (std::exception& e) {
        syslog(LOG_ERR, "msd: %s", e.what());
    }

    return 0;
}
