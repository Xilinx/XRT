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
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <dlfcn.h>

#include "pciefunc.h"
#include "sw_msg.h"
#include "common.h"
#include "mpd_plugin.h"

static bool quit = false;
// Support for msd plugin
static void *plugin_handle;
static init_fn plugin_init;
static fini_fn plugin_fini;
static struct mpd_plugin_callbacks plugin_cbs;
static const std::string plugin_path("/opt/xilinx/xrt/lib/libmpd_plugin.so");

// Init plugin callbacks
static void init_plugin()
{
    plugin_handle = dlopen(plugin_path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (plugin_handle == nullptr)
        return;

    syslog(LOG_INFO, "found mpd plugin: %s", plugin_path.c_str());
    plugin_init = (init_fn) dlsym(plugin_handle, INIT_FN_NAME);
    plugin_fini = (fini_fn) dlsym(plugin_handle, FINI_FN_NAME);
    if (plugin_init == nullptr || plugin_fini == nullptr)
        syslog(LOG_ERR, "failed to find init/fini symbols in mpd plugin");
}

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

/*
 * Local mailbox msg handler which is used to interpret the msg and handle it.
 * If libmpd_plugin.so is found, which means the users don't need the software
 * mailbox in the mgmt side, instead, users want to inperpret and handle the
 * the mailbox msg from user PF themselves, this local mailbox msg handler is
 * required. A typical use case is, xclbin download, when the users want have
 * their own control.
 */
int local_msg_handler(pcieFunc& dev, std::shared_ptr<sw_msg>& orig,
    std::shared_ptr<sw_msg>& processed)
{
    int ret = 0;
    mailbox_req *req = reinterpret_cast<mailbox_req *>(orig->payloadData());
    size_t reqSize;
    if (orig->payloadSize() < sizeof(mailbox_req)) {
        dev.log(LOG_ERR, "local request dropped, wrong size");
        ret = -EINVAL;
        goto out;
    }
    reqSize = orig->payloadSize() - sizeof(mailbox_req);

    dev.log(LOG_INFO, "mpd daemon: request %d received", req->req);

    switch (req->req) {
    case MAILBOX_REQ_LOAD_XCLBIN: {//mandatory for every plugin
        const axlf *xclbin = reinterpret_cast<axlf *>(req->data);
        if (reqSize < sizeof(*xclbin)) {
            dev.log(LOG_ERR, "local request(%d) dropped, wrong size", req->req);
            ret = -EINVAL;
            break;
        }
        if (!plugin_cbs.load_xclbin) {
            ret = -ENOTSUP;
            break;
        }
        ret = (*plugin_cbs.load_xclbin)(dev.getIndex(), xclbin);
        break;
    }
    case MAILBOX_REQ_PEER_DATA: {//optional. aws plugin need to implement this. 
        void *resp;
        size_t resp_len = 0;
        struct mailbox_subdev_peer *subdev_req =
            reinterpret_cast<struct mailbox_subdev_peer *>(req->data);
        if (reqSize < sizeof(*subdev_req)) {
            dev.log(LOG_ERR, "local request(%d) dropped, wrong size", req->req);
            ret = -EINVAL;
            break;
        }
        switch (subdev_req->kind) {
        case ICAP: {
            if (!plugin_cbs.get_icap_data) {
                ret = -ENOTSUP;
                break;
            }
            std::shared_ptr<struct xcl_pr_region> data;
            ret = (*plugin_cbs.get_icap_data)(dev.getIndex(),
                data, resp_len);
            resp = data.get();
            break;
        }
        case SENSOR: {
            if (!plugin_cbs.get_sensor_data) {
                ret = -ENOTSUP;
                break;
            }
            std::shared_ptr<struct xcl_sensor> data;
            ret = (*plugin_cbs.get_sensor_data)(dev.getIndex(),
                data, resp_len);
            resp = data.get();
            break;
        }
        case BDINFO: {
            if (!plugin_cbs.get_board_info) {
                ret = -ENOTSUP;
                break;
            }
            std::shared_ptr<struct xcl_board_info> data;
            ret = (*plugin_cbs.get_board_info)(dev.getIndex(),
                data, resp_len);
            resp = data.get();
            break;
        }
        case MIG_ECC: {
            if (!plugin_cbs.get_mig_data) {
                ret = -ENOTSUP;
                break;
            }
            std::shared_ptr<struct xcl_mig_ecc> data;
            ret = (*plugin_cbs.get_mig_data)(dev.getIndex(),
                data, resp_len);
            resp = data.get();
            break;
        }
        case FIREWALL: {
            if (!plugin_cbs.get_firewall_data) {
                ret = -ENOTSUP;
                break;
            }
            std::shared_ptr<struct xcl_mig_ecc> data;
            ret = (*plugin_cbs.get_firewall_data)(dev.getIndex(),
                data, resp_len);
            resp = data.get();
            break;
        }
        case DNA: {
            if (!plugin_cbs.get_dna_data) {
                ret = -ENOTSUP;
                break;
            }
            std::shared_ptr<struct xcl_dna> data;
            ret = (*plugin_cbs.get_dna_data)(dev.getIndex(),
                data, resp_len);
            resp = data.get();
            break;
        }
        case SUBDEV: {
            if (!plugin_cbs.get_subdev_data) {
                ret = -ENOTSUP;
                break;
            }
            std::shared_ptr<void> data;
            ret = (*plugin_cbs.get_subdev_data)(dev.getIndex(),
                data, resp_len);
            resp = data.get();
            break;
        }
        default:
            ret = -ENOTSUP;
            break;
        }

        if (!ret) {
            processed = std::make_shared<sw_msg>(resp, resp_len, orig->id(),
                   MB_REQ_FLAG_RESPONSE);
            dev.log(LOG_INFO, "mpd daemon: response %d sent", req->req);
            return FOR_LOCAL;
        }
        break;
    }
    case MAILBOX_REQ_USER_PROBE: {//useful for aws plugin
        struct mailbox_conn_resp resp = {0};
        size_t resp_len = sizeof(struct mailbox_conn_resp);
        resp.conn_flags |= MB_PEER_READY;
        processed = std::make_shared<sw_msg>(&resp, resp_len, orig->id(),
            MB_REQ_FLAG_RESPONSE);
        dev.log(LOG_INFO, "mpd daemon: response %d sent", req->req);
        return FOR_LOCAL;
    }
    case MAILBOX_REQ_LOCK_BITSTREAM: {//optional
        if (!plugin_cbs.lock_bitstream) {
            ret = -ENOTSUP;
            break;
        }
        ret = (*plugin_cbs.lock_bitstream)(dev.getIndex());
        break;
    }
    case MAILBOX_REQ_UNLOCK_BITSTREAM: { //optional
        if (!plugin_cbs.unlock_bitstream) {
            ret = -ENOTSUP;
            break;
        }
        ret = (*plugin_cbs.unlock_bitstream)(dev.getIndex());
        break;
    }
    case MAILBOX_REQ_HOT_RESET: {//optional
        if (!plugin_cbs.hot_reset) {
            ret = -ENOTSUP;
            break;
        }
        ret = (*plugin_cbs.hot_reset)(dev.getIndex());
        break;
    }
    case MAILBOX_REQ_RECLOCK: {//optional
        struct xclmgmt_ioc_freqscaling *obj =
            reinterpret_cast<struct xclmgmt_ioc_freqscaling *>(req->data);
        if (!plugin_cbs.hot_reset) {
            ret = -ENOTSUP;
            break;
        }
        ret = (*plugin_cbs.reclock2)(dev.getIndex(), obj);
        break;
    }
    default:
        break;
    }
out:
    processed = std::make_shared<sw_msg>(&ret, sizeof(ret), orig->id(),
        MB_REQ_FLAG_RESPONSE);
    dev.log(LOG_INFO, "mpd daemon: response %d sent ret = %d", req->req, ret);
    return FOR_LOCAL;
}

/*
 * Function to notify software mailbox online/offline.
 * This is usefull for aws. Since there is no mgmt, when xocl driver is loaded,
 * and before the mpd daemon is running, sending MAILBOX_REQ_USER_PROBE msg
 * will timeout and get no response, so there is no chance to know the card is
 * ready. 
 * With this notification, when the mpd open/close the mailbox instance, a fake
 * MAILBOX_REQ_MGMT_STATE msg is sent to mailbox in xocl, pretending a mgmt is
 * ready, then xocl will send a MAILBOX_REQ_USER_PROBE again. This time, mpd
 * will get and msg and send back a MB_PEER_READY response.
 *
 * For other cloud vendors, as long as they want to handle mailbox msg themselves,
 * eg. load xclbin, mpd and plugin is required. mpd exiting will send a offline
 * notification to xocl, which will mark the card as not ready.
 */
int mb_notify(pcieFunc &dev, int &fd, bool online)
{
    struct queue_msg msg;
    std::shared_ptr<sw_msg> swmsg;
    std::shared_ptr<std::vector<char>> buf;
    struct mailbox_req *mb_req = NULL;
    struct mailbox_peer_state mb_conn = { 0 };
    size_t data_len = sizeof(struct mailbox_peer_state) + sizeof(struct mailbox_req);
   
    buf = std::make_unique<std::vector<char>>(data_len, 0);
    if (buf == nullptr)
        return -ENOMEM;
    mb_req = reinterpret_cast<struct mailbox_req *>(buf->data());

    mb_req->req = MAILBOX_REQ_MGMT_STATE;
    if (online)
        mb_conn.state_flags |= MB_STATE_ONLINE;
    else
        mb_conn.state_flags |= MB_STATE_OFFLINE;
    memcpy(mb_req->data, &mb_conn, sizeof(mb_conn));

    swmsg = std::make_shared<sw_msg>(mb_req, data_len, 0x1234, MB_REQ_FLAG_REQUEST);
    if (swmsg == nullptr)
        return -ENOMEM;

    msg.localFd = fd;
    msg.type = REMOTE_MSG;
    msg.cb = nullptr;
    msg.data = swmsg;

    return handleMsg(dev, msg);    
}

// Client of MPD getting msg. Will quit on any error from either local mailbox or socket fd.
// No retry is ever conducted.
static void mpd_getMsg(size_t index,
       std::shared_ptr<std::mutex> &mtx,
       std::shared_ptr<std::condition_variable> &cv,
       std::shared_ptr<std::queue<struct queue_msg>> &msgq,
       std::shared_ptr<std::atomic<bool>> &is_handling)
{
    int msdfd = -1, mbxfd = -1;
    int ret = 0;
    std::string ip;
    msgHandler cb = nullptr;

    pcieFunc dev(index);

    /*
     * If there is user plugin, then we assume the users either don't want to
     * use the communication channel we setup by default, or they even don't
     * want to use the software mailbox at all. In this case, we interpret the
     * mailbox msg and process the msg with the hook function the plugin provides.
     */
    if (plugin_cbs.get_remote_msd_fd) {
        ret = (*plugin_cbs.get_remote_msd_fd)(dev.getIndex(), msdfd);
        if (ret) {
            syslog(LOG_ERR, "failed to get remote fd in plugin");
            quit = true;
            return;
        }
        cb = local_msg_handler;
    } else {
        if (!dev.loadConf()) {
            quit = true;
            return;
        }

        ip = getIP(dev.getHost());
        if (ip.empty()) {
            dev.log(LOG_ERR, "Can't find out IP from host: %s", dev.getHost());
            quit = true;
            return;
        }

        dev.log(LOG_INFO, "peer msd ip=%s, port=%d, id=0x%x",
            ip.c_str(), dev.getPort(), dev.getId());

        if ((msdfd = connectMsd(dev, ip, dev.getPort(), dev.getId())) < 0) {
            quit = true;
            return;
        }
    }

    mbxfd = dev.getMailbox();
    if (mbxfd == -1) {
        quit = true;
        return;
    }

    /*
     * notify mailbox driver the daemon is ready.
     * when mpd daemon is required, it will also notify mailbox driver when it
     * exits, which to the mailbox acts as if the mgmt is down. Then the card
     * will be marked as not ready
     */
    mb_notify(dev, mbxfd, true);

    struct queue_msg msg = {
        .localFd = mbxfd,
        .remoteFd = msdfd,
        .cb = cb,
        .data = nullptr,
    };

    int retfd[2];
    for ( ;; ) {
        retfd[0] = retfd[1] = -100;
        ret = waitForMsg(dev, mbxfd, msdfd, 3, retfd);

        if (quit)
            break;
        if (!(*is_handling)) //handleMsg thread exits
            break;

        if (ret < 0) {
            if (ret == -EAGAIN)
                continue;
            else
                break;
        }
        for (int i = 0; i < 2; i++) {
            if (retfd[i] == mbxfd) {
                msg.type = LOCAL_MSG;
                msg.data = getLocalMsg(dev, mbxfd);
            } else if (retfd[i] == msdfd) {
                msg.type = REMOTE_MSG;
                msg.data = getRemoteMsg(dev, msdfd);
            } else
                continue;

            if (msg.data == nullptr)
                goto out;
            
            std::unique_lock<std::mutex> lck(*mtx);
            msgq->push(msg);
            (*cv).notify_all();
        }
    }
out:
    msg.type = ILLEGAL_MSG;
    std::unique_lock<std::mutex> lck(*mtx);
    msgq->push(msg);
    (*cv).notify_all();

    //notify mailbox driver the daemon is offline 
    mb_notify(dev, mbxfd, false);

    if (msdfd > 0)     
        close(msdfd);
    dev.log(LOG_INFO, "mpd_getMsg thread %d exit!!", index);
}

// Client of MPD handling msg. Will quit on any error from either local mailbox or socket fd.
// No retry is ever conducted.
static void mpd_handleMsg(size_t index,
       std::shared_ptr<std::mutex> &mtx,
       std::shared_ptr<std::condition_variable> &cv,
       std::shared_ptr<std::queue<struct queue_msg>> &msgq,
       std::shared_ptr<std::atomic<bool>> &is_handling)
{
    pcieFunc dev(index);
    *is_handling = true;
    std::unique_lock<std::mutex> lck(*mtx, std::defer_lock);
    for ( ;; ) {
        lck.lock();
        while (msgq->empty()) {
            (*cv).wait_for(lck, std::chrono::seconds(3));
            if (quit) {
                lck.unlock();
                goto out;
            }
        }

        struct queue_msg msg = msgq->front();
        msgq->pop();
        lck.unlock();
        if (msg.type == ILLEGAL_MSG) //getMsg thread exits
            break;
        if (handleMsg(dev, msg) != 0)
            break;
    }
out:
    *is_handling = false;
    dev.log(LOG_INFO, "mpd_handleMsg thread %d exit!!", index);
}

/*
 * mpd daemon will gracefully exit(eg notify mailbox driver) when
 * 'kill -15' is sent. or 'crtl c' on the terminal for debug.
 * so far 'kill -9' is not handled.
 */
static void signalHandler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        syslog(LOG_INFO, "mpd caught signal %d", signum);
        quit = true;
    }
}

