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
#include <map>
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
static const std::string plugin_path("/opt/xilinx/xrt/lib/libmpd_plugin.so");
static struct mpd_plugin_callbacks plugin_cbs;
static std::shared_ptr<Msgq<hotreset_msg>> hotreset_q_req;
static std::map<std::string, std::atomic<bool>> threads_handling;
static std::map<std::string, std::atomic<bool>> remove_done;
static std::map<std::string, std::atomic<bool>> add_done;
static std::map<std::string, std::shared_ptr<Msgq<queue_msg>>> threads_msgq;

class Mpd : public Common
{
public:
    Mpd(const std::string name, const std::string plugin_path, bool for_user) :
        Common(name, plugin_path, for_user), plugin_init(nullptr), plugin_fini(nullptr)
    {
        hotreset_q_req = std::make_shared<Msgq<hotreset_msg>>();
    }

    ~Mpd()
    {
    }

    void start();
    void run();
    void stop();
    static void mpd_getMsg(size_t index);
    static void mpd_handleMsg(size_t index);
    static int localMsgHandler(const pcieFunc& dev,
        std::unique_ptr<sw_msg>& orig,
        std::unique_ptr<sw_msg>& processed);
    static std::string getIP(std::string host);
    static int connectMsd(const pcieFunc& dev, std::string &ip,
        uint16_t port, int id);
    init_fn plugin_init;
    fini_fn plugin_fini;
    std::map<std::string, std::thread> threads_getMsg;
    std::map<std::string, std::thread> threads_handleMsg;

private:
};

void Mpd::start()
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
            syslog(LOG_ERR, "mpd plugin_init failed: %d", ret);
    }
}

void Mpd::run()
{
    /*
     * Fire up 2 threads for each board - one is to get msg and the other is to
     * handle msg. The reason is, in some cases, handle msg may take a relative
     * long time, eg. downloading a large xclbin, and in this case, one thead
     * implementation makes the next mailbox msg not read out promptly and ends
     * up a tx timeout
     *
     * In some cases, the mailbox subdev for some cards need to be closed, while
     * other card should be still running without being impacted. Eg, in azure,
     * when doing hot reset on the card, we close the mailbox by exiting the getMsg
     * thread, and after the reset, the mailbox is reopened. During the period, the
     * handleMsg thread keeps running -- the hot reset response mailbox msg is sent
     * within this thread.
     */
    do
    {
        if (total == 0)
            syslog(LOG_INFO, "no device found");
        for (size_t i = 0; i < total; i++) {
            std::string sysfs_name = pcidev::get_dev(i, true)->sysfs_name;
            if (threads_getMsg.find(sysfs_name) != threads_getMsg.end() &&
                threads_handleMsg.find(sysfs_name) != threads_handleMsg.end())
                continue;

            threads_handling[sysfs_name] = true;
            
            /*
             * getMsg thread for the card has exitted. need to re-create it.
             * there will never be a case where handleMsg thread exits while getMsg
             * thread exists.
             */
            if (threads_getMsg.find(sysfs_name) == threads_getMsg.end() &&
                threads_handleMsg.find(sysfs_name) != threads_handleMsg.end()) {
                threads_getMsg.insert(std::pair<std::string, std::thread>(sysfs_name,
                    std::move(std::thread(Mpd::mpd_getMsg, i))));
                syslog(LOG_INFO, "re-create getMsg thread for %s", sysfs_name.c_str());
                continue;
            }

            /*
             * a card is firstly discovered. create the thread pair for it.
             */
            syslog(LOG_INFO, "create thread pair for %s", sysfs_name.c_str());
            std::shared_ptr<Msgq<queue_msg>> msgq = std::make_shared<Msgq<queue_msg>>();
            threads_msgq[sysfs_name] = msgq;
            add_done[sysfs_name] = false;
            remove_done[sysfs_name] = false;
            threads_getMsg.insert(std::pair<std::string, std::thread>(sysfs_name,
                std::move(std::thread(Mpd::mpd_getMsg, i))));
            threads_handleMsg.insert(std::pair<std::string, std::thread>(sysfs_name,
                std::move(std::thread(Mpd::mpd_handleMsg, i))));
        }

        syslog(LOG_INFO, "%ld pairs of threads running...", threads_getMsg.size());

        while (!quit)
        {
            hotreset_msg req_msg;
            int ret = hotreset_q_req->getMsg(1, req_msg);
            if (ret) //timeout
                continue;

            if (req_msg.type == REMOVE_REQ) {
                syslog(LOG_INFO, "receive req to close mailbox: %s", req_msg.sysfs_name.c_str());
                threads_handling[req_msg.sysfs_name] = false;
                threads_getMsg[req_msg.sysfs_name].join();
                threads_getMsg.erase(req_msg.sysfs_name);
                add_done.erase(req_msg.sysfs_name);
                remove_done[req_msg.sysfs_name] = true;
            } else if (req_msg.type == ADD_REQ) {
                syslog(LOG_INFO, "receive req to open mailbox: %s", req_msg.sysfs_name.c_str());
                break;
            } else //never goes here
                throw;
        }

    } while (!quit);
}

