/**
 * Copyright (C) 2016-2019 Xilinx, Inc
 * Author: Sonal Santan, Ryan Radjabi, Chien-Wei Lan
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
#define XCL_NO_SENSOR_DEV       ~(0UL)
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
    RESET,
    LIST,
    SCAN,
    MEM,
    STATUS,
    CMD_MAX,
};
enum subcommand {
    MEM_READ = 0,
    MEM_WRITE,
    STATUS_SPM,
    STATUS_LAPC,
    STATUS_SSPM,
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

static const std::pair<std::string, command> map_pairs[] = {
    std::make_pair("flash", FLASH),
    std::make_pair("program", PROGRAM),
    std::make_pair("clock", CLOCK),
    std::make_pair("boot", BOOT),
    std::make_pair("help", HELP),
    std::make_pair("query", QUERY),
    std::make_pair("dump", DUMP),
    std::make_pair("reset", RESET),
    std::make_pair("list", LIST),
    std::make_pair("scan", SCAN),
    std::make_pair("mem", MEM),
    std::make_pair("status", STATUS),

};

static const std::pair<std::string, subcommand> subcmd_pairs[] = {
    std::make_pair("read", MEM_READ),
    std::make_pair("write", MEM_WRITE),
    std::make_pair("spm", STATUS_SPM),
    std::make_pair("lapc", STATUS_LAPC),
    std::make_pair("sspm", STATUS_SSPM),
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
        auto dev = pcidev::get_dev(m_idx);
        return dev->mgmt ? dev->mgmt->domain : -1;
    }
    int bus() {
        auto dev = pcidev::get_dev(m_idx);
        return dev->mgmt ? dev->mgmt->bus : -1;
    }
    int dev() {
        auto dev = pcidev::get_dev(m_idx);
       return dev->mgmt ? dev->mgmt->dev : -1;
    }
    int mgmtFunc() {
        auto dev = pcidev::get_dev(m_idx);
        return dev->mgmt ? dev->mgmt->func : -1;
    }
    device(unsigned int idx, const char* log) : m_idx(idx), m_handle(nullptr), m_devinfo{} {
        std::string devstr = "device[" + std::to_string(m_idx) + "]";
        m_handle = xclOpenMgmt(m_idx, log, XCL_QUIET);
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

    int reclock2(unsigned regionIndex, const unsigned short *freq) {
        const unsigned short targetFreqMHz[4] = {freq[0], freq[1], 0, 0};
        return xclReClock2(m_handle, 0, targetFreqMHz);
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
            pcidev::get_dev(m_idx)->mgmt->sysfs_get("icap", "idcode", errmsg, idcode);
            sensor_tree::put( "board.info.idcode", idcode );
            pcidev::get_dev(m_idx)->mgmt->sysfs_get("rom", "FPGA", errmsg, fpga);
            sensor_tree::put( "board.info.fpga_name", fpga );
            pcidev::get_dev(m_idx)->mgmt->sysfs_get("dna", "dna", errmsg, dna);
            sensor_tree::put( "board.info.dna", dna);
        }


        // physical
        sensor_tree::put( "board.physical.thermal.pcb.top_front",                m_devinfo.mSE98Temp[ 0 ] );
        sensor_tree::put( "board.physical.thermal.pcb.top_rear",                 m_devinfo.mSE98Temp[ 1 ] );
        sensor_tree::put( "board.physical.thermal.pcb.btm_front",                m_devinfo.mSE98Temp[ 2 ] );
        sensor_tree::put( "board.physical.thermal.fpga_temp",                    m_devinfo.mOnChipTemp );
        sensor_tree::put( "board.physical.thermal.tcrit_temp",                   m_devinfo.mFanTemp );
        sensor_tree::put( "board.physical.thermal.fan_speed",                    m_devinfo.mFanRpm );
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
        //getMemTopology( devstat );
        // stream

        // xclbin
        /*
        std::string errmsg, xclbinid;
        pcidev::get_dev(m_idx)->user->sysfs_get("", "xclbinuuid", errmsg, xclbinid);
        if(errmsg.empty()) {
            sensor_tree::put( "board.xclbin.uuid", xclbinid );
        }*/
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
        ostr << std::setw(16) << sensor_tree::get<long long>( "board.info.ddr_size", -1 )
             << std::setw(16) << sensor_tree::get( "board.info.ddr_count", -1 )
             << std::setw(16) << sensor_tree::get( "board.info.clock0", -1 )
             << std::setw(16) << sensor_tree::get( "board.info.clock1", -1 ) << std::endl;
        ostr << std::setw(16) << "PCIe"
             << std::setw(32) << "DMA chan(bidir)"
             << std::setw(16) << "MIG Calibrated" << std::endl;
        ostr << "GEN " << sensor_tree::get( "board.info.pcie_speed", -1 ) << "x" << std::setw(10) << sensor_tree::get( "board.info.pcie_width", -1 )
             << std::setw(32) << sensor_tree::get( "board.info.dma_threads", -1 )
             << std::setw(16) << sensor_tree::get<std::string>( "board.info.mig_calibrated", "N/A" ) << std::endl;
        ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ostr << "Temperature(C)\n";
        // use get_pretty for Temperature and Electrical since the driver may rail unsupported values high
        ostr << std::setw(16) << "PCB TOP FRONT" << std::setw(16) << "PCB TOP REAR" << std::setw(16) << "PCB BTM FRONT" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.pcb.top_front" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.pcb.top_rear"  )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.pcb.btm_front" ) << std::endl;
        ostr << std::setw(16) << "FPGA TEMP" << std::setw(16) << "TCRIT Temp" << std::setw(16) << "FAN Speed(RPM)" << std::endl;
        ostr << std::setw(16) << sensor_tree::get<unsigned short>( "board.physical.thermal.fpga_temp", -1 )/1000  // temperature stored in thousandths of degree C
             << std::setw(16) << sensor_tree::get<unsigned short>( "board.physical.thermal.tcrit_temp", -1 )/1000 // temperature stored in thousandths of degree C
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.thermal.fan_speed" ) << std::endl;
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
        ostr << std::setw(16) << "MGT 0V9" << std::setw(16) << "12V SW" << std::setw(16) << "MGT VTT" << std::endl;
        ostr << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.mgt_0v9.voltage" )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.12v_sw.voltage"  )
             << std::setw(16) << sensor_tree::get_pretty<unsigned short>( "board.physical.electrical.mgt_vtt.voltage" ) << std::endl;
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
              unsigned bo_count;
              for (auto& subv : v.second) {
                if( subv.first == "index" )
                  index = subv.second.get_value<int>();
                else if( subv.first == "type" )
                  type = subv.second.get_value<std::string>();
                else if( subv.first == "tag" )
                  tag = subv.second.get_value<std::string>();
                else if( subv.first == "temp" )
                  temp = sensor_tree::pretty<unsigned short>(subv.second.get_value<unsigned short>());
                else if( subv.first == "bo_count" )
                  bo_count = subv.second.get_value<unsigned>();
                else if( subv.first == "mem_usage" )
                  mem_usage = subv.second.get_value<std::string>();
                else if( subv.first == "size" )
                  size = subv.second.get_value<std::string>();
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
        int result = xclDownLoadXclBin(m_handle, header);
        delete [] buffer;

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

    int reset(unsigned region) {
        const xclResetKind kind = (region == 0xffffffff) ? XCL_RESET_FULL : XCL_RESET_KERNEL;
        return xclResetDevice(m_handle, kind);
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

    int deviceInfo(xclDeviceInfo2& devinfo) const {
        return xclGetDeviceInfo2(m_handle, &devinfo);
    }

    int validate(bool quick);

    int printEccInfo(std::ostream& ostr) const;
    int resetEccInfo();

private:
    // Run a test case as <exe> <xclbin> [-d index] on this device and collect
    // all output from the run into "output"
    // Note: exe should assume index to be 0 without -d
    int runTestCase(const std::string& exe, const std::string& xclbin,
        std::string& output);
};

void printHelp(const std::string& exe);
int flash_helper(int argc, char *argv[]);
int xclValidate(int argc, char *argv[]);
std::unique_ptr<xcldev::device> xclGetDevice(unsigned index);
} // end namespace xcldev

#endif /* XBUTIL_H */
