/**
 * Copyright (C) 2016-2018 Xilinx, Inc
 * Author: Sonal Santan, Ryan Radjabi
 * Simple command line utility to inetract with SDX PCIe devices
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
#ifndef XBUTIL_H
#define XBUTIL_H

#include <fstream>
#include <assert.h>
#include <vector>
#include <map>
#include <sstream>
#include <string>

#include "driver/include/xclhal2.h"
#include "driver/include/xclperf.h"
#include "driver/include/xcl_axi_checker_codes.h"
#include "../user_common/dmatest.h"
#include "../user_common/memaccess.h"
#include "../user_common/dd.h"
#include "../user_common/utils.h"
#include "../user_common/sensor.h"
#include "scan.h"
#include "driver/include/xclbin.h"
#include <version.h>

#include <chrono>
using Clock = std::chrono::high_resolution_clock;

#define TO_STRING(x) #x
#define AXI_FIREWALL

#define XCL_NO_SENSOR_DEV_LL    ~(0ULL)
#define XCL_NO_SENSOR_DEV       ~(0U)
#define XCL_NO_SENSOR_DEV_S     0xffff
#define XCL_INVALID_SENSOR_VAL 0

/*
 * Simple command line tool to query and interact with SDx PCIe devices
 * The tool statically links with xcldma HAL driver inorder to avoid
 * dependencies on environment variables like XILINX_OPENCL, LD_LIBRARY_PATH, etc.
 * TODO:
 * Rewrite the command line parsing to provide interface like Android adb:
 * xcldev <cmd> [options]
 */

namespace xcldev {

enum command {
    FLASH,
    PROGRAM,
    CLOCK,
    BOOT,
    HELP,
    QUERY,
    DUMP,
    RUN,
    FAN,
    DMATEST,
    LIST,
    SCAN,
    MEM,
    DD,
    STATUS,
    CMD_MAX,
};
enum subcommand {
    MEM_READ = 0,
    MEM_WRITE,
    STATUS_SPM,
    STATUS_LAPC,
    STATUS_SSPM,
    STREAM,
    STATUS_UNSUPPORTED,
    MEM_QUERY_ECC,
    MEM_RESET_ECC
};
enum statusmask {
    STATUS_NONE_MASK = 0x0,
    STATUS_SPM_MASK = 0x1,
    STATUS_LAPC_MASK = 0x2,
    STATUS_SSPM_MASK = 0x4
};
enum p2pcommand {
    P2P_ENABLE = 0x0,
    P2P_DISABLE,
    P2P_VALIDATE,
};

static const std::pair<std::string, command> map_pairs[] = {
    std::make_pair("flash", FLASH),
    std::make_pair("program", PROGRAM),
    std::make_pair("clock", CLOCK),
    std::make_pair("boot", BOOT),
    std::make_pair("help", HELP),
    std::make_pair("query", QUERY),
    std::make_pair("dump", DUMP),
    std::make_pair("run", RUN),
    std::make_pair("fan", FAN),
    std::make_pair("dmatest", DMATEST),
    std::make_pair("list", LIST),
    std::make_pair("scan", SCAN),
    std::make_pair("mem", MEM),
    std::make_pair("dd", DD),
    std::make_pair("status", STATUS),

};

static const std::pair<std::string, subcommand> subcmd_pairs[] = {
    std::make_pair("read", MEM_READ),
    std::make_pair("write", MEM_WRITE),
    std::make_pair("spm", STATUS_SPM),
    std::make_pair("lapc", STATUS_LAPC),
    std::make_pair("sspm", STATUS_SSPM),
    std::make_pair("stream", STREAM),
    std::make_pair("query-ecc", MEM_QUERY_ECC),
    std::make_pair("reset-ecc", MEM_RESET_ECC)
};

static const std::vector<std::pair<std::string, std::string>> flash_types = {
    // bpi types
    std::make_pair( "7v3", "bpi" ),
    std::make_pair( "8k5", "bpi" ),
    std::make_pair( "ku3", "bpi" ),
    // spi types
    std::make_pair( "vu9p",    "spi" ),
    std::make_pair( "kcu1500", "spi" ),
    std::make_pair( "vcu1525", "spi" ),
    std::make_pair( "ku115",   "spi" )
};

static const std::map<MEM_TYPE, std::string> memtype_map = {
    {MEM_DDR3, "MEM_DDR3"},
    {MEM_DDR4, "MEM_DDR4"},
    {MEM_DRAM, "MEM_DRAM"},
    {MEM_STREAMING, "MEM_STREAMING"},
    {MEM_PREALLOCATED_GLOB, "MEM_PREALLOCATED_GLOB"},
    {MEM_ARE, "MEM_ARE"},
    {MEM_HBM, "MEM_HBM"},
    {MEM_BRAM, "MEM_BRAM"},
    {MEM_URAM, "MEM_URAM"}
};

static const std::map<std::string, command> commandTable(map_pairs, map_pairs + sizeof(map_pairs) / sizeof(map_pairs[0]));

class device {
    unsigned int m_idx;
    xclDeviceHandle m_handle;
    xclDeviceInfo2 m_devinfo;
    xclErrorStatus m_errinfo;

public:
    int domain() {
        return pcidev::get_dev(m_idx)->user->domain;
    }
    int bus() {
        return pcidev::get_dev(m_idx)->user->bus;
    }
    int dev() {
        return pcidev::get_dev(m_idx)->user->dev;
    }
    int userFunc() {
        return pcidev::get_dev(m_idx)->user->func;
    }
    int mgmtFunc() {
        auto dev = pcidev::get_dev(m_idx);
        return dev->mgmt ? dev->mgmt->func : -1;
    }
    device(unsigned int idx, const char* log) : m_idx(idx), m_handle(nullptr), m_devinfo{} {
        std::string devstr = "device[" + std::to_string(m_idx) + "]";
        m_handle = xclOpen(m_idx, log, XCL_QUIET);
        if (!m_handle)
            throw std::runtime_error("Failed to open " + devstr);
        if (xclGetDeviceInfo2(m_handle, &m_devinfo))
            throw std::runtime_error("Unable to obtain info from " + devstr);
#ifdef AXI_FIREWALL
        if (xclGetErrorStatus(m_handle, &m_errinfo))
            throw std::runtime_error("Unable to obtain AXI error from " + devstr);
#endif
    }

    device(device&& rhs) : m_idx(rhs.m_idx), m_handle(rhs.m_handle), m_devinfo(std::move(rhs.m_devinfo)) {
    }

    device(const device &dev) = delete;
    device& operator=(const device &dev) = delete;

    ~device() {
        xclClose(m_handle);
    }

    const char *name() const {
        return m_devinfo.mName;
    }

    /*
     * flash
     *
     * Determine flash method as BPI or SPI from flash_types table by the DSA name.
     * Override this if a flash type is passed in by command line switch.
     */
    int flash( const std::string& mcs1, const std::string& mcs2, std::string flashType )
    {
        std::cout << "Flash disabled. See 'xbflash'.\n";
        return 0;
    }

    int reclock2(unsigned regionIndex, const unsigned short *freq) {
        const unsigned short targetFreqMHz[4] = {freq[0], freq[1], 0, 0};
        return xclReClock2(m_handle, 0, targetFreqMHz);
    }

    int reclockUser(unsigned regionIndex, const unsigned short *freq) {
        const unsigned short targetFreqMHz[4] = {freq[0], freq[1], 0, 0};
        return xclReClockUser(m_handle, 0, targetFreqMHz);
    }

