/**
 * Copyright (C) 2016-2018 Xilinx, Inc
 * Author: Sonal Santan, Ryan Radjabi
 * Simple command line utility to inetract with SDX EDGE devices
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

#include <iostream>
#include <fstream>
#include <assert.h>
#include <vector>
#include <map>
#include <iomanip>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include "xrt.h"
#include "experimental/xrt-next.h"
#include "xclperf.h"
#include "core/common/utils.h"
#include "core/common/sensor.h"
#include "core/edge/include/zynq_ioctl.h"
#include "core/edge/user/zynq_dev.h"
#include "xclbin.h"
#include <version.h>
#include <fcntl.h>
#include <chrono>
using Clock = std::chrono::high_resolution_clock;

#define TO_STRING(x) #x
#define AXI_FIREWALL

#define XCL_NO_SENSOR_DEV_LL    ~(0ULL)
#define XCL_NO_SENSOR_DEV       ~(0U)
#define XCL_NO_SENSOR_DEV_S     0xffff
#define XCL_INVALID_SENSOR_VAL 0

/*
 * Simple command line tool to query and interact with SDx EDGE devices
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
    M2MTEST
};
enum subcommand {
    MEM_READ = 0,
    MEM_WRITE,
    STATUS_SPM,
    STATUS_LAPC,
    STATUS_SSPM,
    STATUS_SPC,
    STREAM,
    STATUS_UNSUPPORTED,
    STATUS_AM,
};
enum statusmask {
    STATUS_NONE_MASK = 0x0,
    STATUS_SPM_MASK = 0x1,
    STATUS_LAPC_MASK = 0x2,
    STATUS_SSPM_MASK = 0x4,
    STATUS_SPC_MASK = 0x8,
    STATUS_AM_MASK = 0x10
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
    std::make_pair("m2mtest", M2MTEST)

};

static const std::pair<std::string, subcommand> subcmd_pairs[] = {
    std::make_pair("read", MEM_READ),
    std::make_pair("write", MEM_WRITE),
    std::make_pair("spm", STATUS_SPM),
    std::make_pair("lapc", STATUS_LAPC),
    std::make_pair("sspm", STATUS_SSPM),
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

static const std::map<std::string, command> commandTable(map_pairs, map_pairs + sizeof(map_pairs) / sizeof(map_pairs[0]));

class device {
    unsigned int m_idx;
    xclDeviceHandle m_handle;
    xclDeviceInfo2 m_devinfo;
    xclErrorStatus m_errinfo;

public:
    device(unsigned int idx, const char* log) : m_idx(idx), m_handle(nullptr), m_devinfo{} {
        std::string devstr = "device[" + std::to_string(m_idx) + "]";
        m_handle = xclOpen(m_idx, log, XCL_QUIET);
        if (!m_handle)
            throw std::runtime_error("Failed to open " + devstr);
        if (xclGetDeviceInfo2(m_handle, &m_devinfo))
            throw std::runtime_error("Unable to obtain info from " + devstr);
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

    int getComputeUnits(std::vector<ip_data> &computeUnits) const
    {
        std::string errmsg;
        std::vector<char> buf;

        zynq_device::get_dev()->sysfs_get("ip_layout", errmsg, buf);
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

    static void* wordcopy(void *dst, const void* src, size_t bytes)
    {
        // assert dest is 4 byte aligned
        assert((reinterpret_cast<intptr_t>(dst) % 4) == 0);

        using word = uint32_t;
        auto d = reinterpret_cast<word*>(dst);
        auto s = reinterpret_cast<const word*>(src);
        auto w = bytes/sizeof(word);

        for (size_t i=0; i<w; ++i)
        {
            d[i] = s[i];
        }

        return dst;
    }

    int parseComputeUnits(const std::vector<ip_data> &computeUnits) const
    {
        char *skip_cu = std::getenv("XCL_SKIP_CU_READ");

        for( unsigned int i = 0; i < computeUnits.size(); i++ ) {
            boost::property_tree::ptree ptCu;
            unsigned statusBuf = 0;
            if (computeUnits.at( i ).m_type != IP_KERNEL)
                continue;
	        if (!skip_cu) {
                void *ptr = NULL;
                size_t si = 4;
                int mKernelFD = open("/dev/dri/renderD128", O_RDWR);
                if (!mKernelFD) {
                    printf("Cannot open /dev/dri/renderD128 \n");
					return -1;
                }
                drm_zocl_info_cu info = {computeUnits.at(i).m_base_address, -1};
                int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_INFO_CU, &info);
                if (result) {
                    printf("failed to find CU info 0x%lx\n", computeUnits.at(i).m_base_address);
                    return -1;
                }
                ptr = mmap(0, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED, mKernelFD, info.apt_idx*getpagesize());
                if (!ptr) {
                    printf("Map failed for aperture 0x%lx, size 0x%x\n", computeUnits.at( i ).m_base_address, 0x10000);
                    return -1;
                }
                uint64_t mask = 0xFFFF;
                uint64_t offset = computeUnits.at( i ).m_base_address & mask;
                wordcopy(&statusBuf, (char *)ptr + offset, si);
	        }
            ptCu.put( "name",         computeUnits.at( i ).m_name );
            ptCu.put( "base_address", computeUnits.at( i ).m_base_address );
            ptCu.put( "status",       parseCUStatus( statusBuf ) );
            sensor_tree::add_child( std::string("board.compute_unit." + std::to_string(i)), ptCu );
        }
        return 0;
    }

    void getMemTopology( const xclDeviceUsage &devstat ) const
    {
        std::string errmsg;
        std::vector<char> buf, temp_buf;
        std::vector<std::string> mm_buf, stream_stat;
        uint64_t memoryUsage, boCount;
        zynq_device::get_dev()->sysfs_get("mem_topology", errmsg, buf);
        zynq_device::get_dev()->sysfs_get("memstat_raw", errmsg, mm_buf);

        const mem_topology *map = (mem_topology *)buf.data();

        if(buf.empty() || mm_buf.empty())
            return;

        int j = 0; // stream index
        int m = 0; // mem index

        for(int i = 0; i < map->m_count; i++) {
            boost::property_tree::ptree ptMem;

            std::string str = "**UNUSED**";
            if(map->m_mem_data[i].m_used != 0) {
                auto search = memtype_map.find((MEM_TYPE)map->m_mem_data[i].m_type );
                str = search->second;
                unsigned ecc_st;
                std::string ecc_st_str;
                std::string tag(reinterpret_cast<const char *>(map->m_mem_data[i].m_tag));
            }
            std::stringstream ss(mm_buf[i]);
            ss >> memoryUsage >> boCount;

            ptMem.put( "type",      str );
            ptMem.put( "tag",       map->m_mem_data[i].m_tag );
            ptMem.put( "enabled",   map->m_mem_data[i].m_used ? true : false );
            ptMem.put( "size",      unitConvert(map->m_mem_data[i].m_size << 10) );
            ptMem.put( "mem_usage", unitConvert(memoryUsage));
            ptMem.put( "bo_count",  boCount);
            sensor_tree::add_child( std::string("board.memory.mem." + std::to_string(m)), ptMem );
            m++;
        }
    }

    int readSensors( void ) const
    {
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
        sensor_tree::put( "board.info.clock2", m_devinfo.mOCLFrequency[2] );
        // memory
        xclDeviceUsage devstat = { 0 };
        getMemTopology( devstat );
        // stream

        // xclbin
        std::string errmsg, xclbinid;
        zynq_device::get_dev()->sysfs_get("xclbinid", errmsg, xclbinid);
        if(errmsg.empty()) {
            sensor_tree::put( "board.xclbin.uuid", xclbinid );
        }

        // compute unit
        std::vector<ip_data> computeUnits;
        if( getComputeUnits( computeUnits ) < 0 ) {
            std::cout << "WARNING: 'ip_layout' invalid. Has the bitstream been loaded? See 'xbutil program'.\n";
        }
        parseComputeUnits( computeUnits );

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

    /*
     * dump
     *
     * TODO: Refactor to make function much shorter.
     */
    int dump(std::ostream& ostr) const {
        readSensors();
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << std::setw(32) << "Shell" << std::setw(32) << "FPGA" << "IDCode" << std::endl;
        ostr << std::setw(32) << sensor_tree::get<std::string>( "board.info.dsa_name",  "N/A" )
             << std::setw(32) << sensor_tree::get<std::string>( "board.info.fpga_name", "N/A" )
             << sensor_tree::get<std::string>( "board.info.idcode",    "N/A" ) << std::endl;
        ostr << std::setw(16) << "Vendor" << std::setw(16) << "Device" << std::setw(16) << "SubDevice" << std::setw(16) << "SubVendor" << std::endl;
        // get_pretty since we want these as hex
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.info.vendor",    "N/A", true )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.info.device",    "N/A", true )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.info.subdevice", "N/A", true )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.info.subvendor", "N/A", true ) << std::dec << std::endl;
        ostr << std::setw(16) << "DDR size" << std::setw(16) << "DDR count" << std::setw(16) << "Clock0" << std::setw(16) << "Clock1" << std::setw(16) << "Clock2" << std::endl;
        ostr << std::setw(16) << unitConvert(sensor_tree::get<long long>( "board.info.ddr_size", -1 ))
             << std::setw(16) << sensor_tree::get( "board.info.ddr_count", -1 )
             << std::setw(16) << sensor_tree::get( "board.info.clock0", -1 )
             << std::setw(16) << sensor_tree::get( "board.info.clock1", -1 )
             << std::setw(16) << sensor_tree::get( "board.info.clock2", -1 ) << std::endl;
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
          int cu_i = 0;
          for (auto& v : sensor_tree::get_child( "board.compute_unit" )) {
            int index = std::stoi(v.first);
            if ( index >= 0 ) {
              uint32_t cu_i;
              std::string cu_n, cu_s, cu_ba;
              for (auto& subv : v.second) {
                if ( subv.first == "name" ) {
                  cu_n = subv.second.get_value<std::string>();
                } else if ( subv.first == "base_address" ) {
                  auto addr = subv.second.get_value<uint64_t>();
                  cu_ba = (addr == (uint64_t)-1) ? "N/A" : sensor_tree::pretty<uint64_t>(addr, "N/A", true);
                } else if ( subv.first == "status" ) {
                  cu_s = subv.second.get_value<std::string>();
                }
              }
              if (xclCuName2Index(m_handle, cu_n.c_str(), &cu_i) != 0) {
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
        catch(std::exception const& e) {
            // eat the exception, probably bad path
        }
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
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

        std::vector<char> buffer(length);
        stream.read(buffer.data(), length);
        const xclBin *header = (const xclBin *)buffer.data();
        int result = xclLockDevice(m_handle);
        if (result == 0)
            result = xclLoadXclBin(m_handle, header);
        (void) xclUnlockDevice(m_handle);

        return result;
    }

    int boot() { std::cout << "Unsupported API " << std::endl; return -1; }
    int fan(unsigned speed) { std::cout << "Unsupported API " << std::endl; return -1; }
    int run(unsigned region, unsigned cu) { std::cout << "Unsupported API " << std::endl; return -1; }
    int dmatest(size_t blockSize, bool verbose) { std::cout << "Unsupported API " << std::endl; return -1; }
    int memread(std::string aFilename, unsigned long long aStartAddr = 0, unsigned long long aSize = 0) { std::cout << "Unsupported API " << std::endl; return -1; }
    int memwrite(unsigned long long aStartAddr, unsigned long long aSize, unsigned int aPattern = 'J') { std::cout << "Unsupported API " << std::endl; return -1; }
    //int do_dd(dd::ddArgs_t args ) { std::cout << "Unsupported API " << std::endl; return -1; }
    int validate(bool quick) { std::cout << "Unsupported API " << std::endl; return -1; }
    int reset(xclResetKind kind) { std::cout << "Unsupported API " << std::endl; return -1; }
    int printStreamInfo(std::ostream& ostr) const { std::cout << "Unsupported API " << std::endl; return -1; }

    // Debug related functionality
    uint32_t getIPCountAddrNames(int type,
				 std::vector<uint64_t>* baseAddress,
				 std::vector<std::string>* portNames);
    std::pair<size_t, size_t> 
      getCUNamePortName(std::vector<std::string>& aSlotNames,
			std::vector<std::pair<std::string, std::string> >& aCUNamePortNames);
    std::pair<size_t, size_t> 
      getStreamName(const std::vector<std::string>& aSlotNames,
		    std::vector<std::pair<std::string, std::string> >& aStreamNames) ;
    int readAIMCounters() ;
    int readASMCounters() ;
    int readAMCounters();
    int readLAPCheckers(int aVerbose) ;
    int readStreamingCheckers(int aVerbose) ;
    int print_debug_ip_list (int aVerbose) ;
    int testM2m() { std::cout << "Unsupported API " << std::endl; return -1; }
    int reclock2(unsigned regionIndex, const unsigned short *freq) { std::cout << "Unsupported API " << std::endl; return -1; }
private:

};

void printHelp(const std::string& exe);

 int xclTop(int argc, char *argv[]) ;
 int xclReset(int argc, char *argv[]) ;
 int xclValidate(int argc, char *argv[]) ;
 int xclP2p(int argc, char *argv[]) ;

} // end namespace xcldev

#endif /* XBUTIL_H */
