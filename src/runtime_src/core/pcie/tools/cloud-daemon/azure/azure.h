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
#ifndef _BAREMETAL_DEV_H_
#define _BAREMETAL_DEV_H_

#include <fstream>
#include <vector>
#include <string>
#include "xclhal2.h"
#include "core/pcie/driver/linux/include/mailbox_proto.h"
#include "core/pcie/driver/linux/include/mgmt-ioctl.h"
#include "core/pcie/linux/scan.h"
#include "core/pcie/driver/linux/include/xocl_ioctl.h"
#include "../common.h"
#include "../mpd_plugin.h"
#include <time.h>

enum azure_rest_err {
    E_SPLIT = 2000,
    E_UPLOAD = 2010,
    E_START_REIMAGE = 2020,
    E_GET_REIMAGE_STATUS = 2021,
    E_RESET = 2030,
    E_GET_RESET_STATUS = 2031,
    E_EMPTY_SN = 2040,
};
/*
 * This class is for azure xclbin download handling.
 *
 * Azure uses a http wireserver, which provides RESTful APIs, to handle xclbin
 * download. 3 steps are required to download the xclbin
 * 1. upload the xclbin to wireserver(in 4M chunks) -- POST
 * 2. start async reimage -- GET (this should be a POST, but the API is a GET)
 * 3. query reimage statue -- GET
 */ 
class AzureDev
{
public:
    explicit AzureDev(size_t index);
    ~AzureDev();

    // Bitstreams
    int azureLoadXclBin(const xclBin *buffer);
    int azureHotReset();
    static std::string get_wireserver_ip()
    {
        const std::string config("/etc/mpd.conf");
        //just check ip address format, not validity
        std::regex ip("^([0-9]{1,3}\\.){3}[0-9]{1,3}$");

        std::ifstream cfile(config, std::ifstream::in);
        if (!cfile.good()) {
            std::cerr << "failed to open config file: " << config << std::endl;
            return "";
        }

        for (std::string line; std::getline(cfile, line);) {
            std::string key, value;
            if (splitLine(line, key, value) != 0)
                continue;

            if (key.compare("restip") == 0 &&
                regex_match(value, ip)) {
                cfile.close();
                return value;
            }
        }
        
        cfile.close();    
        return "";
    };
    static std::vector<std::string> get_serial_number()
    {
        std::regex sn("^[0-9a-zA-Z]{12}$");
        std::vector<std::string> ret = {};
	    size_t total = pcidev::get_dev_total();
	    if (!total) {
            std::cerr << "azure: No device found!" << std::endl;
            return ret;
        }
        for (size_t i = 0; i < total; i++) {
            std::string serialNumber, errmsg;
            pcidev::get_dev(i, true)->sysfs_get("xmc", "serial_num", errmsg, serialNumber); 
	        if (!errmsg.empty() || !regex_match(serialNumber, sn)) {
           	    std::cerr << "azure warning(" << pcidev::get_dev(i, true)->sysfs_name << ")";
                std::cerr << " sysfs errmsg: " << errmsg;
                std::cerr << " serialNumber: " << serialNumber;
                std::cerr << std::endl;
            }
            ret.push_back(serialNumber);
        }
        return ret;
    }
private:
    // 4 MB buffer to truncate and send
    static const int transfer_segment_size { 1024 * 4096 };
    static const int rest_timeout { 30 }; //in second
    static const int upload_retry { 15 };
    static const int reset_retry { 3 };
    std::shared_ptr<pcidev::pci_device> dev;
    size_t index;
    int UploadToWireServer(
        const std::string &ip,
        const std::string &endpoint,
        const std::string &target,
        const std::string &data,
        int index,
        int total,
        const std::string &hash);
    std::string REST_Get(
        const std::string &ip,
        const std::string &endpoint,
        const std::string &target);
    int Sha256AndSplit(
        const std::string &input,
        std::vector<std::string> &output,
        std::string &sha);
    void get_fpga_serialNo(std::string &fpgaSerialNo);
    void msleep(long msecs);
};


struct write_unit {
    const char *uptr;
    size_t  sizeleft;
};
int get_remote_msd_fd(size_t index, int* fd);
int azureLoadXclBin(size_t index, const axlf *xclbin, int *resp);
int azureHotReset(size_t index, int *resp);
static size_t read_callback(void *contents, size_t size,
       size_t nmemb, void *userp);
static size_t WriteCallback(void *contents, size_t size,
       size_t nmemb, void *userp);
#endif