    int getComputeUnits(std::vector<ip_data> &computeUnits) const
    {
        std::string errmsg;
        std::vector<char> buf;

        if(pcidev::get_dev(m_idx)->user)
            pcidev::get_dev(m_idx)->user->sysfs_get("icap", "ip_layout", errmsg, buf);

        if (!errmsg.empty()) {
            std::cout << errmsg << std::endl;
            return -EINVAL;
        }
        if (buf.empty())
            return 0;

        const ip_layout *map = (ip_layout *)buf.data();
        if(map->m_count < 0)
            return -EINVAL;

        for(int i = 0; i < map->m_count; i++)
            computeUnits.emplace_back(map->m_ip_data[i]);
        return 0;
    }

    int parseComputeUnits(const std::vector<ip_data> &computeUnits) const
    {
        for( unsigned int i = 0; i < computeUnits.size(); i++ ) {
            boost::property_tree::ptree ptCu;
            unsigned statusBuf;
            if (computeUnits.at( i ).m_type != IP_KERNEL)
                continue;
            xclRead(m_handle, XCL_ADDR_KERNEL_CTRL, computeUnits.at( i ).m_base_address, &statusBuf, 4);
            ptCu.put( "index",        i );
            ptCu.put( "name",         computeUnits.at( i ).m_name );
            ptCu.put( "base_address", computeUnits.at( i ).m_base_address );
            ptCu.put( "status",       parseCUStatus( statusBuf ) );
            sensor_tree::add_child( "board.compute_unit.cu", ptCu );
        }
        return 0;
    }

    unsigned m_devinfo_power(const xclDeviceInfo2& m_devinfo) const
    {
        unsigned long long power = 0;

        if (m_devinfo.mPexCurr != XCL_INVALID_SENSOR_VAL &&
            m_devinfo.mPexCurr != XCL_NO_SENSOR_DEV_LL &&
            m_devinfo.m12VPex != XCL_INVALID_SENSOR_VAL &&
            m_devinfo.m12VPex != XCL_NO_SENSOR_DEV_S) {
            power = m_devinfo.mPexCurr * m_devinfo.m12VPex +
                m_devinfo.mAuxCurr * m_devinfo.m12VAux;
        }
        power /= 1000000;
        return static_cast<unsigned>(power);
    }

    void m_devinfo_stringize_power(const xclDeviceInfo2& m_devinfo,
        std::vector<std::string> &lines) const
    {
        std::stringstream ss;
        unsigned long long power;
        ss << std::left << "\n";

        ss << std::setw(16) << "Power" << "\n";
        power = m_devinfo.mPexCurr * m_devinfo.m12VPex +
            m_devinfo.mAuxCurr * m_devinfo.m12VAux;
        if(m_devinfo.mPexCurr != XCL_INVALID_SENSOR_VAL &&
            m_devinfo.mPexCurr != XCL_NO_SENSOR_DEV_LL &&
            m_devinfo.m12VPex != XCL_INVALID_SENSOR_VAL &&
            m_devinfo.m12VPex != XCL_NO_SENSOR_DEV_S){
            ss << std::setw(16)
                << std::to_string((float)power / 1000000).substr(0, 4) + "W"
                << "\n";
        } else {
            ss << std::setw(16) << "Not support" << "\n";
        }

        lines.push_back(ss.str());
    }

    void m_mem_usage_bar(xclDeviceUsage &devstat,
        std::vector<std::string> &lines) const
    {
        std::stringstream ss;
        std::string errmsg;
        std::vector<char> buf;
        std::vector<std::string> mm_buf;
        ss << "Device Memory Usage\n";

        pcidev::get_dev(m_idx)->user->sysfs_get(
            "icap", "mem_topology", errmsg, buf);

        if (!errmsg.empty()) {
            ss << errmsg << std::endl;
            lines.push_back(ss.str());
            return;
        }

        const mem_topology *map = (mem_topology *)buf.data();

        if(buf.empty() || map->m_count < 0) {
            ss << "WARNING: 'mem_topology' invalid, unable to report topology. "
                << "Has the bitstream been loaded? See 'xbutil program'.";
            lines.push_back(ss.str());
            return;
        }

        if(map->m_count == 0) {
            ss << "-- none found --. See 'xbutil program'.";
            lines.push_back(ss.str());
            return;
        }

        pcidev::get_dev(m_idx)->user->sysfs_get("", "memstat_raw", errmsg, mm_buf);
        if (!errmsg.empty()) {
            ss << errmsg << std::endl;
            lines.push_back(ss.str());
            return;
        }

        if(mm_buf.empty()){
            ss << "WARNING: 'memstat_raw' invalid, unable to report memory stats. "
                << "Has the bitstream been loaded? See 'xbutil program'.";
            lines.push_back(ss.str());
            return;            
        }
        unsigned numDDR = map->m_count;
        for(unsigned i = 0; i < numDDR; i++ ) {
            if(map->m_mem_data[i].m_type == MEM_STREAMING)
                continue;
            if(!map->m_mem_data[i].m_used)
                continue;
            uint64_t memoryUsage, boCount;
            std::stringstream mem_usage(mm_buf[i]);
            mem_usage >> memoryUsage >> boCount;            

            float percentage = (float)memoryUsage * 100 /
                (map->m_mem_data[i].m_size << 10);
            int nums_fiftieth = (int)percentage / 2;
            std::string str = std::to_string(percentage).substr(0, 4) + "%";

            ss << " [" << i << "] "
                << std::setw(16 - (std::to_string(i).length()) - 4)
                << std::left << map->m_mem_data[i].m_tag;
            ss << "[ " << std::right << std::setw(nums_fiftieth)
                << std::setfill('|') << (nums_fiftieth ? " ":"")
                <<  std::setw(56 - nums_fiftieth);
            ss << std::setfill(' ') << str << " ]" << "\n";
        }

        lines.push_back(ss.str());
    }

    void getMemTopology( const xclDeviceUsage &devstat ) const
    {
        std::string errmsg;
        std::vector<char> buf, temp_buf;
        std::vector<std::string> mm_buf;
        uint64_t memoryUsage, boCount;

        pcidev::get_dev(m_idx)->user->sysfs_get("icap", "mem_topology", errmsg, buf);
        if(pcidev::get_dev(m_idx)->mgmt)
            pcidev::get_dev(m_idx)->mgmt->sysfs_get("xmc", "temp_by_mem_topology", errmsg, temp_buf);
        pcidev::get_dev(m_idx)->user->sysfs_get("", "memstat_raw", errmsg, mm_buf);

        const mem_topology *map = (mem_topology *)buf.data();
        const uint32_t *temp = (uint32_t *)temp_buf.data();

        if(buf.empty() || mm_buf.empty())
            return;

        for(int i = 0; i < map->m_count; i++) {
            std::string str = "**UNUSED**";
            if(map->m_mem_data[i].m_used != 0) {
                auto search = memtype_map.find((MEM_TYPE)map->m_mem_data[i].m_type );
                str = search->second;
            }
            std::stringstream ss(mm_buf[i]);
            ss >> memoryUsage >> boCount;

            boost::property_tree::ptree ptMem;
            ptMem.put( "index",     i );
            ptMem.put( "type",      str );
            ptMem.put( "temp",      temp_buf.empty() ? XCL_NO_SENSOR_DEV : temp[i]);
            ptMem.put( "tag",       map->m_mem_data[i].m_tag );
            ptMem.put( "enabled",   map->m_mem_data[i].m_used ? true : false );
            ptMem.put( "size",      unitConvert(map->m_mem_data[i].m_size << 10) );
            ptMem.put( "mem_usage", unitConvert(memoryUsage));
            ptMem.put( "bo_count",  boCount);
            sensor_tree::add_child( "board.memory.mem", ptMem );
        }
    }

