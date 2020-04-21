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
#define GB(x)           ((size_t) (x) << 30)

#include <fstream>
#include <assert.h>
#include <vector>
#include <map>
#include <iomanip>
#include <sstream>
#include <string>
#include <boost/property_tree/json_parser.hpp>

#include "xrt.h"
#include "ert.h"
#include "xclperf.h"
#include "xcl_axi_checker_codes.h"
#include "core/pcie/common/dmatest.h"
#include "core/pcie/common/memaccess.h"
#include "core/pcie/common/dd.h"
#include "core/common/utils.h"
#include "core/common/sensor.h"
#include "core/pcie/linux/scan.h"
#include "core/pcie/linux/shim.h"
#include "xclbin.h"
#include "core/common/xrt_profiling.h"
#include <version.h>

#include <chrono>
using Clock = std::chrono::high_resolution_clock;


/* exposed by shim */
int xclUpdateSchedulerStat(xclDeviceHandle);
int xclCmaEnable(xclDeviceHandle handle, bool enable, uint64_t total_size);
int xclGetDebugProfileDeviceInfo(xclDeviceHandle handle, xclDebugProfileDeviceInfo* info);

#define TO_STRING(x) #x
#define AXI_FIREWALL

#define XCL_NO_SENSOR_DEV_LL    ~(0ULL)
#define XCL_NO_SENSOR_DEV       ~(0U)
#define XCL_NO_SENSOR_DEV_S     0xffff
#define XCL_INVALID_SENSOR_VAL 0

#define indent(level)   std::string((level) * 4, ' ')
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
    M2MTEST,
    VERSION
};
enum subcommand {
    MEM_READ = 0,
    MEM_WRITE,
    STATUS_AIM,
    STATUS_LAPC,
    STATUS_ASM,
    STATUS_SPC,
    STREAM,
    STATUS_UNSUPPORTED,
    STATUS_AM,
};
enum statusmask {
    STATUS_NONE_MASK = 0x0,
    STATUS_AIM_MASK = 0x1,
    STATUS_LAPC_MASK = 0x2,
    STATUS_ASM_MASK = 0x4,
    STATUS_SPC_MASK = 0x8,
    STATUS_AM_MASK = 0x10,
};
enum p2pcommand {
    P2P_ENABLE = 0x0,
    P2P_DISABLE,
    P2P_VALIDATE,
};
enum cmacommand {
    CMA_ENABLE = 0x0,
    CMA_DISABLE,
    CMA_VALIDATE,
    CMA_SIZE,
};

static const std::pair<std::string, command> map_pairs[] = {
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
    std::make_pair("m2mtest", M2MTEST),
    std::make_pair("version", VERSION),
    std::make_pair("--version", VERSION)

};

static const std::pair<std::string, subcommand> subcmd_pairs[] = {
    std::make_pair("read", MEM_READ),
    std::make_pair("write", MEM_WRITE),
    std::make_pair("aim", STATUS_AIM),
    std::make_pair("lapc", STATUS_LAPC),
    std::make_pair("asm", STATUS_ASM),
    std::make_pair("stream", STREAM),
    std::make_pair("accelmonitor", STATUS_AM)
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
    {MEM_URAM, "MEM_URAM"},
    {MEM_STREAMING_CONNECTION, "MEM_STREAMING_CONNECTION"}
};

static const std::map<int, std::string> oemid_map = {
    {0x10da, "Xilinx"},
    {0x02a2, "Dell"},
    {0x12a1, "IBM"},
    {0xb85c, "HP"},
    {0x2a7c, "Super Micro"},
    {0x4a66, "Lenovo"},
    {0xbd80, "Inspur"},
    {0x12eb, "Amazon"},
    {0x2b79, "Google"}
};

static const std::string getOEMID(std::string oemid)
{
    unsigned int oemIDValue = 0;
    std::stringstream ss;

    try {
        ss << std::hex << oemid;
        ss >> oemIDValue;
    } catch (const std::exception&) {
        //failed to parse oemid to hex value, ignore erros and print origin value
    }

    ss.str(std::string());
    ss.clear();
    auto oemstr = oemid_map.find(oemIDValue);

    ss << oemid << "(" << (oemstr != oemid_map.end() ? oemstr->second : "N/A") << ")";

    return ss.str();
}

static const std::map<std::string, command> commandTable(map_pairs, map_pairs + sizeof(map_pairs) / sizeof(map_pairs[0]));

static std::string lvl2PowerStr(unsigned int lvl)
{
    std::vector<std::string> powers{ "75W", "150W", "225W" };

    if (lvl < powers.size())
        return powers[lvl];

    return "0W";
}

class device {
    unsigned int m_idx;
    xclDeviceHandle m_handle;
    std::string m_devicename;

    struct xclbin_lock
    {
        xclDeviceHandle m_handle;
        uuid_t m_uuid;
        xclbin_lock(xclDeviceHandle handle, unsigned int m_idx) : m_handle(handle) {
            std::string errmsg, xclbinid;

            pcidev::get_dev(m_idx)->sysfs_get("", "xclbinuuid", errmsg, xclbinid);

            if (!errmsg.empty()) {
                std::cout<<errmsg<<std::endl;
                throw std::runtime_error("Failed to get uuid.");
            }

            uuid_parse(xclbinid.c_str(), m_uuid);

            if (uuid_is_null(m_uuid))
                   throw std::runtime_error("'uuid' invalid, please re-program xclbin.");

            if (xclOpenContext(m_handle, m_uuid, -1, true))
                   throw std::runtime_error("'Failed to lock down xclbin");
        }
        ~xclbin_lock(){
            xclCloseContext(m_handle, m_uuid, -1);
        }
    };


public:
    int domain() {
        return pcidev::get_dev(m_idx)->domain;
    }
    int bus() {
        return pcidev::get_dev(m_idx)->bus;
    }
    int dev() {
        return pcidev::get_dev(m_idx)->dev;
    }
    int userFunc() {
        return pcidev::get_dev(m_idx)->func;
    }
    device(unsigned int idx, const char* log) : m_idx(idx), m_handle(nullptr),
        m_devicename(""){
        std::string devstr = "device[" + std::to_string(m_idx) + "]";
        m_handle = xclOpen(m_idx, nullptr, XCL_QUIET);
        if (!m_handle)
            throw std::runtime_error("Failed to open " + devstr);

        std::string errmsg;
        pcidev::get_dev(m_idx)->sysfs_get("rom", "VBNV", errmsg, m_devicename);
        if(!errmsg.empty())
            throw std::runtime_error("Failed to determine device name. ");
    }

    device(device&& rhs) = delete;
    device(const device &dev) = delete;
    device& operator=(const device &dev) = delete;

    ~device() {
        xclClose(m_handle);
    }

    std::string name() const {
        return m_devicename;
    }

    void
    schedulerUpdateStat() const
    {
        try {
          xclbin_lock lk(m_handle, m_idx);
          xclUpdateSchedulerStat(m_handle);
        }
        catch (const std::exception&) {
          // xclbin_lock failed, safe to ignore
        }
    }


    int reclock2(unsigned regionIndex, const unsigned short *freq) {
        const unsigned short targetFreqMHz[4] = {freq[0], freq[1], freq[2], 0};
        uuid_t uuid;
        int ret;

        ret = getXclbinuuid(uuid);
        if (ret)
            return ret;

        return xclReClock2(m_handle, 0, targetFreqMHz);
    }