void Mpd::stop()
{
    // Wait for all threads to finish before quit.
    for (auto& t : threads_handleMsg) {
        syslog(LOG_INFO, "%s handleMsg thread exit", t.first.c_str());
        t.second.join();
    }
    for (auto& t : threads_getMsg) {
        syslog(LOG_INFO, "%s getMsg thread exit", t.first.c_str());
        t.second.join();
    }

    if (plugin_fini)
        (*plugin_fini)(plugin_cbs.mpc_cookie);
}

std::string Mpd::getIP(std::string host)
{
    struct hostent *hp = gethostbyname(host.c_str());

    if (hp == NULL)
        return "";

    char dst[INET_ADDRSTRLEN + 1] = { 0 };
    const char *d = inet_ntop(AF_INET, (struct in_addr *)(hp->h_addr),
        dst, sizeof(dst));
    return d;
}

int Mpd::connectMsd(const pcieFunc& dev, std::string &ip, uint16_t port, int id)
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
    if (recv(msdfd, &ret, sizeof(ret), MSG_WAITALL) != sizeof(ret) || ret) {
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
int Mpd::localMsgHandler(const pcieFunc& dev, std::unique_ptr<sw_msg>& orig,
    std::unique_ptr<sw_msg>& processed)
{
    int ret = 0;
    xcl_mailbox_req *req = reinterpret_cast<xcl_mailbox_req *>(orig->payloadData());
    size_t reqSize;
    if (orig->payloadSize() < sizeof(xcl_mailbox_req)) {
        dev.log(LOG_ERR, "local request dropped, wrong size");
        ret = -EINVAL;
        processed = std::make_unique<sw_msg>(&ret, sizeof(ret), orig->id(),
            XCL_MB_REQ_FLAG_RESPONSE);
        dev.log(LOG_INFO, "mpd daemon: response %d sent ret = %d", req->req, ret);
        return FOR_LOCAL;
    }
    reqSize = orig->payloadSize() - sizeof(xcl_mailbox_req);

    dev.log(LOG_INFO, "mpd daemon: request %d received(reqSize: %d)",
           req->req, reqSize);

    switch (req->req) {
    case XCL_MAILBOX_REQ_LOAD_XCLBIN: {//mandatory for every plugin
        Sw_mb_container c(sizeof(int), orig->id());
        if (plugin_cbs.mb_req.load_xclbin) {
            int *resp = reinterpret_cast<int *>(c.get_payload_buf());
            const axlf *xclbin = reinterpret_cast<axlf *>(req->data);
            c.set_hook(std::bind(plugin_cbs.mb_req.load_xclbin, dev.getIndex(), xclbin, resp));
        }
        processed = c.get_response();
        dev.log(LOG_INFO, "mpd daemon: response %d sent ret = %d", req->req,
             *((int *)(processed->payloadData())));
        break;
    }
    case XCL_MAILBOX_REQ_PEER_DATA: {//optional. aws plugin need to implement this. 
        xcl_mailbox_subdev_peer *subdev_req =
            reinterpret_cast<xcl_mailbox_subdev_peer *>(req->data);
        switch (subdev_req->kind) {
        case XCL_ICAP: {
            Sw_mb_container c(sizeof(xcl_pr_region), orig->id());
            if (plugin_cbs.mb_req.peer_data.get_icap_data) {
                xcl_pr_region *resp = reinterpret_cast<xcl_pr_region *>(c.get_payload_buf());
                c.set_hook(std::bind(plugin_cbs.mb_req.peer_data.get_icap_data, dev.getIndex(), resp));
            }
            processed = c.get_response();
            break;
        }
        case XCL_SENSOR: {
            Sw_mb_container c(sizeof(xcl_sensor), orig->id());
            if (plugin_cbs.mb_req.peer_data.get_sensor_data) {
                xcl_sensor *resp = reinterpret_cast<xcl_sensor *>(c.get_payload_buf());
                c.set_hook(std::bind(plugin_cbs.mb_req.peer_data.get_sensor_data, dev.getIndex(), resp));
            }
            processed = c.get_response();
            break;
        }
        case XCL_BDINFO: {
            Sw_mb_container c(sizeof(xcl_board_info), orig->id());
            if (plugin_cbs.mb_req.peer_data.get_board_info) {
                xcl_board_info *resp = reinterpret_cast<xcl_board_info *>(c.get_payload_buf());
                c.set_hook(std::bind(plugin_cbs.mb_req.peer_data.get_board_info, dev.getIndex(), resp));
            }
            processed = c.get_response();
            break;
        }
        case XCL_MIG_ECC: {
            Sw_mb_container c(subdev_req->entries * sizeof(xcl_mig_ecc), orig->id());
            if (plugin_cbs.mb_req.peer_data.get_mig_data) {
                char *resp = c.get_payload_buf();
                size_t actualSz = subdev_req->entries * sizeof(xcl_mig_ecc);
                c.set_hook(std::bind(plugin_cbs.mb_req.peer_data.get_mig_data, dev.getIndex(), resp, actualSz));
            }
            processed = c.get_response();
            break;
        }
        case XCL_FIREWALL: {
            Sw_mb_container c(sizeof(xcl_mig_ecc), orig->id());
            if (plugin_cbs.mb_req.peer_data.get_firewall_data) {
                xcl_mig_ecc *resp = reinterpret_cast<xcl_mig_ecc *>(c.get_payload_buf());
                c.set_hook(std::bind(plugin_cbs.mb_req.peer_data.get_firewall_data, dev.getIndex(), resp));
            }
            processed = c.get_response();
            break;
        }
        case XCL_DNA: {
            Sw_mb_container c(sizeof(xcl_dna), orig->id());
            if (plugin_cbs.mb_req.peer_data.get_dna_data) {
                xcl_dna *resp = reinterpret_cast<xcl_dna *>(c.get_payload_buf());
                c.set_hook(std::bind(plugin_cbs.mb_req.peer_data.get_dna_data, dev.getIndex(), resp));
            }
            processed = c.get_response();
            break;
        }
        case XCL_SUBDEV: {
            Sw_mb_container c(subdev_req->size, orig->id());
            if (plugin_cbs.mb_req.peer_data.get_subdev_data) {
                char *resp = c.get_payload_buf();
                size_t actualSz = subdev_req->size;
                c.set_hook(std::bind(plugin_cbs.mb_req.peer_data.get_subdev_data, dev.getIndex(), resp, actualSz));
            }
            processed = c.get_response();
            break;
        }
        default:
            ret = -ENOTSUP;
            processed = std::make_unique<sw_msg>(&ret, sizeof(ret), orig->id(),
                XCL_MB_REQ_FLAG_RESPONSE);
            break;
        }

        return FOR_LOCAL;
    }
    case XCL_MAILBOX_REQ_USER_PROBE: {//mandary for aws
        Sw_mb_container c(sizeof(xcl_mailbox_conn_resp), orig->id());
        if (plugin_cbs.mb_req.user_probe) {
            xcl_mailbox_conn_resp *resp = reinterpret_cast<xcl_mailbox_conn_resp *>(c.get_payload_buf());
            c.set_hook(std::bind(plugin_cbs.mb_req.user_probe, dev.getIndex(), resp));
        }
        processed = c.get_response();
        break;
    }
    case XCL_MAILBOX_REQ_HOT_RESET: {//optional
        if (plugin_cbs.plugin_cap & (1 << CAP_RESET_NEED_HELP)) {
            //notify the mpd main thread to close mailbox fd
            dev.log(LOG_INFO, "mpd daemon: pre-hotreset");
            hotreset_msg msg;
            std::string sysfs_name = pcidev::get_dev(dev.getIndex(), true)->sysfs_name;
            msg.sysfs_name = sysfs_name;
            msg.type = REMOVE_REQ;
            hotreset_q_req->addMsg(msg);
            while (remove_done.find(sysfs_name) == remove_done.end()
                   || !remove_done[sysfs_name])
                sleep(1);
            remove_done.erase(sysfs_name);
        }
        //do reset
        dev.log(LOG_INFO, "mpd daemon: hotreset");
        Sw_mb_container c(sizeof(int), orig->id());
        if (plugin_cbs.mb_req.hot_reset) {
            int *resp = reinterpret_cast<int *>(c.get_payload_buf());
            c.set_hook(std::bind(plugin_cbs.mb_req.hot_reset, dev.getIndex(), resp));
        }
        processed = c.get_response();

        if (plugin_cbs.plugin_cap & (1 << CAP_RESET_NEED_HELP)) {
            //notify the mpd main thread that reset done
            dev.log(LOG_INFO, "mpd daemon: post-reset");
            hotreset_msg msg;
            std::string sysfs_name = pcidev::get_dev(dev.getIndex(), true)->sysfs_name;
            msg.sysfs_name = sysfs_name;
            msg.type = ADD_REQ;
            hotreset_q_req->addMsg(msg);
            while (add_done.find(sysfs_name) == add_done.end()
                   || !add_done[sysfs_name])
                sleep(1);
        }
        break;
    }
    case XCL_MAILBOX_REQ_RECLOCK: {//optional
        Sw_mb_container c(sizeof(int), orig->id());
        if (plugin_cbs.mb_req.reclock2) {
            int *resp = reinterpret_cast<int *>(c.get_payload_buf());
            struct xclmgmt_ioc_freqscaling *obj =
                reinterpret_cast<struct xclmgmt_ioc_freqscaling *>(req->data);
            c.set_hook(std::bind(plugin_cbs.mb_req.reclock2, dev.getIndex(), obj, resp));
        }
        processed = c.get_response();
        break;
    }
    case XCL_MAILBOX_REQ_PROGRAM_SHELL: {//optional
        Sw_mb_container c(sizeof(int), orig->id());
        if (plugin_cbs.mb_req.program_shell) {
            int *resp = reinterpret_cast<int *>(c.get_payload_buf());
            c.set_hook(std::bind(plugin_cbs.mb_req.program_shell, dev.getIndex(), resp));
        }
        processed = c.get_response();
        break;
    }
    case XCL_MAILBOX_REQ_READ_P2P_BAR_ADDR: {//optional
        Sw_mb_container c(sizeof(int), orig->id());
        if (plugin_cbs.mb_req.read_p2p_bar_addr) {
            const xcl_mailbox_p2p_bar_addr *addr = reinterpret_cast<xcl_mailbox_p2p_bar_addr *>(req->data);
            int *resp = reinterpret_cast<int *>(c.get_payload_buf());
            c.set_hook(std::bind(plugin_cbs.mb_req.read_p2p_bar_addr, dev.getIndex(), addr, resp));
        }
        processed = c.get_response();
        break;
    }
    default:
        processed = std::make_unique<sw_msg>(&ret, sizeof(ret), orig->id(),
            XCL_MB_REQ_FLAG_RESPONSE);
        break;
    }

    return FOR_LOCAL;
}

// Client of MPD getting msg. Will quit on any error from either local mailbox or socket fd.
// No retry is ever conducted.
void Mpd::mpd_getMsg(size_t index)
{
    std::string sysfs_name = pcidev::get_dev(index, true)->sysfs_name;
    std::shared_ptr<Msgq<queue_msg>> msgq = threads_msgq[sysfs_name];
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
        ret = (*plugin_cbs.get_remote_msd_fd)(dev.getIndex(), &msdfd);
        if (ret) {
            syslog(LOG_ERR, "failed to get remote fd in plugin");
            quit = true;
            return;
        }
        cb = Mpd::localMsgHandler;
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
    * Notify software mailbox online
    * This is usefull for aws. Since there is no mgmt, when xocl driver is loaded,
    * and before the mpd daemon is running, sending MAILBOX_REQ_USER_PROBE msg
    * will timeout and get no response, so there is no chance to know the card is
    * ready. 
    * With this notification, when the mpd open/close the mailbox instance, a fake
    * MAILBOX_REQ_MGMT_STATE msg is sent to mailbox in xocl, pretending a mgmt is
    * ready, then xocl will send a MAILBOX_REQ_USER_PROBE again. This time, mpd
    * will get and msg and send back a MB_PEER_READY response.
    */
    if (plugin_cbs.mb_notify) {
        ret = (*plugin_cbs.mb_notify)(index, mbxfd, true);
        if (ret)
            syslog(LOG_ERR, "failed to mark mgmt as online");
    }

    add_done[sysfs_name] = true;

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
        if (!threads_handling[sysfs_name])
            break;

        if (ret < 0) {
            if (ret == -EAGAIN)
                continue;
            else
                break;
        }

        bool broken = false;
        for (int i = 0; i < 2; i++) {
            if (retfd[i] == mbxfd) {
                msg.type = LOCAL_MSG;
                msg.data = std::move(getLocalMsg(dev, mbxfd));
            } else if (retfd[i] == msdfd) {
                msg.type = REMOTE_MSG;
                msg.data = std::move(getRemoteMsg(dev, msdfd));
            } else
                continue;

            if (msg.data == nullptr) {
                broken = true;
                break;
            }
        
            msgq->addMsg(msg);    
        }

        if (broken)
            break;
    }

    threads_handling[sysfs_name] = false;

    //notify mailbox driver the daemon is offline 
    if (plugin_cbs.mb_notify) {
        ret = (*plugin_cbs.mb_notify)(index, mbxfd, false);
        if (ret)
            syslog(LOG_ERR, "failed to mark mgmt as offline");
    }

    if (msdfd > 0)     
        close(msdfd);
    dev.log(LOG_INFO, "mpd_getMsg thread for %s exit!!",
	pcidev::get_dev(index)->sysfs_name.c_str());
}

// Client of MPD handling msg. Will quit on any error from either local mailbox or socket fd.
// No retry is ever conducted.
void Mpd::mpd_handleMsg(size_t index)
{
    pcieFunc dev(index);
    std::string sysfs_name = pcidev::get_dev(index, true)->sysfs_name;
    std::shared_ptr<Msgq<queue_msg>> msgq = threads_msgq[sysfs_name];
    for ( ;; ) {
        struct queue_msg msg;
        if (quit)
                break;
        if (!threads_handling[sysfs_name])
                break;
        int ret = msgq->getMsg(3, msg);
        if (ret) //timeout
            continue;
        if (handleMsg(dev, msg) != 0)
            break;
    }
    threads_handling[sysfs_name] = false;
    dev.log(LOG_INFO, "mpd_handleMsg thread for %s exit!!",
	pcidev::get_dev(index)->sysfs_name.c_str());
}

/*
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

    Mpd mpd("mpd", plugin_path, true);
    mpd.preStart();
    mpd.start();
    mpd.run();
    mpd.stop();
    mpd.postStop();
    return 0;
}