    void m_mem_usage_stringize_dynamics(xclDeviceUsage &devstat,
        const xclDeviceInfo2& m_devinfo, std::vector<std::string> &lines) const
    {
        std::stringstream ss;
        std::string errmsg;
        std::vector<char> buf;
        std::vector<std::string> mm_buf;

        ss << std::left << std::setw(48) << "Mem Topology"
            << std::setw(32) << "Device Memory Usage" << "\n";
        auto dev = pcidev::get_dev(m_idx);
        if(!dev->user){
            ss << "xocl driver is not loaded, skipped" << std::endl;
            lines.push_back(ss.str());
            return;
        }
        pcidev::get_dev(m_idx)->user->sysfs_get(
            "icap", "mem_topology", errmsg, buf);

        if (!errmsg.empty()) {
            ss << errmsg << std::endl;
            lines.push_back(ss.str());
            return;
        }

        const mem_topology *map = (mem_topology *)buf.data();
        unsigned numDDR = 0;

        if(!buf.empty())
            numDDR = map->m_count;

        if(numDDR == 0) {
            ss << "-- none found --. See 'xbutil program'.\n";
        } else if(numDDR < 0) {
            ss << "WARNING: 'mem_topology' invalid, unable to report topology. "
                << "Has the bitstream been loaded? See 'xbutil program'.";
            lines.push_back(ss.str());
            return;
        } else {
            ss << std::setw(16) << "Tag"  << std::setw(12) << "Type"
                << std::setw(12) << "Temp" << std::setw(8) << "Size";
            ss << std::setw(16) << "Mem Usage" << std::setw(8) << "BO nums"
                << "\n";
        }

        pcidev::get_dev(m_idx)->user->sysfs_get("", "memstat_raw", errmsg, mm_buf);
        if(mm_buf.empty())
            return;

        for(unsigned i = 0; i < numDDR; i++) {
            if (map->m_mem_data[i].m_type == MEM_STREAMING)
                continue;
            if (!map->m_mem_data[i].m_used)
                continue;
            ss << " [" << i << "] " <<
                std::setw(16 - (std::to_string(i).length()) - 4) << std::left
                << map->m_mem_data[i].m_tag;

            std::string str;
            if(map->m_mem_data[i].m_used == 0) {
                str = "**UNUSED**";
            } else {
                std::map<MEM_TYPE, std::string> my_map = {
                    {MEM_DDR3, "MEM_DDR3"}, {MEM_DDR4, "MEM_DDR4"},
                    {MEM_DRAM, "MEM_DRAM"}, {MEM_STREAMING, "MEM_STREAMING"},
                    {MEM_PREALLOCATED_GLOB, "MEM_PREALLOCATED_GLOB"},
                    {MEM_ARE, "MEM_ARE"}, {MEM_HBM, "MEM_HBM"},
                    {MEM_BRAM, "MEM_BRAM"}, {MEM_URAM, "MEM_URAM"}
                };
                auto search = my_map.find((MEM_TYPE)map->m_mem_data[i].m_type );
                str = search->second;
            }

            ss << std::left << std::setw(12) << str;
            if (i < sizeof (m_devinfo.mDimmTemp) / sizeof (m_devinfo.mDimmTemp[0]) &&
                m_devinfo.mDimmTemp[i] != XCL_INVALID_SENSOR_VAL &&
                m_devinfo.mDimmTemp[i] != XCL_NO_SENSOR_DEV_S) {
                ss << std::setw(12) << std::to_string(m_devinfo.mDimmTemp[i]) + " C";
            } else {
                ss << std::setw(12) << "Not Supp";
            }
            uint64_t memoryUsage, boCount;
            std::stringstream mem_stat(mm_buf[i]);
            mem_stat >> memoryUsage >> boCount;

            ss << std::setw(8) << unitConvert(map->m_mem_data[i].m_size << 10);
            ss << std::setw(16) << unitConvert(memoryUsage);
            // print size
            ss << std::setw(8) << std::dec << boCount << "\n";
        }

        ss << "\nTotal DMA Transfer Metrics:" << "\n";
        for (unsigned i = 0; i < 2; i++) {
            ss << "  Chan[" << i << "].h2c:  " << unitConvert(devstat.h2c[i]) << "\n";
            ss << "  Chan[" << i << "].c2h:  " << unitConvert(devstat.c2h[i]) << "\n";
        }

        ss << std::setw(80) << std::setfill('#') << std::left << "\n";
        lines.push_back(ss.str());
    }

    /*
     * rewrite this function to place stream info in tree, dump will format the info.
     */
    void m_stream_usage_stringize_dynamics( const xclDeviceInfo2& m_devinfo,
        std::vector<std::string> &lines) const
    {
        std::stringstream ss;
        std::string errmsg;
        std::vector<char> buf;
        std::vector<std::string> attrs;

        ss << std::setfill(' ') << "\n";

        ss << std::left << std::setw(48) << "Stream Topology" << "\n";

        auto dev = pcidev::get_dev(m_idx);
        if(!dev->user){
            ss << "xocl driver is not loaded, skipped" << std::endl;
            lines.push_back(ss.str());
            return;
        }
        pcidev::get_dev(m_idx)->user->sysfs_get("icap", "mem_topology", errmsg, buf);

        if (!errmsg.empty()) {
            ss << errmsg << std::endl;
            lines.push_back(ss.str());
            return;
        }

        const mem_topology *map = (mem_topology *)buf.data();
        unsigned num = 0;

        if(!buf.empty())
            num = map->m_count;

        if(num == 0) {
            ss << "-- none found --. See 'xbutil program'.\n";
        } else if(num < 0) {
            ss << "WARNING: 'mem_topology' invalid, unable to report topology. "
               << "Has the bitstream been loaded? See 'xbutil program'.";
            lines.push_back(ss.str());
            return;
        } else {
            ss << std::setw(16) << "Tag"  << std::setw(8) << "Route"
               << std::setw(5) << "Flow" << std::setw(10) << "Status"
               << std::setw(14) << "Request(B/#)" << std::setw(14) << "Complete(B/#)"
               << std::setw(10) << "Pending(B/#)" << "\n";
        }

        for(unsigned i = 0; i < num; i++) {
            std::string lname;
            bool flag = false;
            std::map<std::string, std::string> stat_map;

            if (map->m_mem_data[i].m_type != MEM_STREAMING)
                continue;

            ss << " [" << i << "] " << std::setw(16 - (std::to_string(i).length()) - 4) << std::left
               << map->m_mem_data[i].m_tag;

            ss << std::setw(8) << map->m_mem_data[i].route_id;
            ss << std::setw(5) << map->m_mem_data[i].flow_id;

            lname = std::string((char *)map->m_mem_data[i].m_tag);
            if (lname.back() == 'w'){
                lname = "route" + std::to_string(map->m_mem_data[i].route_id) + "/stat";
                flag = true;
            }
            else
                lname = "flow" + std::to_string(map->m_mem_data[i].flow_id) + "/stat";
            pcidev::get_dev(m_idx)->user->sysfs_get("str_dma", lname, errmsg, attrs);

            if (!errmsg.empty()) {
                ss << std::setw(10) << "Inactive";
                ss << std::setw(14) << "N/A" << std::setw(14) << "N/A" << std::setw(10) << "N/A";
            } else {
                ss << std::setw(10) << "Active";
                for (unsigned k = 0; k < attrs.size(); k++) {
                    char key[50];
                    int64_t value;

                    std::sscanf(attrs[k].c_str(), "%[^:]:%ld", key, &value);
                    stat_map[std::string(key)] = std::to_string(value);
                }

                ss << std::setw(14) << stat_map[std::string("total_req_bytes")] +

                    "/" + stat_map[std::string("total_req_num")];

                ss << std::setw(14) << stat_map[std::string("total_complete_bytes")] +
                    "/" + stat_map[std::string("total_complete_num")];

                pcidev::get_dev(m_idx)->user->sysfs_get("wq2", lname, errmsg, attrs);
                if (flag) {

                    int write_pending = ((std::stoi(stat_map[std::string("descq_pidx")]) - std::stoi(stat_map[std::string("descq_cidx")])) &
                                        (std::stoi(stat_map[std::string("descq_rngsz")])- 1))*4096;

                    ss << std::setw(10) << std::to_string(write_pending) ;

                }

                pcidev::get_dev(m_idx)->user->sysfs_get("rq2", lname, errmsg, attrs);
                if (!flag) {

                    int read_pending = ((std::stoi(stat_map[std::string("c2h_wrb_pidx")]) - std::stoi(stat_map[std::string("descq_cidx_wrb_pend")])) &
                                       (std::stoi(stat_map[std::string("descq_rngsz")]) - 1))*4096;
                    ss << std::setw(10) << std::to_string(read_pending);

                }
            }
            ss << "\n";

        }


        lines.push_back(ss.str());
    }

