/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <netinet/in.h>
#include <libudev.h>
#include <boost/algorithm/string.hpp>
#include "boost/filesystem.hpp"

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
#include <exception>
#include <dlfcn.h>

#include "pciefunc.h"
#include "sw_msg.h"
#include "common.h"
#include "mpd_plugin.h"

enum Hotplug_state {
    MAILBOX_REMOVED,
    MAILBOX_ADDED,
};
static bool quit = false;
static const std::string plugin_path("/opt/xilinx/xrt/lib/libmpd_plugin.so");
static struct mpd_plugin_callbacks plugin_cbs;
static std::map<std::string, std::atomic<bool>> threads_handling;
static std::map<std::string, enum Hotplug_state> state_machine;
static std::map<std::string, std::shared_ptr<Msgq<queue_msg>>> threads_msgq;
static std::map<std::string, std::string>dev_maj_min;
udev* mpd_hotplug;
udev_monitor* mpd_hotplug_monitor;

class Mpd : public Common
{
public:
    Mpd(const std::string name, const std::string plugin_path, bool for_user) :
        Common(name, plugin_path, for_user), plugin_init(nullptr), plugin_fini(nullptr)
    {
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
    void update_profile_subdev_to_container(const std::string &sysfs_name,
        const std::string &subdev_name,    
        const std::string &suffix);
    void update_cgroup_device(const std::string &cgroup_file,
        const std::string &subdev_name);
    std::string get_xocl_major_minor(const std::string &sysfs_name);
    bool device_in_container(const std::string major_minor, std::string &path);
    bool file_exist(const std::string &name);
    bool string_in_file(const std::string &name, const std::string &str);
    void extract_sysfs_name_and_subdev_name(const std::string &devpath,
        std::string &sysfs_name,
        std::string &subdev);
};

/*
 * get major:minor info of the xocl node from sysfs entry
 * /sys/bus/pci/devices/dbdf/drm/renderDxxx/dev
 */
std::string Mpd::get_xocl_major_minor(const std::string &sysfs_name)
{
    std::string sysfs_base = "/sys/bus/pci/devices/";

    if (!file_exist(sysfs_base + sysfs_name + "/drm"))
            return "";

    boost::filesystem::directory_iterator dir(sysfs_base + sysfs_name + "/drm"), end;
    while (dir != end) {
        std::string fn = dir->path().filename().string();
        if (fn.find("render") != std::string::npos) {
            fn = dir->path().string() + "/dev";
            std::ifstream dev(fn);
            if (dev.is_open()) {
                std::string ret;
                std::getline(dev, ret);
                dev.close();
                return ret;
            }
        }
        dir++;
    }
    return "";    
}

bool Mpd::file_exist(const std::string &name)
{
    struct stat buf;
    return (stat(name.c_str(), &buf) == 0);
}

/*
 * check whether major:minor info is in the device cgroup file
 */   
bool Mpd::string_in_file(const std::string &name, const std::string &str)
{
    std::ifstream f(name);
    if (f.is_open()) {
        std::string line;
        while (!f.eof()) {
            getline(f, line);
            if (line.find(str) != std::string::npos)
                return true;
        }
        f.close();
    }
    return false;
}

/*
 * check whether a device is assigned to a container.
 * lxc containers are under folder 'lxc'
 * docker containers are under folder 'docker'
 * kubernetes & openshift OCI compliant containers are under folder 'kubepods'
 */ 
bool Mpd::device_in_container(const std::string major_minor, std::string &path)
{
    std::string cgroup_base = "/sys/fs/cgroup/devices/";
    std::vector<std::string> folder = { "lxc", "docker", "kubepods" };
    std::string target = "devices.list";

    if (major_minor.empty())
        return false;

    for (auto &t : folder) {
        if (!file_exist(cgroup_base + t))
            continue;        
        boost::filesystem::recursive_directory_iterator dir(cgroup_base + t), end;
        while (dir != end) {
            std::string fn = dir->path().filename().string();
            if (!fn.compare(target)) {
                if (string_in_file(dir->path().string(), major_minor)) {
                    path = dir->path().string();
                    return true;
                }
            }
            dir++;
        }
    }
    return false;
}

/*
 * get major:minor info of the subdevice, and add the info to cgroup file
 * device.[allow|deny]
 */ 
void Mpd::update_cgroup_device(const std::string &cgroup_file,
    const std::string &subdev_name)
{
    std::string fn = "/dev/xfpga/" + subdev_name;
    struct stat buf;
    if (stat(fn.c_str(), &buf) == 0 &&
        ((buf.st_mode & S_IFMT) == S_IFCHR)) {
        std::string str = "c ";
        str += std::to_string(major(buf.st_rdev));
        str += ":";
        str += std::to_string(minor(buf.st_rdev));
        str += " rwm";
        std::ofstream f(cgroup_file);
        if (f.is_open()) {
            f << str;
            f.close();
        }
        syslog(LOG_INFO, "subdev %s(%s) added to container %s",
            fn.c_str(), str.c_str(), cgroup_file.c_str());
    }    
}

/*
 * If the FPGA device is assigned to a container, update the ULP subdevice
 * major:minor info to container cgroup file also
 */ 
void Mpd::update_profile_subdev_to_container(const std::string &sysfs_name,
    const std::string &subdev_name,
    const std::string &suffix)
{
    std::string major_minor = dev_maj_min[sysfs_name];
    std::string path;
    if (device_in_container(major_minor, path)) {
        path.replace(path.rfind(".") + 1, suffix.size(), suffix);
        update_cgroup_device(path, subdev_name);
    }
}

/*
 * From udev event info, extract the sysfs_name (dbdf) of the xocl node
 * and subdev name.
 * udev so far only monitors mailbox and ULP subdevice events
 */
void Mpd::extract_sysfs_name_and_subdev_name(const std::string &devpath,
    std::string &sysfs_name,
    std::string &subdev)
{
    std::vector<std::string> subdevs = {
        "mailbox.u", "aximm_mon.u", "accel_mon.u",
        "axistream_mon.u", "trace_fifo_lite.u", "trace_fifo_full.u",
        "trace_funnel.u", "trace_s2mm.u", "lapc.u", "spc.u"
    };
    for (auto &t : subdevs) {
        size_t pos_s, pos_e = devpath.find(t);
        if (pos_e != std::string::npos) {
            pos_s = devpath.substr(0, pos_e - 1).rfind("/"); 
            sysfs_name = devpath.substr(pos_s + 1, pos_e - pos_s - 2);
            pos_s = devpath.rfind("!");
                if (pos_s != std::string::npos) {
                    subdev = devpath.substr(pos_s + 1);
            }
            break;    
        }
    }
}

void Mpd::start()
{
    mpd_hotplug = udev_new();
    if (!mpd_hotplug)
        throw std::runtime_error("mpd: can't create udev object");
    mpd_hotplug_monitor = udev_monitor_new_from_netlink(mpd_hotplug, "udev");
    udev_monitor_enable_receiving(mpd_hotplug_monitor);

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
     * MPD, running as a daemon, will open mailbox subdevice. As a result, removing
     * the xocl module before mailbox is closed is impossible, this will make
     * the hotplug hang.(hotplug is required in some cases, eg, auzre hotreset,
     * aws device id change, etc. In order to handle that, mpd is to monitor udev
     * events, which hotplug will produce. For each hotplug, a bunch of events will
     * be produced, here we need to monitor mailbox remove and add events.
     * We maintain a state machine for each fpga. After mpd get started, the state is
     * initialized as MAILBOX_ADDED, we create pair of threads for each fpga. Whenever
     * a mailbox remove event is monitored, the state machine changes to MAILBOX_REMOVED,
     * and the pair of threads will exit and the mailbox will be closed. After a
     * mailbox add event is monitored, new pair of threads will be created.
     *
     */
    for (size_t i = 0; i < total; i++) {
        std::string sysfs_name = pcidev::get_dev(i, true)->sysfs_name;
	std::string major_minor;;
	major_minor = get_xocl_major_minor(sysfs_name);

	if (!major_minor.empty())
		dev_maj_min[sysfs_name] =  major_minor;
	else
            syslog(LOG_INFO, "could not read major:minor number for %s", sysfs_name.c_str());

        state_machine[sysfs_name] = MAILBOX_ADDED;
    }

    int udev_fd = udev_monitor_get_fd(mpd_hotplug_monitor);
    do
    {
        if (total == 0)
            syslog(LOG_INFO, "no device found");
        for (size_t i = 0; i < total; i++) {
            std::string sysfs_name = pcidev::get_dev(i, true)->sysfs_name;

            if (state_machine[sysfs_name] != MAILBOX_ADDED)
                continue;
            if (threads_getMsg.find(sysfs_name) != threads_getMsg.end() &&
                threads_handleMsg.find(sysfs_name) != threads_handleMsg.end())
                continue;

            threads_handling[sysfs_name] = true;
            
            /*
             * create the thread pair for it.
             */
            syslog(LOG_INFO, "create thread pair for %s", sysfs_name.c_str());
            std::shared_ptr<Msgq<queue_msg>> msgq = std::make_shared<Msgq<queue_msg>>();
            threads_msgq[sysfs_name] = msgq;
            threads_getMsg.insert(std::pair<std::string, std::thread>(sysfs_name,
                std::move(std::thread(Mpd::mpd_getMsg, i))));
            threads_handleMsg.insert(std::pair<std::string, std::thread>(sysfs_name,
                std::move(std::thread(Mpd::mpd_handleMsg, i))));
            syslog(LOG_INFO, "%ld pairs of threads running...", threads_getMsg.size());
        }


        std::string sysfs_name = "";
        int ret = waitForMsg(udev_fd, 3);
        if (ret) { //timeout
            continue;
        } else { //udev events
            udev_device* udev_dev = udev_monitor_receive_device(mpd_hotplug_monitor);
            if (!udev_dev)
                continue;
            const char *subsystem = udev_device_get_subsystem(udev_dev);
            if (!subsystem || strcmp(subsystem, "xrt_user")) {
                udev_device_unref(udev_dev);
                continue;
            }
            const char *devpath = udev_device_get_devpath(udev_dev);
            if (!devpath) {
                udev_device_unref(udev_dev);
                continue;
            }
            std::string pathStr = devpath;
            std::string subdev = "";
            extract_sysfs_name_and_subdev_name(pathStr, sysfs_name, subdev);
            if (subdev.empty() || sysfs_name.empty()) {
                udev_device_unref(udev_dev);
                continue;
            }

            const char *action = udev_device_get_action(udev_dev);
            if (action && strcmp(action, "remove") == 0) {
                if (subdev.find("mailbox.u") != std::string::npos) {
                    state_machine[sysfs_name] = MAILBOX_REMOVED;
                    threads_handling[sysfs_name] = false;
                    if (threads_getMsg.find(sysfs_name) != threads_getMsg.end()) {
                        threads_getMsg[sysfs_name].join();
                        threads_getMsg.erase(sysfs_name);
                    }
                    if (threads_handleMsg.find(sysfs_name) != threads_handleMsg.end()) {
                        threads_handleMsg[sysfs_name].join();
                        threads_handleMsg.erase(sysfs_name);
                    }
                    syslog(LOG_INFO, "udev: %s %s. Close mailbox", action, devpath);
                } else {
                    syslog(LOG_INFO, "udev: %s %s of %s", action, subdev.c_str(), devpath);
                    update_profile_subdev_to_container(sysfs_name, subdev, "deny");
                }
            } else if (action && strcmp(action, "add") == 0 ) {
                if (subdev.find("mailbox.u") != std::string::npos &&
                    state_machine[sysfs_name] == MAILBOX_REMOVED) {
                    state_machine[sysfs_name] = MAILBOX_ADDED;
                    syslog(LOG_INFO, "udev: %s %s. Open mailbox", action, devpath);
                } else if (subdev.find("mailbox.u") == std::string::npos) {
                    syslog(LOG_INFO, "udev: %s %s of %s", action, subdev.c_str(), devpath);
                    update_profile_subdev_to_container(sysfs_name, subdev, "allow");
                }
            }
            udev_device_unref(udev_dev);
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

    if (mpd_hotplug_monitor)
        udev_monitor_unref(mpd_hotplug_monitor);
    if (mpd_hotplug)
        udev_unref(mpd_hotplug);

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
        //do reset
        Sw_mb_container c(sizeof(int), orig->id());
        if (plugin_cbs.mb_req.hot_reset) {
            int *resp = reinterpret_cast<int *>(c.get_payload_buf());
            c.set_hook(std::bind(plugin_cbs.mb_req.hot_reset, dev.getIndex(), resp));
        }
        processed = c.get_response();
        dev.log(LOG_INFO, "mpd daemon: response %d sent ret = %d", req->req,
             *((int *)(processed->payloadData())));
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
            dev.log(LOG_ERR, "failed to get remote fd in plugin, mpd_getMsg thread for %s exit!!", sysfs_name.c_str());
            threads_handling[sysfs_name] = false;
            return;
        }
        cb = Mpd::localMsgHandler;
    } else {
        if (!dev.loadConf()) {
            dev.log(LOG_ERR, "loadConf() failed, mpd_getMsg thread for %s exit!!", sysfs_name.c_str());
            threads_handling[sysfs_name] = false;
            return;
        }

        ip = getIP(dev.getHost());
        if (ip.empty()) {
            dev.log(LOG_ERR, "Can't find out IP from host: %s, mpd_getMsg thread for %s exit!!",
                    dev.getHost(), sysfs_name.c_str());
            threads_handling[sysfs_name] = false;
            return;
        }

        dev.log(LOG_INFO, "peer msd ip=%s, port=%d, id=0x%x",
            ip.c_str(), dev.getPort(), dev.getId());

        if ((msdfd = connectMsd(dev, ip, dev.getPort(), dev.getId())) < 0) {
            dev.log(LOG_ERR, "Unable to connect to msd, mpd_getMsg thread for %s exit!!", sysfs_name.c_str());
            threads_handling[sysfs_name] = false;
            return;
        }
    }

    mbxfd = dev.getMailbox();
    if (mbxfd == -1) {
        dev.log(LOG_ERR, "Unable to get mailbox fd, mpd_getMsg thread for %s exit!!",
                sysfs_name.c_str());
        threads_handling[sysfs_name] = false;
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
            dev.log(LOG_ERR, "failed to mark mgmt as online");
    }

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
            dev.log(LOG_ERR, "failed to mark mgmt as offline");
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
    try {
        Mpd mpd("mpd", plugin_path, true);
        mpd.preStart();
        mpd.start();
        mpd.run();
        mpd.stop();
        mpd.postStop();
    } catch (std::exception& e) {
        syslog(LOG_ERR, "mpd: %s", e.what());
    }

    return 0;
}