    int getComputeUnits(std::vector<ip_data> &computeUnits) const
    {
        std::string errmsg;
        std::vector<char> buf;

        pcidev::get_dev(m_idx)->sysfs_get("icap", "ip_layout", errmsg, buf);

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

    uint32_t parseComputeUnitStatus(const std::vector<std::string>& custat, uint32_t offset) const
    {
       if (custat.empty())
          return 0;

       for (auto& line : custat) {
           uint32_t ba = 0, cnt = 0, sta = 0;
           std::sscanf(line.c_str(), "CU[@0x%x] : %d status : %d", &ba, &cnt, &sta);

           if (offset != ba)
               continue;

           return sta;
       }

       return 0;
    }

    int parseComputeUnits(const std::vector<ip_data> &computeUnits) const
    {
        if (!std::getenv("XCL_SKIP_CU_READ"))
          schedulerUpdateStat();

        std::vector<std::string> custat;
        std::string errmsg;
        pcidev::get_dev(m_idx)->sysfs_get("mb_scheduler", "kds_custat", errmsg, custat);

        for (unsigned int i = 0; i < computeUnits.size(); ++i) {
            const auto& ip = computeUnits[i];
            if (ip.m_type != IP_KERNEL)
                continue;
            uint32_t status = parseComputeUnitStatus(custat,ip.m_base_address);
            boost::property_tree::ptree ptCu;
            ptCu.put( "name",         ip.m_name );
            ptCu.put( "base_address", ip.m_base_address );
            ptCu.put( "status",       xrt_core::utils::parse_cu_status( status ) );
            sensor_tree::add_child( std::string("board.compute_unit." + std::to_string(i)), ptCu );
        }
        return 0;
    }

    float sysfs_power() const
    {
        unsigned long long power = 0;
        std::string errmsg;

        pcidev::get_dev(m_idx)->sysfs_get<unsigned long long>( "xmc", "xmc_power",  errmsg, power, 0);

        if (!errmsg.empty()) {
            return -1;
        }

        return (float)power / 1000000;
    }

    void sysfs_stringize_power(std::vector<std::string> &lines) const
    {
        std::stringstream ss;
        ss << std::left << "\n";
        ss << std::setw(16) << "Power" << "\n";

        ss << sensor_tree::get_pretty<unsigned>( "board.physical.power" ) << + "W" << "\n";

        lines.push_back(ss.str());
    }

    void m_mem_usage_bar(xclDeviceUsage &devstat,
        std::vector<std::string> &lines) const
    {
        std::stringstream ss;
        ss << "Device Memory Usage\n";

        try {
          for (auto& v : sensor_tree::get_child("board.memory.mem")) {
            int index = std::stoi(v.first);
            if( index >= 0 ) {
              uint64_t size = 0, mem_usage = 0;
              std::string tag, type, temp;
              bool enabled = false;

              for (auto& subv : v.second) {
                  if( subv.first == "type" ) {
                      type = subv.second.get_value<std::string>();
                  } else if( subv.first == "tag" ) {
                      tag = subv.second.get_value<std::string>();
                  } else if( subv.first == "temp" ) {
                      unsigned int t = subv.second.get_value<unsigned int>();
                      temp = sensor_tree::pretty<unsigned int>(t == XCL_INVALID_SENSOR_VAL ? XCL_NO_SENSOR_DEV : t, "N/A");
                  } else if( subv.first == "mem_usage_raw" ) {
                      mem_usage = subv.second.get_value<uint64_t>();
                  } else if( subv.first == "size_raw" ) {
                      size = subv.second.get_value<uint64_t>();
                  } else if( subv.first == "enabled" ) {
                      enabled = subv.second.get_value<bool>();
                  }
              }
              if (!enabled || !size)
                continue;

              float percentage = (float)mem_usage * 100 /
                    (size << 10);
              int nums_fiftieth = (int)percentage / 2;
              std::string str = std::to_string(percentage).substr(0, 4) + "%";

              ss << " [" << index << "] "
                 << std::setw(16 - (std::to_string(index).length()) - 4)
                 << std::left << tag
                 << "[ " << std::right << std::setw(nums_fiftieth)
                 << std::setfill('|') << (nums_fiftieth ? " ":"")
                 <<  std::setw(56 - nums_fiftieth)
                 << std::setfill(' ') << str << " ]" << std::endl;

            }
          }
        } catch( std::exception const& e) {
            ss << "WARNING: Unable to report memory stats. "
               << "Has the bitstream been loaded? See 'xbutil program'.";
        }

        lines.push_back(ss.str());
    }

    static int eccStatus2Str(unsigned int status, std::string& str)
    {
        const int ce_mask = (0x1 << 1);
        const int ue_mask = (0x1 << 0);

        str.clear();

        // If unknown status bits, can't support.
        if (status & ~(ce_mask | ue_mask)) {
            std::cout << "Bad ECC status detected!" << std::endl;
            return -EINVAL;
        }

        if (status == 0) {
            str = "(None)";
            return 0;
        }

        if (status & ue_mask)
            str += "UE ";
        if (status & ce_mask)
            str += "CE ";
        // Remove the trailing space.
        str.pop_back();
        return 0;
    }

    void getMemTopology( const xclDeviceUsage &devstat ) const
    {
        std::string errmsg;
        std::vector<char> buf, temp_buf;
        std::vector<std::string> mm_buf, stream_stat;
        uint64_t memoryUsage, boCount, memBankSize;
        auto dev = pcidev::get_dev(m_idx);

        dev->sysfs_get("icap", "mem_topology", errmsg, buf);
        dev->sysfs_get("", "memstat_raw", errmsg, mm_buf);
        dev->sysfs_get("xmc", "temp_by_mem_topology", errmsg, temp_buf);

        const mem_topology *map = (mem_topology *)buf.data();
        const uint32_t *temp = (uint32_t *)temp_buf.data();

        if(buf.empty() || mm_buf.empty())
            return;

        int j = 0; // stream index
        int m = 0; // mem index

        dev->sysfs_put( "", "mig_cache_update", errmsg, "1");
        for(int i = 0; i < map->m_count; i++) {
            if (map->m_mem_data[i].m_type == MEM_STREAMING || map->m_mem_data[i].m_type == MEM_STREAMING_CONNECTION) {
                std::string lname, status = "Inactive", total = "N/A", pending = "N/A";
                boost::property_tree::ptree ptStream;
                std::map<std::string, std::string> stat_map;
                lname = std::string((char *)map->m_mem_data[i].m_tag);
                if (lname.back() == 'w')
                    lname = "route" + std::to_string(map->m_mem_data[i].route_id) + "/stat";
                else if (lname.back() == 'r')
                    lname = "flow" + std::to_string(map->m_mem_data[i].flow_id) + "/stat";
                else
                    status = "N/A";

                dev->sysfs_get("dma", lname, errmsg, stream_stat);
                if (errmsg.empty()) {
                    status = "Active";
                    for (unsigned k = 0; k < stream_stat.size(); k++) {
                        char key[50];
                        int64_t value;

                        std::sscanf(stream_stat[k].c_str(), "%[^:]:%ld", key, &value);
                        stat_map[std::string(key)] = std::to_string(value);
                    }

                    total = stat_map[std::string("complete_bytes")] + "/" + stat_map[std::string("complete_requests")];
                    pending = stat_map[std::string("pending_bytes")] + "/" + stat_map[std::string("pending_requests")];
                }

                ptStream.put( "tag", map->m_mem_data[i].m_tag );
                ptStream.put( "flow_id", map->m_mem_data[i].flow_id );
                ptStream.put( "route_id", map->m_mem_data[i].route_id );
                ptStream.put( "status", status );
                ptStream.put( "total", total );
                ptStream.put( "pending", pending );
                sensor_tree::add_child( std::string("board.memory.stream." + std::to_string(j)), ptStream);
                j++;
                continue;
            }

            boost::property_tree::ptree ptMem;

            std::string str = "**UNUSED**";
            if(map->m_mem_data[i].m_used != 0) {
                auto search = memtype_map.find((MEM_TYPE)map->m_mem_data[i].m_type );
                str = search->second;
                unsigned ecc_st;
                std::string ecc_st_str;
                std::string tag(reinterpret_cast<const char *>(map->m_mem_data[i].m_tag));
                dev->sysfs_get<unsigned>(tag, "ecc_status", errmsg, ecc_st, 0);
                if (errmsg.empty() && eccStatus2Str(ecc_st, ecc_st_str) == 0) {
                    unsigned ce_cnt = 0;
                    dev->sysfs_get<unsigned>(tag, "ecc_ce_cnt", errmsg, ce_cnt, 0);
                    unsigned ue_cnt = 0;
                    dev->sysfs_get<unsigned>(tag, "ecc_ue_cnt", errmsg, ue_cnt, 0);
                    uint64_t ce_ffa = 0;
                    dev->sysfs_get<uint64_t>(tag, "ecc_ce_ffa", errmsg, ce_ffa, 0);
                    uint64_t ue_ffa = 0;
                    dev->sysfs_get<uint64_t>(tag, "ecc_ue_ffa", errmsg, ue_ffa, 0);

                    ptMem.put("ecc_status", ecc_st_str);
                    ptMem.put("ecc_ce_cnt", ce_cnt);
                    ptMem.put("ecc_ue_cnt", ue_cnt);
                    ptMem.put("ecc_ce_ffa", ce_ffa);
                    ptMem.put("ecc_ue_ffa", ue_ffa);
                }
            }
            std::stringstream ss(mm_buf[i]);
            ss >> memoryUsage >> boCount;

            ptMem.put( "type",      str );
            ptMem.put( "temp",      temp_buf.empty() ? XCL_NO_SENSOR_DEV : temp[i]);
            ptMem.put( "tag",       map->m_mem_data[i].m_tag );
            ptMem.put( "enabled",   map->m_mem_data[i].m_used ? true : false );
            ptMem.put( "size",      xrt_core::utils::unit_convert(map->m_mem_data[i].m_size << 10) );
            ptMem.put( "size_raw",  map->m_mem_data[i].m_size << 10 );
            ptMem.put( "mem_usage", xrt_core::utils::unit_convert(memoryUsage));
            ptMem.put( "mem_usage_raw", memoryUsage);
            ptMem.put( "bo_count",  boCount);
            sensor_tree::add_child( std::string("board.memory.mem." + std::to_string(m)), ptMem );
            m++;
        }

        boost::property_tree::ptree ptMem;

        std::string str = "MEM_HOST";

        std::stringstream ss(mm_buf[m]);
        ss >> memoryUsage >> boCount >> memBankSize;

        bool enabled = memBankSize ? true : false;

        if (enabled) {
            ptMem.put( "type",      str );
            ptMem.put( "temp",      XCL_INVALID_SENSOR_VAL);
            ptMem.put( "tag",       "CMA_BANK" );
            ptMem.put( "enabled",   memBankSize ? true : false);
            ptMem.put( "size",      xrt_core::utils::unit_convert(memBankSize));
            ptMem.put( "mem_usage", xrt_core::utils::unit_convert(memoryUsage));
            ptMem.put( "bo_count",  boCount);
            sensor_tree::add_child( std::string("board.memory.mem." + std::to_string(m)), ptMem );
        }
    }

    void m_mem_usage_stringize_dynamics(xclDeviceUsage &devstat, std::vector<std::string> &lines) const
    {
        std::stringstream ss;

        ss << std::left << std::setw(48) << "Mem Topology"
            << std::setw(32) << "Device Memory Usage" << "\n";
        auto dev = pcidev::get_dev(m_idx);
        if(!dev){
            ss << "xocl driver is not loaded, skipped" << std::endl;
            lines.push_back(ss.str());
            return;
        }

        try {
           ss << std::setw(17) << "Tag"  << std::setw(12) << "Type"
              << std::setw(9) << "Temp" << std::setw(10) << "Size";
           ss << std::setw(16) << "Mem Usage" << std::setw(8) << "BO nums"
              << "\n";
          for (auto& v : sensor_tree::get_child("board.memory.mem")) {
            int index = std::stoi(v.first);
            if( index >= 0 ) {
              std::string mem_usage, tag, size, type, temp;
              unsigned bo_count = 0;
              bool enabled = false;
              for (auto& subv : v.second) {
                  if( subv.first == "type" ) {
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
                  } else if( subv.first == "enabled" ) {
                      enabled = subv.second.get_value<bool>();
                  }
              }
              if (!enabled)
                continue;

              ss   << " [" << std::right << index << "] "
                   << std::setw(17 - (std::to_string(index).length()) - 4)
                   << std::left << tag
                   << std::setw(12) << type
                   << std::setw(9) << temp
                   << std::setw(10) << size
                   << std::setw(16) << mem_usage
                   << std::setw(8) << bo_count << std::endl;
            }
          }
        } catch( std::exception const& e) {
            ss << "WARNING: Unable to report memory stats. "
               << "Has the bitstream been loaded? See 'xbutil program'.";
        }

        ss << "\nTotal DMA Transfer Metrics:" << "\n";
        for (unsigned i = 0; i < 2; i++) {
            ss << "  Chan[" << i << "].h2c:  " << xrt_core::utils::unit_convert(devstat.h2c[i]) << "\n";
            ss << "  Chan[" << i << "].c2h:  " << xrt_core::utils::unit_convert(devstat.c2h[i]) << "\n";
        }

        ss << std::setw(80) << std::setfill('#') << std::left << "\n";
        lines.push_back(ss.str());
    }

    /*
     * rewrite this function to place stream info in tree, dump will format the info.
     */
    void m_stream_usage_stringize_dynamics(std::vector<std::string> &lines) const
    {
    }

    void m_cu_usage_stringize_dynamics(std::vector<std::string>& lines) const
    {
        std::stringstream ss;

        ss << "\nCompute Unit Usage:" << "\n";

        try {
          for (auto& v : sensor_tree::get_child( "board.compute_unit" )) {
            int index = std::stoi(v.first);
            if( index >= 0 ) {
              std::string cu_s, cu_ba;
              for (auto& subv : v.second) {
                if( subv.first == "base_address" ) {
                  auto addr = subv.second.get_value<uint64_t>();
                  cu_ba = (addr == (uint64_t)-1) ? "N/A" : sensor_tree::pretty<uint64_t>(addr, "N/A", true);
                } else if( subv.first == "status" ) {
                  cu_s = subv.second.get_value<std::string>();
                }
              }

              ss << "CU[@" << std::hex << cu_ba
                   << "] : "<<cu_s << std::endl;
            }
          }
        }
        catch( std::exception const& e) {
            // eat the exception, probably bad path
        }

        ss << std::setw(80) << std::setfill('#') << std::left << "\n";
        lines.push_back(ss.str());
    }

    void clearSensorTree( void ) const
    {
        sensor_tree::clear();
    }

    int readSensors( void ) const
    {
        // board info
        std::string vendor, device, subsystem, subvendor, xmc_ver, xmc_oem_id,
            ser_num, bmc_ver, idcode, fpga, dna, errmsg, max_power;
        int ddr_size = 0, ddr_count = 0, pcie_speed = 0, pcie_width = 0, p2p_enabled = 0;
        std::vector<std::string> clock_freqs;
        std::vector<std::string> dma_threads;
        std::vector<std::string> mac_addrs;
        bool mig_calibration;

        clock_freqs.resize(3);
        mac_addrs.resize(4);
        pcidev::get_dev(m_idx)->sysfs_get( "", "vendor",                     errmsg, vendor );
        pcidev::get_dev(m_idx)->sysfs_get( "", "device",                     errmsg, device );
        pcidev::get_dev(m_idx)->sysfs_get( "", "subsystem_device",           errmsg, subsystem );
        pcidev::get_dev(m_idx)->sysfs_get( "", "subsystem_vendor",           errmsg, subvendor );
        pcidev::get_dev(m_idx)->sysfs_get( "xmc", "version",                 errmsg, xmc_ver );
        pcidev::get_dev(m_idx)->sysfs_get( "xmc", "xmc_oem_id",              errmsg, xmc_oem_id );
        pcidev::get_dev(m_idx)->sysfs_get( "xmc", "serial_num",              errmsg, ser_num );
        pcidev::get_dev(m_idx)->sysfs_get( "xmc", "max_power",               errmsg, max_power );
        pcidev::get_dev(m_idx)->sysfs_get( "xmc", "bmc_ver",                 errmsg, bmc_ver );
        pcidev::get_dev(m_idx)->sysfs_get( "xmc", "mac_addr0",               errmsg, mac_addrs[0] );
        pcidev::get_dev(m_idx)->sysfs_get( "xmc", "mac_addr1",               errmsg, mac_addrs[1] );
        pcidev::get_dev(m_idx)->sysfs_get( "xmc", "mac_addr2",               errmsg, mac_addrs[2] );
        pcidev::get_dev(m_idx)->sysfs_get( "xmc", "mac_addr3",               errmsg, mac_addrs[3] );
        pcidev::get_dev(m_idx)->sysfs_get<int>("rom", "ddr_bank_size",       errmsg, ddr_size,  0 );
        pcidev::get_dev(m_idx)->sysfs_get<int>( "rom", "ddr_bank_count_max", errmsg, ddr_count, 0 );
        pcidev::get_dev(m_idx)->sysfs_get( "icap", "clock_freqs",            errmsg, clock_freqs );
        pcidev::get_dev(m_idx)->sysfs_get( "dma", "channel_stat_raw",        errmsg, dma_threads );
        pcidev::get_dev(m_idx)->sysfs_get<int>( "", "link_speed",            errmsg, pcie_speed, 0 );
        pcidev::get_dev(m_idx)->sysfs_get<int>( "", "link_width",            errmsg, pcie_width, 0 );
        pcidev::get_dev(m_idx)->sysfs_get<bool>( "", "mig_calibration",      errmsg, mig_calibration, false );
        pcidev::get_dev(m_idx)->sysfs_get( "rom", "FPGA",                    errmsg, fpga );
        pcidev::get_dev(m_idx)->sysfs_get( "icap", "idcode",                 errmsg, idcode );
        pcidev::get_dev(m_idx)->sysfs_get( "dna", "dna",                     errmsg, dna );
        pcidev::get_dev(m_idx)->sysfs_get<int>("", "p2p_enable",             errmsg, p2p_enabled, 0 );
        sensor_tree::put( "board.info.dsa_name",       name() );
        sensor_tree::put( "board.info.vendor",         vendor );
        sensor_tree::put( "board.info.device",         device );
        sensor_tree::put( "board.info.subdevice",      subsystem );
        sensor_tree::put( "board.info.subvendor",      subvendor );
        sensor_tree::put( "board.info.xmcversion",     xmc_ver );
        sensor_tree::put( "board.info.xmc_oem_id",     getOEMID(xmc_oem_id));
        sensor_tree::put( "board.info.serial_number",  ser_num );
        sensor_tree::put( "board.info.max_power",      lvl2PowerStr(max_power.empty() ? UINT_MAX : stoi(max_power)) );
        sensor_tree::put( "board.info.sc_version",     bmc_ver );
        sensor_tree::put( "board.info.ddr_size",       GB(ddr_size)*ddr_count );
        sensor_tree::put( "board.info.ddr_count",      ddr_count );
        sensor_tree::put( "board.info.clock0",         clock_freqs[0] );
        sensor_tree::put( "board.info.clock1",         clock_freqs[1] );
        sensor_tree::put( "board.info.clock2",         clock_freqs[2] );
        sensor_tree::put( "board.info.pcie_speed",     pcie_speed );
        sensor_tree::put( "board.info.pcie_width",     pcie_width );
        sensor_tree::put( "board.info.dma_threads",    dma_threads.size() );
        sensor_tree::put( "board.info.mig_calibrated", mig_calibration );
        sensor_tree::put( "board.info.idcode",         idcode );
        sensor_tree::put( "board.info.fpga_name",      fpga );
        sensor_tree::put( "board.info.dna",            dna );
        sensor_tree::put( "board.info.p2p_enabled",    p2p_enabled );

        for (uint32_t i = 0; i < mac_addrs.size(); ++i) {
            std::string entry_name = "board.info.mac_addr."+std::to_string(i);

            if (mac_addrs[i].empty())
                continue;

            sensor_tree::put( entry_name,     mac_addrs[i]);
        }
        //interface uuid
        std::vector<std::string> interface_uuid;
        pcidev::get_dev(m_idx)->sysfs_get( "", "interface_uuids", errmsg, interface_uuid );
        for (unsigned i =0; i < interface_uuid.size(); i++) {
            sensor_tree::put( "board.interface_uuid.uuid" + std::to_string(i), interface_uuid[i] );
        }

        //logic uuid
        std::vector<std::string> logic_uuid;
        pcidev::get_dev(m_idx)->sysfs_get( "", "logic_uuids", errmsg, logic_uuid );
        for (unsigned i =0; i < logic_uuid.size(); i++) {
            sensor_tree::put( "board.logic_uuid.uuid" + std::to_string(i), logic_uuid[i] );
        }

        // physical.thermal.pcb
        unsigned int xmc_se98_temp0, xmc_se98_temp1, xmc_se98_temp2;
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_se98_temp0", xmc_se98_temp0 );
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_se98_temp1", xmc_se98_temp1 );
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_se98_temp2", xmc_se98_temp2 );
        sensor_tree::put( "board.physical.thermal.pcb.top_front", xmc_se98_temp0);
        sensor_tree::put( "board.physical.thermal.pcb.top_rear",  xmc_se98_temp1);
        sensor_tree::put( "board.physical.thermal.pcb.btm_front", xmc_se98_temp2);

        // physical.thermal
        unsigned int fan_rpm, xmc_fpga_temp, xmc_fan_temp, vccint_temp, xmc_hbm_temp;
        std::string fan_presence;

        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_fpga_temp", xmc_fpga_temp );
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_fan_temp",  xmc_fan_temp );
        pcidev::get_dev(m_idx)->sysfs_get( "xmc", "fan_presence",         errmsg, fan_presence );
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_fan_rpm",   fan_rpm );
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_vccint_temp",  vccint_temp);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_hbm_temp",  xmc_hbm_temp);
        sensor_tree::put( "board.physical.thermal.fpga_temp",    xmc_fpga_temp );
        sensor_tree::put( "board.physical.thermal.tcrit_temp",   xmc_fan_temp );
        sensor_tree::put( "board.physical.thermal.fan_presence", fan_presence );
        sensor_tree::put( "board.physical.thermal.fan_speed",    fan_rpm );
        sensor_tree::put( "board.physical.thermal.vccint_temp",  vccint_temp);
        sensor_tree::put( "board.physical.thermal.hbm_temp",     xmc_hbm_temp);

        // physical.thermal.cage
        unsigned int temp0, temp1, temp2, temp3;
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_cage_temp0", temp0);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_cage_temp1", temp1);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_cage_temp2", temp2);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_cage_temp3", temp3);
        sensor_tree::put( "board.physical.thermal.cage.temp0", temp0);
        sensor_tree::put( "board.physical.thermal.cage.temp1", temp1);
        sensor_tree::put( "board.physical.thermal.cage.temp2", temp2);
        sensor_tree::put( "board.physical.thermal.cage.temp3", temp3);

        //electrical
        unsigned int m3v3_pex_vol, m3v3_aux_vol, ddr_vpp_btm, ddr_vpp_top, sys_5v5, m1v2_top, m1v2_btm, m1v8,
                     m0v85, mgt0v9avcc, m12v_sw, mgtavtt, vccint_vol, vccint_curr, m3v3_pex_curr, m0v85_curr, m3v3_vcc_vol,
                     hbm_1v2_vol, vpp2v5_vol, vccint_bram_vol, m12v_pex_vol, m12v_aux_curr, m12v_pex_curr, m12v_aux_vol,
                     vol_12v_aux1, vol_vcc1v2_i, vol_v12_in_i, vol_v12_in_aux0_i, vol_v12_in_aux1_i, vol_vccaux,
                     vol_vccaux_pmc, vol_vccram;
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_12v_pex_vol",    m12v_pex_vol);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_12v_pex_curr",   m12v_pex_curr);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_12v_aux_vol",    m12v_aux_vol);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_12v_aux_curr",   m12v_aux_curr);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_3v3_pex_vol",    m3v3_pex_vol);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_3v3_aux_vol",    m3v3_aux_vol);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_ddr_vpp_btm",    ddr_vpp_btm);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_ddr_vpp_top",    ddr_vpp_top);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_sys_5v5",        sys_5v5);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_1v2_top",        m1v2_top);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_vcc1v2_btm",     m1v2_btm);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_1v8",            m1v8);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_0v85",           m0v85);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_mgt0v9avcc",     mgt0v9avcc);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_12v_sw",         m12v_sw);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_mgtavtt",        mgtavtt);
        pcidev::get_dev(m_idx)->sysfs_get_sensor( "xmc", "xmc_vccint_vol",     vccint_vol);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_vccint_curr",     vccint_curr);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_3v3_pex_curr",    m3v3_pex_curr);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_0v85_curr",       m0v85_curr);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_3v3_vcc_vol",     m3v3_vcc_vol);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_hbm_1v2_vol",     hbm_1v2_vol);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_vpp2v5_vol",      vpp2v5_vol);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_vccint_bram_vol", vccint_bram_vol);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_12v_aux1",        vol_12v_aux1);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_vcc1v2_i",        vol_vcc1v2_i);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_v12_in_i",        vol_v12_in_i);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_v12_in_aux0_i",   vol_v12_in_aux0_i);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_v12_in_aux1_i",   vol_v12_in_aux1_i);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_vccaux",          vol_vccaux);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_vccaux_pmc",      vol_vccaux_pmc);
        pcidev::get_dev(m_idx)->sysfs_get_sensor("xmc", "xmc_vccram",          vol_vccram);
        sensor_tree::put( "board.physical.electrical.12v_pex.voltage",         m12v_pex_vol );
        sensor_tree::put( "board.physical.electrical.12v_pex.current",         m12v_pex_curr );
        sensor_tree::put( "board.physical.electrical.12v_aux.voltage",         m12v_aux_vol );
        sensor_tree::put( "board.physical.electrical.12v_aux.current",         m12v_aux_curr );
        sensor_tree::put( "board.physical.electrical.3v3_pex.voltage",         m3v3_pex_vol );
        sensor_tree::put( "board.physical.electrical.3v3_aux.voltage",         m3v3_aux_vol );
        sensor_tree::put( "board.physical.electrical.ddr_vpp_bottom.voltage",  ddr_vpp_btm );
        sensor_tree::put( "board.physical.electrical.ddr_vpp_top.voltage",     ddr_vpp_top );
        sensor_tree::put( "board.physical.electrical.sys_5v5.voltage",         sys_5v5 );
        sensor_tree::put( "board.physical.electrical.1v2_top.voltage",         m1v2_top );
        sensor_tree::put( "board.physical.electrical.1v2_btm.voltage",         m1v2_btm );
        sensor_tree::put( "board.physical.electrical.1v8.voltage",             m1v8 );
        sensor_tree::put( "board.physical.electrical.0v85.voltage",            m0v85 );
        sensor_tree::put( "board.physical.electrical.mgt_0v9.voltage",         mgt0v9avcc );
        sensor_tree::put( "board.physical.electrical.12v_sw.voltage",          m12v_sw );
        sensor_tree::put( "board.physical.electrical.mgt_vtt.voltage",         mgtavtt );
        sensor_tree::put( "board.physical.electrical.vccint.voltage",          vccint_vol );
        sensor_tree::put( "board.physical.electrical.vccint.current",          vccint_curr);
        sensor_tree::put( "board.physical.electrical.3v3_pex.current",         m3v3_pex_curr);
        sensor_tree::put( "board.physical.electrical.0v85.current",            m0v85_curr);
        sensor_tree::put( "board.physical.electrical.vcc3v3.voltage",          m3v3_vcc_vol);
        sensor_tree::put( "board.physical.electrical.hbm_1v2.voltage",         hbm_1v2_vol);
        sensor_tree::put( "board.physical.electrical.vpp2v5.voltage",          vpp2v5_vol);
        sensor_tree::put( "board.physical.electrical.vccint_bram.voltage",     vccint_bram_vol);
        sensor_tree::put( "board.physical.electrical.12v_aux1.current",        vol_12v_aux1);
        sensor_tree::put( "board.physical.electrical.vcc1v2_i.current",        vol_vcc1v2_i);
        sensor_tree::put( "board.physical.electrical.v12_in_i.current",        vol_v12_in_i);
        sensor_tree::put( "board.physical.electrical.v12_in_aux0_i.current",   vol_v12_in_aux0_i);
        sensor_tree::put( "board.physical.electrical.v12_in_aux1_i.current",   vol_v12_in_aux1_i);
        sensor_tree::put( "board.physical.electrical.vccaux.current",          vol_vccaux);
        sensor_tree::put( "board.physical.electrical.vccaux_pmc.current",      vol_vccaux_pmc);
        sensor_tree::put( "board.physical.electrical.vccram.current",          vol_vccram);

        // physical.power
        sensor_tree::put( "board.physical.power", static_cast<unsigned>(sysfs_power()));

        // firewall
        unsigned int level = 0, status = 0;
        unsigned long long time = 0;
        pcidev::get_dev(m_idx)->sysfs_get<unsigned int>( "firewall", "detected_level",      errmsg, level, 0 );
        pcidev::get_dev(m_idx)->sysfs_get<unsigned int>( "firewall", "detected_status",     errmsg, status, 0 );
        pcidev::get_dev(m_idx)->sysfs_get<unsigned long long>( "firewall", "detected_time", errmsg, time, 0 );
        sensor_tree::put( "board.error.firewall.firewall_level", level );
        sensor_tree::put( "board.error.firewall.firewall_status", status );
        sensor_tree::put( "board.error.firewall.firewall_time", time );
        sensor_tree::put( "board.error.firewall.status", xrt_core::utils::parse_firewall_status(status) );

        // memory
        xclDeviceUsage devstat = { 0 };
        (void) xclGetUsageInfo(m_handle, &devstat);
        for (unsigned i = 0; i < 2; i++) {
            boost::property_tree::ptree pt_dma;
            pt_dma.put( "h2c", xrt_core::utils::unit_convert(devstat.h2c[i]) );
            pt_dma.put( "c2h", xrt_core::utils::unit_convert(devstat.c2h[i]) );
            sensor_tree::add_child( std::string("board.pcie_dma.transfer_metrics.chan." + std::to_string(i)), pt_dma );
        }

        getMemTopology( devstat );

        // xclbin
        std::string xclbinid;
        pcidev::get_dev(m_idx)->sysfs_get("", "xclbinuuid", errmsg, xclbinid);
        sensor_tree::put( "board.xclbin.uuid", xclbinid );

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
        sensor_tree::put("debug_profile.device_info.device_name", std::string(info.device_name));
        /** End of debug and profile device information */

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

    void printTree (std::ostream& ostr, boost::property_tree::ptree &pt, int level) const
    {
        if (pt.empty()) {
            ostr << ": " << pt.data() << std::endl;
        } else {
            if (level > 0)
                ostr << std::endl;
            for (auto pos = pt.begin(); pos != pt.end();) {
                std::cout << indent(level+1) << pos->first;
                printTree(ostr, pos->second, level + 1);
                ++pos;
            }
        }
        return;
    }

    int dumpPartitionInfo(std::ostream& ostr) const
    {
        std::vector<std::string> partinfo;
        pcidev::get_dev(m_idx)->get_partinfo(partinfo);

        for (unsigned int i = 0; i < partinfo.size(); i++)
        {
            auto info = partinfo[i];
            if (info.empty())
                continue;
            boost::property_tree::ptree ptInfo;
            std::istringstream is(info);
            boost::property_tree::read_json(is, ptInfo);
            ostr << "Partition Info:" << std::endl;
            printTree(ostr, ptInfo, 0);
            if (i != partinfo.size() - 1)
                ostr << std::endl;
        }
	if (partinfo.size())
            ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        return 0;
    }

    /*
     * dump
     *
     * TODO: Refactor to make function much shorter.
     */
    int dump(std::ostream& ostr) const {
        readSensors();
        std::ios::fmtflags f( ostr.flags() );
        ostr << std::left << std::endl;
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << std::setw(32) << "Shell" << std::setw(32) << "FPGA" << "IDCode" << std::endl;
        ostr << std::setw(32) << sensor_tree::get<std::string>( "board.info.dsa_name",  "N/A" )
             << std::setw(32) << sensor_tree::get<std::string>( "board.info.fpga_name", "N/A" )
             << sensor_tree::get<std::string>( "board.info.idcode",    "N/A" ) << std::endl;
        ostr << std::setw(16) << "Vendor" << std::setw(16) << "Device" << std::setw(16) << "SubDevice"
             << std::setw(16) << "SubVendor" << std::setw(16) << "SerNum" << std::endl;
        ostr << std::setw(16) << sensor_tree::get<std::string>( "board.info.vendor",    "N/A" )
             << std::setw(16) << sensor_tree::get<std::string>( "board.info.device",    "N/A" )
             << std::setw(16) << sensor_tree::get<std::string>( "board.info.subdevice", "N/A" )
             << std::setw(16) << sensor_tree::get<std::string>( "board.info.subvendor", "N/A" )
             << std::setw(16) << sensor_tree::get<std::string>( "board.info.serial_number", "N/A" ) << std::endl;
        ostr << std::setw(16) << "DDR size" << std::setw(16) << "DDR count" << std::setw(16)
             << "Clock0" << std::setw(16) << "Clock1" << std::setw(16) << "Clock2" << std::endl;
        ostr << std::setw(16) << xrt_core::utils::unit_convert(sensor_tree::get<long long>( "board.info.ddr_size", -1 ))
             << std::setw(16) << sensor_tree::get( "board.info.ddr_count", -1 )
             << std::setw(16) << sensor_tree::get( "board.info.clock0", -1 )
             << std::setw(16) << sensor_tree::get( "board.info.clock1", -1 )
             << std::setw(16) << sensor_tree::get( "board.info.clock2", -1 ) << std::endl;
        ostr << std::setw(16) << "PCIe"
             << std::setw(16) << "DMA chan(bidir)"
             << std::setw(16) << "MIG Calibrated"
             << std::setw(16) << "P2P Enabled"
	     << std::setw(16) << "OEM ID" << std::endl;
        ostr << "GEN " << sensor_tree::get( "board.info.pcie_speed", -1 ) << "x" << std::setw(10)
             << sensor_tree::get( "board.info.pcie_width", -1 ) << std::setw(16) << sensor_tree::get( "board.info.dma_threads", -1 )
             << std::setw(16) << sensor_tree::get<std::string>( "board.info.mig_calibrated", "N/A" );
             switch(sensor_tree::get( "board.info.p2p_enabled", -1)) {
             case ENXIO:
                      ostr << std::setw(16) << "N/A";
                  break;
             case 0:
                      ostr << std::setw(16) << "false";
                  break;
             case 1:
                      ostr << std::setw(16) << "true";
                  break;
             case EBUSY:
                      ostr << std::setw(16) << "no iomem";
                  break;
             }
        ostr << std::setw(16) << sensor_tree::get<std::string>( "board.info.xmc_oem_id" , "N/A") << std::endl;

	std::vector<std::string> interface_uuids;
	std::vector<std::string> logic_uuids;
	std::string errmsg;
        pcidev::get_dev(m_idx)->sysfs_get( "", "interface_uuids", errmsg, interface_uuids);
        if (interface_uuids.size())
        {
            ostr << "Interface UUID" << std::endl;
            for (auto uuid : interface_uuids)
            {
                ostr << uuid;
            }
            ostr << std::endl;
        }

        pcidev::get_dev(m_idx)->sysfs_get( "", "logic_uuids", errmsg, logic_uuids);
        if (logic_uuids.size())
        {
            ostr << "Logic UUID" << std::endl;
            for (auto uuid : logic_uuids)
            {
                ostr << uuid;
            }
            ostr << std::endl;
        }
        ostr << "DNA" << std::endl;
        ostr << sensor_tree::get<std::string>( "board.info.dna", "N/A" ) << std::endl;


        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Temperature(C)\n";
        ostr << std::setw(16) << "PCB TOP FRONT" << std::setw(16) << "PCB TOP REAR" << std::setw(16) << "PCB BTM FRONT" << std::setw(16) << "VCCINT TEMP" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.thermal.pcb.top_front" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.thermal.pcb.top_rear"  )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.thermal.pcb.btm_front" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.thermal.vccint_temp" ) << std::endl;
        ostr << std::setw(16) << "FPGA TEMP" << std::setw(16) << "TCRIT Temp" << std::setw(16) << "FAN Presence"
             << std::setw(16) << "FAN Speed(RPM)" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.thermal.fpga_temp")
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.thermal.tcrit_temp")
             << std::setw(16) << sensor_tree::get<std::string>( "board.physical.thermal.fan_presence")
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.thermal.fan_speed" ) << std::endl;
        ostr << std::setw(16) << "QSFP 0" << std::setw(16) << "QSFP 1" << std::setw(16) << "QSFP 2" << std::setw(16) << "QSFP 3"
             << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.thermal.cage.temp0" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.thermal.cage.temp1" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.thermal.cage.temp2" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.thermal.cage.temp3" ) << std::endl;
        ostr << std::setw(16) << "HBM TEMP" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.thermal.hbm_temp") << std::endl;
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Electrical(mV|mA)\n";
        ostr << std::setw(16) << "12V PEX" << std::setw(16) << "12V AUX" << std::setw(16) << "12V PEX Current" << std::setw(16)
             << "12V AUX Current" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.12v_pex.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.12v_aux.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.12v_pex.current" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.12v_aux.current" ) << std::endl;
        ostr << std::setw(16) << "3V3 PEX" << std::setw(16) << "3V3 AUX" << std::setw(16) << "DDR VPP BOTTOM" << std::setw(16)
             << "DDR VPP TOP" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.3v3_pex.voltage"        )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.3v3_aux.voltage"        )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.ddr_vpp_bottom.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.ddr_vpp_top.voltage"    ) << std::endl;
        ostr << std::setw(16) << "SYS 5V5" << std::setw(16) << "1V2 TOP" << std::setw(16) << "1V8 TOP" << std::setw(16)
             << "0V85" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.sys_5v5.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.1v2_top.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.1v8.voltage"     )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.0v85.voltage"    ) << std::endl;
        ostr << std::setw(16) << "MGT 0V9" << std::setw(16) << "12V SW" << std::setw(16) << "MGT VTT"
             << std::setw(16) << "1V2 BTM" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.mgt_0v9.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.12v_sw.voltage"  )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.mgt_vtt.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.1v2_btm.voltage" ) << std::endl;
        ostr << std::setw(16) << "VCCINT VOL" << std::setw(16) << "VCCINT CURR" << std::setw(16) << "VCCINT BRAM VOL" << std::setw(16) << "VCC3V3 VOL"  << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.vccint.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.vccint.current" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.vccint_bram.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.vcc3v3.voltage"  ) << std::endl;
        ostr << std::setw(16) << "3V3 PEX CURR" << std::setw(16) << "VCC0V85 CURR" << std::setw(16) << "HBM1V2 VOL" << std::setw(16) << "VPP2V5 VOL"  << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.3v3_pex.current" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.0v85.current" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.hbm_1v2.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.vpp2v5.voltage"  ) << std::endl;
        ostr << std::setw(16) << "VCC1V2 CURR" << std::setw(16) << "V12 I CURR" << std::setw(16) << "V12 AUX0 CURR" << std::setw(16) << "V12 AUX1 CURR"  << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.vcc1v2_i.current" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.v12_in_i.current" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.v12_in_aux0_i.current" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.v12_in_aux1_i.current"  ) << std::endl;
        ostr << std::setw(16) << "12V AUX1 CURR" << std::setw(16) << "VCCAUX CURR" << std::setw(16) << "VCCAUX PMC CURR" << std::setw(16) << "VCCRAM CURR"  << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.12v_aux1.current" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.vccaux.current" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.vccaux_pmc.current" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned int>( "board.physical.electrical.vccram.current"  ) << std::endl;

        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Card Power(W)\n";
        ostr << sensor_tree::get_pretty<unsigned>( "board.physical.power" ) << std::endl;
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Firewall Last Error Status\n";
        unsigned short lvl = sensor_tree::get( "board.error.firewall.firewall_level", 0 );
        ostr << "Level " << std::setw(2) << lvl << ": 0x"
             << std::hex << sensor_tree::get( "board.error.firewall.firewall_status", -1 ) << std::dec
             << sensor_tree::get<std::string>( "board.error.firewall.status", "N/A" ) << std::endl;
        if (lvl != 0) {
            char cbuf[80];
            time_t stamp = static_cast<time_t>(sensor_tree::get( "board.error.firewall.firewall_time", 0 ));
            struct tm *ts = localtime(&stamp);
            strftime(cbuf, sizeof(cbuf), "%a %Y-%m-%d %H:%M:%S %Z", ts);
            ostr << "Error occurred on: " << cbuf << std::endl;
        }
        ostr << std::endl;
        ostr << "ECC Error Status\n";
        ostr << std::left << std::setw(8) << "Tag" << std::setw(12) << "Errors"
             << std::setw(10) << "CE Count" << std::setw(10) << "UE Count"
             << std::setw(20) << "CE FFA" << std::setw(20) << "UE FFA" << std::endl;
        try {
          for (auto& v : sensor_tree::get_child("board.memory.mem")) {
            int index = std::stoi(v.first);
            if( index >= 0 ) {
              std::string tag, st;
              unsigned int ce_cnt = 0, ue_cnt = 0;
              uint64_t ce_ffa = 0, ue_ffa = 0;
              for (auto& subv : v.second) {
                  if( subv.first == "tag" ) {
                      tag = subv.second.get_value<std::string>();
                  } else if( subv.first == "ecc_status" ) {
                      st = subv.second.get_value<std::string>();
                  } else if( subv.first == "ecc_ce_cnt" ) {
                      ce_cnt = subv.second.get_value<unsigned int>();
                  } else if( subv.first == "ecc_ue_cnt" ) {
                      ue_cnt = subv.second.get_value<unsigned int>();
                  } else if( subv.first == "ecc_ce_ffa" ) {
                      ce_ffa = subv.second.get_value<uint64_t>();
                  } else if( subv.first == "ecc_ue_ffa" ) {
                      ue_ffa = subv.second.get_value<uint64_t>();
                  }
              }
              if (!st.empty()) {
                  ostr << std::left << std::setw(8) << tag << std::setw(12)
                    << st << std::dec << std::setw(10) << ce_cnt
                    << std::setw(10) << ue_cnt << "0x" << std::setw(18)
                    << std::hex << ce_ffa << "0x" << std::setw(18) << ue_ffa
                    << std::endl;
              }
            }
          }
        }
        catch( std::exception const& e) {
          // eat the exception, probably bad path
        }

        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << std::left << "Memory Status" << std::endl;
        ostr << std::setw(17) << "     Tag"  << std::setw(12) << "Type"
             << std::setw(9)  << "Temp(C)"   << std::setw(8)  << "Size";
        ostr << std::setw(16) << "Mem Usage" << std::setw(8)  << "BO count" << std::endl;

        try {
          for (auto& v : sensor_tree::get_child("board.memory.mem")) {
            int index = std::stoi(v.first);
            if( index >= 0 ) {
              std::string mem_usage, tag, size, type, temp;
              unsigned bo_count = 0;
              for (auto& subv : v.second) {
                  if( subv.first == "type" ) {
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

        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "DMA Transfer Metrics" << std::endl;
        try {
          for (auto& v : sensor_tree::get_child( "board.pcie_dma.transfer_metrics.chan" )) {
            int index = std::stoi(v.first);
            if( index >= 0 ) {
              std::string chan_h2c, chan_c2h, chan_val = "N/A";
              for (auto& subv : v.second ) {
                chan_val = subv.second.get_value<std::string>();
                if( subv.first == "h2c" )
                  chan_h2c = chan_val;
                else if( subv.first == "c2h" )
                  chan_c2h = chan_val;
              }
              ostr << "Chan[" << index << "].h2c:  " << chan_h2c << std::endl;
              ostr << "Chan[" << index << "].c2h:  " << chan_c2h << std::endl;
            }
          }
        }
        catch( std::exception const& e) {
          // eat the exception, probably bad path
        }

        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Streams" << std::endl;
        ostr << std::setw(17) << "     Tag"  << std::setw(9) << "Flow ID"
             << std::setw(9)  << "Route ID"   << std::setw(9)  << "Status";
        ostr << std::setw(16) << "Total (B/#)" << std::setw(10)  << "Pending (B/#)" << std::endl;
        try {
          int index = 0;
          for (auto& v : sensor_tree::get_child("board.memory.stream")) {
            int stream_index = std::stoi(v.first);
            if( stream_index >= 0 ) {
              std::string status, tag, total, pending;
              unsigned int flow_id = 0, route_id = 0;
              for (auto& subv : v.second) {
                if( subv.first == "tag" ) {
                  tag = subv.second.get_value<std::string>();
                } else if( subv.first == "flow_id" ) {
                  flow_id = subv.second.get_value<unsigned int>();
                } else if( subv.first == "route_id" ) {
                  route_id = subv.second.get_value<unsigned int>();
                } else if ( subv.first == "status" ) {
                  status = subv.second.get_value<std::string>();
                } else if ( subv.first == "total" ) {
                  total = subv.second.get_value<std::string>();
                } else if ( subv.first == "pending" ) {
                  pending = subv.second.get_value<std::string>();
                }
              }
              ostr << std::left
                   << "[" << std::right << std::setw(2) << index << "] " << std::left
                   << std::setw(12) << tag
                   << std::setw(9) << flow_id
                   << std::setw(9)  << route_id
                   << std::setw(9)  << status
                   << std::setw(16) << total
                   << std::setw(10) << pending << std::endl;
              index++;
            }
          }
        }
        catch( std::exception const& e) {
          // eat the exception, probably bad path
        }

        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Xclbin UUID\n"
             << sensor_tree::get<std::string>( "board.xclbin.uuid", "N/A" ) << std::endl;
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Compute Unit Status\n";
        try {
          for (auto& v : sensor_tree::get_child( "board.compute_unit" )) {
            int index = std::stoi(v.first);
            if( index >= 0 ) {
              uint32_t cu_i;
              std::string cu_n, cu_s, cu_ba;
              for (auto& subv : v.second) {
                if( subv.first == "name" ) {
                  cu_n = subv.second.get_value<std::string>();
                } else if( subv.first == "base_address" ) {
                  auto addr = subv.second.get_value<uint64_t>();
                  cu_ba = (addr == (uint64_t)-1) ? "N/A" : sensor_tree::pretty<uint64_t>(addr, "N/A", true);
                } else if( subv.first == "status" ) {
                  cu_s = subv.second.get_value<std::string>();
                }
              }
              if (xclIPName2Index(m_handle, cu_n.c_str(), &cu_i) != 0) {
                ostr << "CU: ";
              } else {
                ostr << "CU[" << std::right << std::setw(2) << cu_i << "]: ";
              }
              ostr << std::left << std::setw(32) << cu_n
                   << "@" << std::setw(18) << std::hex << cu_ba
                   << cu_s << std::endl;
            }
          }
        }
        catch( std::exception const& e) {
            // eat the exception, probably bad path
        }
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        dumpPartitionInfo(ostr);
        ostr.flags(f);
        return 0;
    }

    /*
     * print stream topology
     */
    int printStreamInfo(std::ostream& ostr) const {
        std::vector<std::string> lines;
        m_stream_usage_stringize_dynamics(lines);
        for(auto line:lines) {
            ostr << line.c_str() << std::endl;
        }

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
        xclbin_lock xclbin_lock(m_handle, m_idx);

        if (blockSize == 0)
            blockSize = 256 * 1024 * 1024; // Default block size

        int ddr_mem_size = get_ddr_mem_size();
        if (ddr_mem_size == -EINVAL)
            return -EINVAL;

        int result = 0;
        unsigned long long addr = 0x0;
        unsigned int pattern = 'J';

        // get DDR bank count from mem_topology if possible
        std::vector<char> buf;
        std::string errmsg;

        auto dev = pcidev::get_dev(m_idx);
        dev->sysfs_get("icap", "mem_topology", errmsg, buf);

        if (!errmsg.empty()) {
            std::cout << errmsg << std::endl;
            return -EINVAL;
        }
        const mem_topology *map = (mem_topology *)buf.data();

        std::string hbm_mem_size = xrt_core::utils::unit_convert(map->m_count*(map->m_mem_data[0].m_size << 10));
        if (verbose) {
            std::cout << "INFO: DMA test on [" << m_idx << "]: "<< name() << "\n";
            if (hbm_mem_size.compare(std::string("0 Byte")) != 0)
                std::cout << "Total HBM size: " << hbm_mem_size << "\n";
            if (ddr_mem_size != 0)
                std::cout << "Total DDR size: " << ddr_mem_size << " MB\n";

            if (blockSize < (1024*1024))
                std::cout << "Buffer Size: " << blockSize/(1024) << " KB\n";
            else
                std::cout << "Buffer Size: " << blockSize/(1024*1024) << " MB\n";
        }

        if(buf.empty() || map->m_count == 0) {
            std::cout << "WARNING: 'mem_topology' invalid, "
                << "unable to perform DMA Test. Has the bitstream been loaded? "
                << "See 'xbutil program' to load a specific xclbin file or run "
                << "'xbutil validate' to use the xclbins provided with this card." << std::endl;
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
                try {
                    DMARunner runner( m_handle, blockSize, i);
                    result = runner.run();
                } catch (const xrt_core::error &ex) {
                    std::cout << "ERROR: " << ex.what() << std::endl;
                    return ex.get();
                }
            }
        }

        return result;
    }

    int memread(std::string aFilename, unsigned long long aStartAddr = 0, unsigned long long aSize = 0)
    {
        xclbin_lock xclbin_lock(m_handle, m_idx);
        return memaccess(m_handle, get_ddr_mem_size(), getpagesize(),
            pcidev::get_dev(m_idx)->sysfs_name).read(
            aFilename, aStartAddr, aSize);
    }


    int memDMATest(size_t blocksize, unsigned int aPattern = 'J') {
        return memaccess(m_handle, get_ddr_mem_size(), getpagesize(),
            pcidev::get_dev(m_idx)->sysfs_name).runDMATest(
            blocksize, aPattern);
    }

    int memreadCompare(unsigned long long aStartAddr = 0, unsigned long long aSize = 0, unsigned int aPattern = 'J', bool checks = true) {
        return memaccess(m_handle, get_ddr_mem_size(), getpagesize(),
            pcidev::get_dev(m_idx)->sysfs_name).readCompare(
            aStartAddr, aSize, aPattern, checks);
    }

    int memwrite(unsigned long long aStartAddr, unsigned long long aSize, unsigned int aPattern = 'J')
    {
        xclbin_lock xclbin_lock(m_handle, m_idx);
        return memaccess(m_handle, get_ddr_mem_size(), getpagesize(),
            pcidev::get_dev(m_idx)->sysfs_name).write(
            aStartAddr, aSize, aPattern);
    }

    int memwrite( unsigned long long aStartAddr, unsigned long long aSize, char *srcBuf )
    {
        return memaccess(m_handle, get_ddr_mem_size(), getpagesize(),
            pcidev::get_dev(m_idx)->sysfs_name).write(
            aStartAddr, aSize, srcBuf);
    }

    int memwriteQuiet(unsigned long long aStartAddr, unsigned long long aSize, unsigned int aPattern = 'J') {
        return memaccess(m_handle, get_ddr_mem_size(), getpagesize(),
            pcidev::get_dev(m_idx)->sysfs_name).writeQuiet(
            aStartAddr, aSize, aPattern);
    }

    size_t get_ddr_mem_size() {
        std::string errmsg;
        long long ddr_size = 0;
        int ddr_bank_count = 0;
        pcidev::get_dev(m_idx)->sysfs_get<long long>("rom", "ddr_bank_size", errmsg, ddr_size, 0);
        pcidev::get_dev(m_idx)->sysfs_get<int>("rom", "ddr_bank_count_max", errmsg, ddr_bank_count, 0);

        if (!errmsg.empty()) {
            std::cout << errmsg << std::endl;
            return -EINVAL;
        }
        return GB(ddr_size)*ddr_bank_count / (1024 * 1024);
    }


   //Debug related functionality.
    uint32_t getIPCountAddrNames(int type, std::vector<uint64_t> *baseAddress, std::vector<std::string> * portNames);

    std::pair<size_t, size_t> getCUNamePortName (std::vector<std::string>& aSlotNames,
                             std::vector< std::pair<std::string, std::string> >& aCUNamePortNames);
    std::pair<size_t, size_t> getStreamName (const std::vector<std::string>& aSlotNames,
                             std::vector< std::pair<std::string, std::string> >& aStreamNames);
    int readAIMCounters();
    int readAMCounters();
    int readASMCounters();
    int readLAPCheckers(int aVerbose);
    int readStreamingCheckers(int aVerbose);
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
     * --skip : specify the source offset (in block counts)
     * --seek : specify the destination offset (in block counts)
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

    int validate(bool quick, bool hidden);

    int reset(xclResetKind kind);
    int setP2p(bool enable, bool force);
    int setCma(bool enable, uint64_t total_size);
    int testP2p(void);
    int testM2m(void);
    int iopsTest(void);

private:
    // Run a test case as <exe> <xclbin> [-d index] on this device and collect
    // all output from the run into "output"
    // Note: exe should assume index to be 0 without -d
    int runTestCase(const std::string& exe, const std::string& xclbin, std::string& output);

    int scVersionTest(void);
    int pcieLinkTest(void);
    int auxConnectionTest(void);
    int verifyKernelTest(void);
    int bandwidthKernelTest(void);
    // testFunc must return 0 for success, 1 for warning, and < 0 for error
    int runOneTest(std::string testName, std::function<int(void)> testFunc);

    int getXclbinuuid(uuid_t &uuid);
};

void printHelp(const std::string& exe);
int xclTop(int argc, char *argv[]);
int xclReset(int argc, char *argv[]);
int xclValidate(int argc, char *argv[]);
std::unique_ptr<xcldev::device> xclGetDevice(unsigned index);
int xclP2p(int argc, char *argv[]);
int xclCma(int argc, char *argv[]);
} // end namespace xcldev

#endif /* XBUTIL_H */