    void m_cu_usage_stringize_dynamics(std::vector<std::string>& lines) const
    {
        std::stringstream ss;
        std::string errmsg;
        std::vector<char> buf;

#if 0
        std::vector<ip_data> computeUnits;
        if( getComputeUnits( computeUnits ) < 0 )
            std::cout << "WARNING: 'ip_layout' invalid. Has the bitstream been loaded? See 'xbutil program'.\n";
#endif

        pcidev::get_dev(m_idx)->user->
          sysfs_get("mb_scheduler", "kds_custat", errmsg, buf);

        if (!errmsg.empty()) {
            ss << errmsg << std::endl;
            lines.push_back(ss.str());
            return;
        }

        if (buf.size()) {
          ss << "\nCompute Unit Usage:" << "\n";
          ss << buf.data() << "\n";
        }

        ss << std::setw(80) << std::setfill('#') << std::left << "\n";
        lines.push_back(ss.str());
    }


    int readSensors( void ) const
    {
        sensor_tree::put( "runtime.build.version",   xrt_build_version );
        sensor_tree::put( "runtime.build.hash",      xrt_build_version_hash );
        sensor_tree::put( "runtime.build.date",      xrt_build_version_date );
        sensor_tree::put( "runtime.build.branch",    xrt_build_version_branch );
        // info
        sensor_tree::put( "board.info.dsa_name", m_devinfo.mName );
        sensor_tree::put( "board.info.vendor", m_devinfo.mVendorId );
        sensor_tree::put( "board.info.device", m_devinfo.mDeviceId );
        sensor_tree::put( "board.info.subdevice", m_devinfo.mSubsystemId );
        sensor_tree::put( "board.info.subvendor", m_devinfo.mSubsystemVendorId );
        sensor_tree::put( "board.info.xmcversion", m_devinfo.mXMCVersion );
        sensor_tree::put( "board.info.ddr_size", m_devinfo.mDDRSize );
        sensor_tree::put( "board.info.ddr_count", m_devinfo.mDDRBankCount );
        sensor_tree::put( "board.info.clock0", m_devinfo.mOCLFrequency[0] );
        sensor_tree::put( "board.info.clock1", m_devinfo.mOCLFrequency[1] );
        sensor_tree::put( "board.info.pcie_speed", m_devinfo.mPCIeLinkSpeed );
        sensor_tree::put( "board.info.pcie_width", m_devinfo.mPCIeLinkWidth );
        sensor_tree::put( "board.info.dma_threads", m_devinfo.mDMAThreads );
        sensor_tree::put( "board.info.mig_calibrated", m_devinfo.mMigCalib );
        {
            std::string idcode, fpga, dna, errmsg;
            if(pcidev::get_dev(m_idx)->mgmt){
                pcidev::get_dev(m_idx)->mgmt->sysfs_get("icap", "idcode", errmsg, idcode);
                pcidev::get_dev(m_idx)->mgmt->sysfs_get("rom", "FPGA", errmsg, fpga);
                pcidev::get_dev(m_idx)->mgmt->sysfs_get("dna", "dna", errmsg, dna);
            }
            sensor_tree::put( "board.info.idcode", idcode );
            sensor_tree::put( "board.info.fpga_name", fpga );
            sensor_tree::put( "board.info.dna", dna);
        }


        // physical
        sensor_tree::put( "board.physical.thermal.pcb.top_front",                m_devinfo.mSE98Temp[ 0 ] );
        sensor_tree::put( "board.physical.thermal.pcb.top_rear",                 m_devinfo.mSE98Temp[ 1 ] );
        sensor_tree::put( "board.physical.thermal.pcb.btm_front",                m_devinfo.mSE98Temp[ 2 ] );
        sensor_tree::put( "board.physical.thermal.fpga_temp",                    m_devinfo.mOnChipTemp );
        sensor_tree::put( "board.physical.thermal.tcrit_temp",                   m_devinfo.mFanTemp );
        sensor_tree::put( "board.physical.thermal.fan_speed",                    m_devinfo.mFanRpm );
        {
            unsigned short temp0, temp1, temp2, temp3;
            std::string errmsg;
            pcidev::get_dev(m_idx)->mgmt->sysfs_get("xmc", "xmc_cage_temp0", errmsg, temp0);
            sensor_tree::put( "board.physical.thermal.cage.temp0", temp0);
            pcidev::get_dev(m_idx)->mgmt->sysfs_get("xmc", "xmc_cage_temp1", errmsg, temp1);
            sensor_tree::put( "board.physical.thermal.cage.temp1", temp1);
            pcidev::get_dev(m_idx)->mgmt->sysfs_get("xmc", "xmc_cage_temp2", errmsg, temp2);
            sensor_tree::put( "board.physical.thermal.cage.temp2", temp2);
            pcidev::get_dev(m_idx)->mgmt->sysfs_get("xmc", "xmc_cage_temp3", errmsg, temp3);
            sensor_tree::put( "board.physical.thermal.cage.temp3", temp3);
        }
        sensor_tree::put( "board.physical.electrical.12v_pex.voltage",           m_devinfo.m12VPex );
        sensor_tree::put( "board.physical.electrical.12v_pex.current",           m_devinfo.mPexCurr );
        sensor_tree::put( "board.physical.electrical.12v_aux.voltage",           m_devinfo.m12VAux );
        sensor_tree::put( "board.physical.electrical.12v_aux.current",           m_devinfo.mAuxCurr );
        sensor_tree::put( "board.physical.electrical.3v3_pex.voltage",           m_devinfo.m3v3Pex );
        sensor_tree::put( "board.physical.electrical.3v3_aux.voltage",           m_devinfo.m3v3Aux );
        sensor_tree::put( "board.physical.electrical.ddr_vpp_bottom.voltage",    m_devinfo.mDDRVppBottom );
        sensor_tree::put( "board.physical.electrical.ddr_vpp_top.voltage",       m_devinfo.mDDRVppTop );
        sensor_tree::put( "board.physical.electrical.sys_5v5.voltage",           m_devinfo.mSys5v5 );
        sensor_tree::put( "board.physical.electrical.1v2_top.voltage",           m_devinfo.m1v2Top );
        sensor_tree::put( "board.physical.electrical.1v2_btm.voltage",           m_devinfo.m1v2Bottom );
        sensor_tree::put( "board.physical.electrical.1v8_top.voltage",           m_devinfo.m1v8Top );
        sensor_tree::put( "board.physical.electrical.0v85.voltage",              m_devinfo.m0v85 );
        sensor_tree::put( "board.physical.electrical.mgt_0v9.voltage",           m_devinfo.mMgt0v9 );
        sensor_tree::put( "board.physical.electrical.12v_sw.voltage",            m_devinfo.m12vSW );
        sensor_tree::put( "board.physical.electrical.mgt_vtt.voltage",           m_devinfo.mMgtVtt );
        sensor_tree::put( "board.physical.electrical.vccint.voltage",            m_devinfo.mVccIntVol );
        sensor_tree::put( "board.physical.electrical.vccint.current",            m_devinfo.mVccIntCurr );

        // powerm_devinfo_power
        sensor_tree::put( "board.physical.power", m_devinfo_power(m_devinfo));

        // firewall
        unsigned i = m_errinfo.mFirewallLevel;
        sensor_tree::put( "board.error.firewall.firewall_level", m_errinfo.mFirewallLevel );
        sensor_tree::put( "board.error.firewall.status", parseFirewallStatus( m_errinfo.mAXIErrorStatus[ i ].mErrFirewallStatus ) );

        // memory
        xclDeviceUsage devstat = { 0 };
        (void) xclGetUsageInfo(m_handle, &devstat);
        for (unsigned i = 0; i < 2; i++) {
            boost::property_tree::ptree pt_dma;
            pt_dma.put( "index", i );
            pt_dma.put( "h2c", unitConvert(devstat.h2c[i]) );
            pt_dma.put( "c2h", unitConvert(devstat.c2h[i]) );
            sensor_tree::add_child( "board.pcie_dma.transfer_metrics.chan", pt_dma );
        }
        getMemTopology( devstat );
        // stream

        // xclbin
        std::string errmsg, xclbinid;
        pcidev::get_dev(m_idx)->user->sysfs_get("", "xclbinuuid", errmsg, xclbinid);
        if(errmsg.empty()) {
            sensor_tree::put( "board.xclbin.uuid", xclbinid );
        }

        // compute unit
        std::vector<ip_data> computeUnits;
        if( getComputeUnits( computeUnits ) < 0 ) {
            std::cout << "WARNING: 'ip_layout' invalid. Has the bitstream been loaded? See 'xbutil program'.\n";
        }
        parseComputeUnits( computeUnits );

        /**
         * \note Adding device information for debug and profile
         * This will put one more section debug_profile into the
         * json dump that shows all the device information that
         * debug and profile code in external systems will need
         * e.g. sdx_server, hardware_sercer, GUI, etc
         */
        xclDebugProfileDeviceInfo info;
        int err = xclGetDebugProfileDeviceInfo(m_handle, &info);
        sensor_tree::put("debug_profile.device_info.error", err);
        sensor_tree::put("debug_profile.device_info.device_index", info.device_index);
        sensor_tree::put("debug_profile.device_info.user_instance", info.user_instance);
        sensor_tree::put("debug_profile.device_info.mgmt_instance", info.mgmt_instance);
        sensor_tree::put("debug_profile.device_info.nifd_instance", info.nifd_instance);
        sensor_tree::put("debug_profile.device_info.device_name", std::string(info.device_name));
        sensor_tree::put("debug_profile.device_info.nifd_name", std::string(info.nifd_name));
        /** End of debug and profile device information */

	// p2p enable
	int p2p_enabled;
        pcidev::get_dev(m_idx)->user->sysfs_get("", "p2p_enable", errmsg, p2p_enabled);
	if(errmsg.empty()) {
		sensor_tree::put( "board.info.p2p_enabled", p2p_enabled );
	}
        return 0;
    }