int main(void)
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Daemon has no connection to terminal.
    fcloseall();

    // Start logging ASAP.
    openlog("mpd", LOG_PID|LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "started");

    init_plugin();

    if (plugin_init) {
        int ret = (*plugin_init)(&plugin_cbs);
        if (ret != 0) {
            syslog(LOG_ERR, "mpd plugin_init failed: %d", ret);
            dlclose(plugin_handle);
            return 0;
        }
    }

    /*
     * Fire up 2 threads for each board - one is to get msg and the other is to
     * handle msg. The reason is, in some cases, handle msg may take a relative
     * long time, eg. downloading a large xclbin, and in this case, one thead
     * implementation makes the next mailbox msg not read out promptly and ends
     * up a tx timeout
     */
    auto total = pcidev::get_dev_total();
    if (total == 0)
        syslog(LOG_INFO, "no device found");

    std::vector<std::thread> threads_getMsg;
    std::vector<std::thread> threads_handleMsg;
    for (size_t i = 0; i < total; i++) {
        std::shared_ptr<std::mutex> mtx = std::make_shared<std::mutex>();
        std::shared_ptr<std::condition_variable> cv =
            std::make_shared<std::condition_variable>();
        std::shared_ptr<std::queue<struct queue_msg>> msgq =
            std::make_shared<std::queue<struct queue_msg>>();
        std::shared_ptr<std::atomic<bool>> is_handling =
            std::make_shared<std::atomic<bool>>(true);
        auto t0 = std::bind(&mpd_getMsg, i, mtx, cv, msgq, is_handling);
        threads_getMsg.emplace_back(t0);
        auto t1 = std::bind(&mpd_handleMsg, i, mtx, cv, msgq, is_handling);
        threads_handleMsg.emplace_back(t1);
    }

    // Wait for all threads to finish before quit.
    for (auto& t : threads_handleMsg)
        t.join();
    for (auto& t : threads_getMsg)
        t.join();

    if (plugin_fini)
        (*plugin_fini)(plugin_cbs.mpc_cookie);
    if (plugin_handle)
        dlclose(plugin_handle);

    syslog(LOG_INFO, "ended");
    closelog();
    return 0;
}