    /*
     * dumpJson
     */
    int dumpJson(std::ostream& ostr) const
    {
        readSensors();
        sensor_tree::json_dump( ostr );
        return 0;
    }

    /*
     * dump
     *
     * TODO: Refactor to make function much shorter.
     */
    int dump(std::ostream& ostr) const {
        readSensors();
        ostr << std::left;
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "XRT\nVersion:    " << sensor_tree::get<std::string>( "runtime.build.version", "N/A" )
             <<    "\nGit Hash:   " << sensor_tree::get<std::string>( "runtime.build.hash", "N/A" )
             <<    "\nGit Branch: " << sensor_tree::get<std::string>( "runtime.build.branch", "N/A" )
             <<    "\nBuild Date: " << sensor_tree::get<std::string>( "runtime.build.date", "N/A" ) << std::endl;
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << std::setw(32) << "DSA" << std::setw(28) << "FPGA" << "IDCode" << std::endl;
        ostr << std::setw(32) << sensor_tree::get<std::string>( "board.info.dsa_name",  "N/A" )
             << std::setw(28) << sensor_tree::get<std::string>( "board.info.fpga_name", "N/A" )
             << sensor_tree::get<std::string>( "board.info.idcode",    "N/A" ) << std::endl;
        ostr << std::setw(16) << "Vendor" << std::setw(16) << "Device" << std::setw(16) << "SubDevice" << std::setw(16) << "SubVendor" << std::endl;
        // get_pretty since we want these as hex
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.info.vendor",    "N/A", true )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.info.device",    "N/A", true )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.info.subdevice", "N/A", true )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.info.subvendor", "N/A", true ) << std::dec << std::endl;
        ostr << std::setw(16) << "DDR size" << std::setw(16) << "DDR count" << std::setw(16) << "Clock0" << std::setw(16) << "Clock1" << std::endl;
        ostr << std::setw(16) << unitConvert(sensor_tree::get<long long>( "board.info.ddr_size", -1 ))
             << std::setw(16) << sensor_tree::get( "board.info.ddr_count", -1 )
             << std::setw(16) << sensor_tree::get( "board.info.clock0", -1 )
             << std::setw(16) << sensor_tree::get( "board.info.clock1", -1 ) << std::endl;
        ostr << std::setw(16) << "PCIe"
             << std::setw(16) << "DMA chan(bidir)"
             << std::setw(16) << "MIG Calibrated"
             << std::setw(16) << "P2P Enabled" << std::endl;
        ostr << "GEN " << sensor_tree::get( "board.info.pcie_speed", -1 ) << "x" << std::setw(10) << sensor_tree::get( "board.info.pcie_width", -1 )
             << std::setw(16) << sensor_tree::get( "board.info.dma_threads", -1 )
             << std::setw(16) << sensor_tree::get<std::string>( "board.info.mig_calibrated", "N/A" );
	switch(sensor_tree::get( "board.info.p2p_enabled", -1)) {
	case -1:
             ostr << std::setw(16) << "N/A" << std::endl;
	     break;
	case 0:
             ostr << std::setw(16) << "false" << std::endl;
	     break;
	case 1:
             ostr << std::setw(16) << "true" << std::endl;
	     break;
	case 2:
             ostr << std::setw(16) << "no iomem" << std::endl;
	     break;
	}
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Temperature(C)\n";
        // use get_pretty for Temperature and Electrical since the driver may rail unsupported values high
        ostr << std::setw(16) << "PCB TOP FRONT" << std::setw(16) << "PCB TOP REAR" << std::setw(16) << "PCB BTM FRONT" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.pcb.top_front" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.pcb.top_rear"  )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.pcb.btm_front" ) << std::endl;
        ostr << std::setw(16) << "FPGA TEMP" << std::setw(16) << "TCRIT Temp" << std::setw(16) << "FAN Speed(RPM)" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.fpga_temp")
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.tcrit_temp")
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.fan_speed" ) << std::endl;
        ostr << std::setw(16) << "QSFP 0" << std::setw(16) << "QSFP 1" << std::setw(16) << "QSFP 2" << std::setw(16) << "QSFP 3" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.cage.temp0" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.cage.temp1" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.cage.temp2" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.cage.temp3" ) << std::endl;
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Electrical(mV|mA)\n";
        ostr << std::setw(16) << "12V PEX" << std::setw(16) << "12V AUX" << std::setw(16) << "12V PEX Current" << std::setw(16) << "12V AUX Current" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.12v_pex.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.12v_aux.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned long long>( "board.physical.electrical.12v_pex.current" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned long long>( "board.physical.electrical.12v_aux.current" ) << std::endl;
        ostr << std::setw(16) << "3V3 PEX" << std::setw(16) << "3V3 AUX" << std::setw(16) << "DDR VPP BOTTOM" << std::setw(16) << "DDR VPP TOP" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.3v3_pex.voltage"        )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.3v3_aux.voltage"        )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.ddr_vpp_bottom.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.ddr_vpp_top.voltage"    ) << std::endl;
        ostr << std::setw(16) << "SYS 5V5" << std::setw(16) << "1V2 TOP" << std::setw(16) << "1V8 TOP" << std::setw(16) << "0V85" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.sys_5v5.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.1v2_top.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.1v8_top.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.0v85.voltage"    ) << std::endl;
        ostr << std::setw(16) << "MGT 0V9" << std::setw(16) << "12V SW" << std::setw(16) << "MGT VTT" << std::setw(16) << "1V2 BTM" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.mgt_0v9.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.12v_sw.voltage"  )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.mgt_vtt.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.1v2_btm.voltage" ) << std::endl;
        ostr << std::setw(16) << "VCCINT VOL" << std::setw(16) << "VCCINT CURR" << std::setw(16) << "DNA" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.vccint.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.vccint.current" )
             << std::setw(16) << sensor_tree::get<std::string>( "board.info.dna", "N/A" ) << std::endl;

        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Board Power\n";
        ostr << sensor_tree::get_pretty<unsigned>( "board.physical.power" ) << " W" << std::endl;
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Firewall Last Error Status\n";
        ostr << " Level " << std::setw(2) << sensor_tree::get( "board.error.firewall.firewall_level", -1 ) << ": 0x0"
             << sensor_tree::get<std::string>( "board.error.firewall.status", "N/A" ) << std::endl;
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << std::left << "Memory Status" << std::endl;
        ostr << std::setw(17) << "     Tag"  << std::setw(12) << "Type"
             << std::setw(9)  << "Temp(C)"   << std::setw(8)  << "Size";
        ostr << std::setw(16) << "Mem Usage" << std::setw(8)  << "BO count" << std::endl;

        try {
          for (auto& v : sensor_tree::get_child("board.memory")) {
            if( v.first == "mem" ) {
              std::string mem_usage, tag, size, type, temp;
              int index = 0;
              unsigned bo_count = 0;
              for (auto& subv : v.second) {
                if( subv.first == "index" ) {
                  index = subv.second.get_value<int>();
                } else if( subv.first == "type" ) {
                  type = subv.second.get_value<std::string>();
                } else if( subv.first == "tag" ) {
                  tag = subv.second.get_value<std::string>();
                } else if( subv.first == "temp" ) {
                  unsigned int t = subv.second.get_value<unsigned int>();
                  temp = sensor_tree::pretty<unsigned int>(t == XCL_INVALID_SENSOR_VAL ? XCL_NO_SENSOR_DEV : t, "N/A");
                } else if( subv.first == "bo_count" ) {
                  bo_count = subv.second.get_value<unsigned>();
                } else if( subv.first == "mem_usage" ) {
                  mem_usage = subv.second.get_value<std::string>();
                } else if( subv.first == "size" ) {
                  size = subv.second.get_value<std::string>();
                }
              }
              ostr << std::left
                   << "[" << std::right << std::setw(2) << index << "] " << std::left
                   << std::setw(12) << tag
                   << std::setw(12) << type
                   << std::setw(9) << temp
                   << std::setw(8) << size
                   << std::setw(16) << mem_usage
                   << std::setw(8) << bo_count << std::endl;
            }
          }
        }
        catch( std::exception const& e) {
          // eat the exception, probably bad path
        }

        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "DMA Transfer Metrics" << std::endl;
        try {
          for (auto& v : sensor_tree::get_child( "board.pcie_dma.transfer_metrics" )) {
            if( v.first == "chan" ) {
              std::string chan_index, chan_h2c, chan_c2h, chan_val = "N/A";
              for (auto& subv : v.second ) {
                chan_val = subv.second.get_value<std::string>();
                if( subv.first == "index" )
                  chan_index = chan_val;
                else if( subv.first == "h2c" )
                  chan_h2c = chan_val;
                else if( subv.first == "c2h" )
                  chan_c2h = chan_val;
              }
              ostr << "Chan[" << chan_index << "].h2c:  " << chan_h2c << std::endl;
              ostr << "Chan[" << chan_index << "].c2h:  " << chan_c2h << std::endl;
            }
          }
        }
        catch( std::exception const& e) {
          // eat the exception, probably bad path
        }
        /* TODO: Stream topology and xclbin id. */
//        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
//        ostr << "Stream Topology, TODO\n";
//        printStreamInfo(ostr);
//        ostr << "#################################\n";
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Xclbin UUID\n"
             << sensor_tree::get<std::string>( "board.xclbin.uuid", "N/A" ) << std::endl;
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Compute Unit Status\n";
        try {
          for (auto& v : sensor_tree::get_child( "board.compute_unit" )) {
            if( v.first == "cu" ) {
              std::string cu_n, cu_s, cu_ba;
              int cu_i = 0;
              for (auto& subv : v.second) {
                if( subv.first == "index" )
                  cu_i = subv.second.get_value<int>();
                else if( subv.first == "name" )
                  cu_n = subv.second.get_value<std::string>();
                else if( subv.first == "base_address" )
                  cu_ba = sensor_tree::pretty<int>(subv.second.get_value<int>(), "N/A", true);
                else if( subv.first == "status" )
                  cu_s = subv.second.get_value<std::string>();
              }
              ostr << "CU[" << std::right << std::setw(2) << cu_i << "]: "
                   << std::left << std::setw(32) << cu_n
                   << "@" << std::setw(18) << std::hex << cu_ba
                   << cu_s << std::endl;
            }
          }
        }
        catch( std::exception const& e) {
            // eat the exception, probably bad path
        }
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        return 0;
    }


    /*
     * print stream topology
     */
    int printStreamInfo(std::ostream& ostr) const {
        std::vector<std::string> usage_lines;
        m_stream_usage_stringize_dynamics(m_devinfo, usage_lines);
        return 0;
    }

    /*
     * program
     */
    int program(const std::string& xclbin, unsigned region) {
        std::ifstream stream(xclbin.c_str());

        if(!stream.is_open()) {
            std::cout << "ERROR: Cannot open " << xclbin << ". Check that it exists and is readable." << std::endl;
            return -ENOENT;
        }

        if(region) {
            std::cout << "ERROR: Not support other than -r 0 " << std::endl;
            return -EINVAL;
        }

        char temp[8];
        stream.read(temp, 8);

        if (std::strncmp(temp, "xclbin0", 8)) {
            if (std::strncmp(temp, "xclbin2", 8))
                return -EINVAL;
        }


        stream.seekg(0, stream.end);
        int length = stream.tellg();
        stream.seekg(0, stream.beg);

        char *buffer = new char[length];
        stream.read(buffer, length);
        const xclBin *header = (const xclBin *)buffer;
        int result = xclLockDevice(m_handle);
        if (result == 0)
		result = xclLoadXclBin(m_handle, header);
        delete [] buffer;
        (void) xclUnlockDevice(m_handle);

        return result;
    }

    /*
     * boot
     *
     * Boot requires root privileges. Boot calls xclBootFPGA given the device handle.
     * The device is closed and a re-enumeration of devices is performed. After, the
     * device is created again by calling xclOpen(). This cannot be done inside
     * xclBootFPGA because of scoping issues in m_handle, so it is done within boot().
     * Check m_handle as a valid pointer before returning.
     */
    int boot() {
        if (getuid() && geteuid()) {
            std::cout << "ERROR: boot operation requires root privileges" << std::endl; // todo move this to a header of common messages
            return -EACCES;
        } else {
            int retVal = -1;
            retVal = xclBootFPGA(m_handle);
            if( retVal == 0 )
            {
                m_handle = xclOpen( m_idx, nullptr, XCL_QUIET );
                ( m_handle != nullptr ) ? retVal = 0 : retVal = -1;
            }
            return retVal;
        }
    }

    int run(unsigned region, unsigned cu) {
        std::cout << "ERROR: Not implemented\n";
        return -1;
    }

    int fan(unsigned speed) {
        std::cout << "ERROR: Not implemented\n";
        return -1;
    }

    /*
     * dmatest
     *
     * TODO: Refactor this function to be much shorter.
     */
    int dmatest(size_t blockSize, bool verbose) {
        if (blockSize == 0)
            blockSize = 256 * 1024 * 1024; // Default block size

        if (verbose)
            std::cout << "Total DDR size: " << m_devinfo.mDDRSize/(1024 * 1024) << " MB\n";

        bool isAREDevice = false;
        if (strstr(m_devinfo.mName, "-xare")) {//This is ARE device
            isAREDevice = true;
        }

        int result = 0;
        unsigned long long addr = 0x0;
        unsigned long long sz = 0x1;
        unsigned int pattern = 'J';

        // get DDR bank count from mem_topology if possible
        std::string errmsg;
        std::vector<char> buf;

        auto dev = pcidev::get_dev(m_idx);
        if(dev->user)
            pcidev::get_dev(m_idx)->user->sysfs_get(
                "icap", "mem_topology", errmsg, buf);

        if (!errmsg.empty()) {
            std::cout << errmsg << std::endl;
            return -EINVAL;
        }
        const mem_topology *map = (mem_topology *)buf.data();

        if(buf.empty() || map->m_count == 0) {
            std::cout << "WARNING: 'mem_topology' invalid, "
                << "unable to perform DMA Test. Has the bitstream been loaded? "
                << "See 'xbutil program'." << std::endl;
            return -EINVAL;
        }

        if (verbose)
            std::cout << "Reporting from mem_topology:" << std::endl;

        for(int32_t i = 0; i < map->m_count; i++) {
            if(map->m_mem_data[i].m_type == MEM_STREAMING)
                continue;

            if(map->m_mem_data[i].m_used) {
                if (verbose) {
                    std::cout << "Data Validity & DMA Test on "
                        << map->m_mem_data[i].m_tag << "\n";
                }
                addr = map->m_mem_data[i].m_base_address;

                for(unsigned sz = 1; sz <= 256; sz *= 2) {
                    result = memwriteQuiet(addr, sz, pattern);
                    if( result < 0 )
                        return result;
                    result = memreadCompare(addr, sz, pattern , false);
                    if( result < 0 )
                        return result;
                }
                DMARunner runner( m_handle, blockSize, i);
                result = runner.run();
            }
        }

        if (isAREDevice) {//This is ARE device
            //XARE Status Reg Base Addr = 0x90000
            //XARE Channel Up Addr is = 0x90010 (& 0x98010)
            // 32 bits = 0x2 means clock is up but channel is down
            // 32 bits = 0x3 mean clocks and channel both are up..
            //??? Sarab: Also check if link channel is up;
            //After that see if we should do one hope or more hops..

            //Raw Read/Write Delay Check
            unsigned numIteration = 10000;
            //addr = 0xC00000000;//48GB = 3 hops
            addr = 0x400000000;//16GB = one hop
            sz = 0x20000;//128KB
            long numHops = addr / m_devinfo.mDDRSize;
            auto t1 = Clock::now();
            for (unsigned i = 0; i < numIteration; i++) {
                memwriteQuiet(addr, sz, pattern);
            }
            auto t2 = Clock::now();
            auto timeARE = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();

            addr = 0x0;
            sz = 0x1;
            t1 = Clock::now();
            for (unsigned i = 0; i < numIteration; i++) {
                memwriteQuiet(addr, sz, pattern);
            }
            t2 = Clock::now();
            auto timeDDR = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
            long delayPerHop = (timeARE - timeDDR) / (numIteration * numHops);
            std::cout << "Averaging ARE hardware latency over " << numIteration * numHops << " hops\n";
            std::cout << "Latency per ARE hop for 128KB: " << delayPerHop << " ns\n";
            std::cout << "Total latency over ARE: " << (timeARE - timeDDR) << " ns\n";
        }
        return result;
    }

    int memread(std::string aFilename, unsigned long long aStartAddr = 0, unsigned long long aSize = 0) {
        std::ios_base::fmtflags f(std::cout.flags());
        if (strstr(m_devinfo.mName, "-xare")) {//This is ARE device
          if (aStartAddr > m_devinfo.mDDRSize) {
              std::cout << "Start address " << std::hex << aStartAddr <<
                           " is over ARE" << std::endl;
          }
          if (aSize > m_devinfo.mDDRSize || aStartAddr+aSize > m_devinfo.mDDRSize) {
              std::cout << "Read size " << std::dec << aSize << " from address 0x" << std::hex << aStartAddr <<
                           " is over ARE" << std::endl;
          }
        }
        std::cout.flags(f);

        return memaccess(m_handle, m_devinfo.mDDRSize, m_devinfo.mDataAlignment,
            pcidev::get_dev(m_idx)->user->sysfs_name).read(
            aFilename, aStartAddr, aSize);
    }


    int memDMATest(size_t blocksize, unsigned int aPattern = 'J') {
        return memaccess(m_handle, m_devinfo.mDDRSize, m_devinfo.mDataAlignment,
            pcidev::get_dev(m_idx)->user->sysfs_name).runDMATest(
            blocksize, aPattern);
    }

    int memreadCompare(unsigned long long aStartAddr = 0, unsigned long long aSize = 0, unsigned int aPattern = 'J', bool checks = true) {
        return memaccess(m_handle, m_devinfo.mDDRSize, m_devinfo.mDataAlignment,
            pcidev::get_dev(m_idx)->user->sysfs_name).readCompare(
            aStartAddr, aSize, aPattern, checks);
    }

    int memwrite(unsigned long long aStartAddr, unsigned long long aSize, unsigned int aPattern = 'J') {
        std::ios_base::fmtflags f(std::cout.flags());
        if (strstr(m_devinfo.mName, "-xare")) {//This is ARE device
            if (aStartAddr > m_devinfo.mDDRSize) {
                std::cout << "Start address " << std::hex << aStartAddr <<
                             " is over ARE" << std::endl;
            }
            if (aSize > m_devinfo.mDDRSize || aStartAddr+aSize > m_devinfo.mDDRSize) {
                std::cout << "Write size " << std::dec << aSize << " from address 0x" << std::hex << aStartAddr <<
                             " is over ARE" << std::endl;
            }
        }
        std::cout.flags(f);
        return memaccess(m_handle, m_devinfo.mDDRSize, m_devinfo.mDataAlignment,
            pcidev::get_dev(m_idx)->user->sysfs_name).write(
            aStartAddr, aSize, aPattern);
    }

    int memwrite( unsigned long long aStartAddr, unsigned long long aSize, char *srcBuf )
    {
        std::ios_base::fmtflags f(std::cout.flags());
        if( strstr( m_devinfo.mName, "-xare" ) ) { //This is ARE device
            if( aStartAddr > m_devinfo.mDDRSize ) {
                std::cout << "Start address " << std::hex << aStartAddr <<
                             " is over ARE" << std::endl;
            }
            if( aSize > m_devinfo.mDDRSize || aStartAddr + aSize > m_devinfo.mDDRSize ) {
                std::cout << "Write size " << std::dec << aSize << " from address 0x" << std::hex << aStartAddr <<
                             " is over ARE" << std::endl;
            }
        }
        std::cout.flags(f);
        return memaccess(m_handle, m_devinfo.mDDRSize, m_devinfo.mDataAlignment,
            pcidev::get_dev(m_idx)->user->sysfs_name).write(
            aStartAddr, aSize, srcBuf);
    }

    int memwriteQuiet(unsigned long long aStartAddr, unsigned long long aSize, unsigned int aPattern = 'J') {
        return memaccess(m_handle, m_devinfo.mDDRSize, m_devinfo.mDataAlignment,
            pcidev::get_dev(m_idx)->user->sysfs_name).writeQuiet(
            aStartAddr, aSize, aPattern);
    }


   //Debug related functionality.
    uint32_t getIPCountAddrNames(int type, std::vector<uint64_t> *baseAddress, std::vector<std::string> * portNames);

    std::pair<size_t, size_t> getCUNamePortName (std::vector<std::string>& aSlotNames,
                             std::vector< std::pair<std::string, std::string> >& aCUNamePortNames);
    int readSPMCounters();
    int readSSPMCounters();
    int readLAPCheckers(int aVerbose);
    int print_debug_ip_list (int aVerbose);

    /*
     * do_dd
     *
     * Perform block read or writes to-device-from-file or from-device-to-file.
     *
     * Usage:
     * dd -d0 --if=in.txt --bs=4096 --count=16 --seek=10
     * dd -d0 --of=out.txt --bs=1024 --count=4 --skip=2
     * --if : specify the input file, if specified, direction is fileToDevice
     * --of : specify the output file, if specified, direction is deviceToFile
     * --bs : specify the block size OPTIONAL defaults to value specified in 'dd.h'
     * --count : specify the number of blocks to copy
     *           OPTIONAL for fileToDevice; will copy the remainder of input file by default
     *           REQUIRED for deviceToFile
     * --skip : specify the source offset (in block counts) OPTIONAL defaults to 0
     * --seek : specify the destination offset (in block counts) OPTIONAL defaults to 0
     */
    int do_dd(dd::ddArgs_t args )
    {
        if( !args.isValid ) {
            return -1; // invalid arguments
        }
        if( args.dir == dd::unset ) {
            return -1; // direction invalid
        } else if( args.dir == dd::deviceToFile ) {
            unsigned long long addr = args.skip; // ddr read offset
            while( args.count-- > 0 ) { // writes all full blocks
                memread( args.file, addr, args.blockSize ); // returns 0 on complete read.
                // how to check for partial reads when device is empty?
                addr += args.blockSize;
            }
        } else if( args.dir == dd::fileToDevice ) {
            // write entire contents of file to device DDR at seek offset.
            unsigned long long addr = args.seek; // ddr write offset
            std::ifstream iStream( args.file.c_str(), std::ifstream::binary );
            if( !iStream ) {
                perror( "open input file" );
                return errno;
            }
            // If unspecified count, calculate the count from the full file size.
            if( args.count <= 0 ) {
                iStream.seekg( 0, iStream.end );
                int length = iStream.tellg();
                args.count = length / args.blockSize + 1; // round up
                iStream.seekg( 0, iStream.beg );
            }
            iStream.seekg( 0, iStream.beg );

            char *buf;
            static char *inBuf;
            size_t inSize;

            inBuf = (char*)malloc( args.blockSize );
            if( !inBuf ) {
                perror( "malloc block size" );
                return errno;
            }

            while( args.count-- > 0 ) { // writes all full blocks
                buf = inBuf;
                inSize = iStream.read( inBuf, args.blockSize ).gcount();
                if( (int)inSize == args.blockSize ) {
                    // full read
                } else {
                    // Partial read--write size specified greater than read size. Writing remainder of input file.
                    args.count = 0; // force break
                }
                memwrite( addr, inSize, buf );
                addr += inSize;
            }
            iStream.close();
        }
        return 0;
    }

    int usageInfo(xclDeviceUsage& devstat) const {
        return xclGetUsageInfo(m_handle, &devstat);
    }

    int deviceInfo(xclDeviceInfo2& devinfo) const {
        return xclGetDeviceInfo2(m_handle, &devinfo);
    }

    int validate(bool quick);

    int printEccInfo(std::ostream& ostr) const;
    int resetEccInfo();
    int reset(xclResetKind kind);
    int setP2p(bool enable, bool force);
    int testP2p();

private:
    // Run a test case as <exe> <xclbin> [-d index] on this device and collect
    // all output from the run into "output"
    // Note: exe should assume index to be 0 without -d
    int runTestCase(const std::string& exe, const std::string& xclbin,
        std::string& output);
};

void printHelp(const std::string& exe);
int xclTop(int argc, char *argv[]);
int xclReset(int argc, char *argv[]);
int xclValidate(int argc, char *argv[]);
std::unique_ptr<xcldev::device> xclGetDevice(unsigned index);
int xclP2p(int argc, char *argv[]);
} // end namespace xcldev

#endif /* XBUTIL_H */
