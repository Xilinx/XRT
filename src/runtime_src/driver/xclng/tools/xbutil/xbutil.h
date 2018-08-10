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
#ifndef XBSAK_H
#define XBSAK_H

#include <getopt.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <cstddef>
#include <cctype>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <assert.h>
#include <vector>
#include <memory>
#include <map>
#include <iomanip>
#include <sstream>
#include <errno.h>

#include "driver/include/xclhal2.h"
#include "driver/include/xcl_axi_checker_codes.h"
#include "../user_common/dmatest.h"
#include "../user_common/memaccess.h"
#include "../user_common/dd.h"
#include "../user_common/utils.h"
#include "scan.h"
#include "hwmon.h"

#include "driver/include/xclbin.h"

#include <chrono>
typedef std::chrono::high_resolution_clock Clock;

#define TO_STRING(x) #x
#define AXI_FIREWALL


#define INVAID_SENSOR_VAL 0

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
    RESET,
    RUN,
    FAN,
    DMATEST,
    LIST,
    SCAN,
    MEM,
    DD,
    STATUS,
    VALIDATE,
    CMD_MAX
};
enum subcommand {
    MEM_READ = 0,
    MEM_WRITE,
    STATUS_SPM,
    STATUS_LAPC,
    STATUS_UNSUPPORTED
};
enum statusmask {
    STATUS_NONE_MASK = 0x0,
    STATUS_SPM_MASK = 0x1,
    STATUS_LAPC_MASK = 0x2
};

static const std::pair<std::string, command> map_pairs[] = {
    std::make_pair("flash", FLASH),
    std::make_pair("program", PROGRAM),
    std::make_pair("clock", CLOCK),
    std::make_pair("boot", BOOT),
    std::make_pair("help", HELP),
    std::make_pair("query", QUERY),
    std::make_pair("reset", RESET),
    std::make_pair("run", RUN),
    std::make_pair("fan", FAN),
    std::make_pair("dmatest", DMATEST),
    std::make_pair("list", LIST),
    std::make_pair("scan", SCAN),
    std::make_pair("mem", MEM),
    std::make_pair("dd", DD),
    std::make_pair("status", STATUS),
    std::make_pair("validate", VALIDATE)
};

static const std::pair<std::string, subcommand> subcmd_pairs[] = {
    std::make_pair("read", MEM_READ),
    std::make_pair("write", MEM_WRITE),
    std::make_pair("spm", STATUS_SPM),
    std::make_pair("lapc", STATUS_LAPC)
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

static const std::map<std::string, command> commandTable(map_pairs, map_pairs + sizeof(map_pairs) / sizeof(map_pairs[0]));

class device {
    unsigned int m_idx;
    xclDeviceHandle m_handle;
    xclDeviceInfo2 m_devinfo;
    xclErrorStatus m_errinfo;

public:
    device(unsigned int idx, const char* log) : m_idx(idx), m_handle(nullptr), m_devinfo{} {
        m_handle = xclOpen(m_idx, log, XCL_QUIET);
        if (!m_handle)
            throw std::runtime_error("Failed to open device index, " + std::to_string(m_idx));
        if (xclGetDeviceInfo2(m_handle, &m_devinfo))
            throw std::runtime_error("Unable to query device index, " + std::to_string(m_idx));
#ifdef AXI_FIREWALL
        if (xclGetErrorStatus(m_handle, &m_errinfo))
            throw std::runtime_error("Unable to query device index for AXI error, " + std::to_string(m_idx));
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

    /*
     * getComputeUnits
     */
    int getComputeUnits( std::vector<ip_data> &computeUnits ) const
    {
        const std::string DevPath = "/sys/bus/pci/devices/" + xcldev::pci_device_scanner::device_list[ m_idx ].user_name;
        std::string iplayout_path = DevPath + "/ip_layout";
        std::ifstream ifs;
        ifs.open( iplayout_path.c_str(), std::ifstream::binary );
        if( !ifs.good() ) {
            return errno;
        }
        struct stat sb;
        if(stat( iplayout_path.c_str(), &sb ) < 0)
	    return -1;

	int numCount = 0;
	ifs.read((char*)&numCount, sizeof(numCount));
	ifs.seekg(0, ifs.beg);
	if(numCount == 0)
	    return 0;
	if(numCount < 0)
	    return -1;
	int buf_size = sizeof(ip_layout)*numCount + offsetof(ip_layout, m_ip_data);
	buf_size *=2; //TODO: double it for padding safety.
        char* fileReadBuf = new char [buf_size];
	memset(fileReadBuf, 0, buf_size);
        ifs.read( fileReadBuf, buf_size);
        if( ifs.gcount() <= 0 ) {
            delete [] fileReadBuf;
            return errno;
        }
        const ip_layout *map = (ip_layout *)fileReadBuf;
        for( int i = 0; i < map->m_count; i++ ) {
            computeUnits.emplace_back( map->m_ip_data[ i ] );
        }
	delete [] fileReadBuf;
        ifs.close();
        return 0;
    }

    void m_devinfo_stringize_statics(const xclDeviceInfo2 *m_devinfo, std::vector<std::string> &lines) const
    {

        std::stringstream ss, subss;

        ss << std::left;
        ss << std::setw(16) << "DSA name" <<"\n";
        ss << std::setw(16) << m_devinfo->mName << "\n\n";
        ss << std::setw(16) << "Vendor" << std::setw(16) << "Device";
        ss << std::setw(16) << "SubDevice" <<  std::setw(16) << "SubVendor";
        ss << std::setw(16) << "XMC fw version" << "\n";

        ss << std::setw(16) << std::hex << m_devinfo->mVendorId << std::dec;
        ss << std::setw(16) << std::hex << m_devinfo->mDeviceId << std::dec;
        ss << std::setw(16) << std::hex << m_devinfo->mSubsystemId << std::dec;
        ss << std::setw(16) << std::hex << m_devinfo->mSubsystemVendorId << std::dec;
        ss << std::setw(16) << m_devinfo->mXMCVersion << "\n\n";

        ss << std::setw(16) << "DDR size" << std::setw(16) << "DDR count";
        ss << std::setw(16) << "OCL Frequency";

        subss << std::left << std::setw(16) << unitConvert(m_devinfo->mDDRSize);
        subss << std::setw(16) << m_devinfo->mDDRBankCount << std::setw(16) << " ";

        for(unsigned i= 0; i < m_devinfo->mNumClocks; ++i) {
            ss << "Clock" << std::setw(11) << i ;
            subss << m_devinfo->mOCLFrequency[i] << std::setw(13) << " MHz";
        }
        ss << "\n" << subss.str() << "\n\n";

        ss << std::setw(16) << "PCIe" << std::setw(32) << "DMA bi-directional threads";
        ss << std::setw(16) << "MIG Calibrated " << "\n";

        ss << "GEN " << m_devinfo->mPCIeLinkSpeed << "x" << std::setw(10) << m_devinfo->mPCIeLinkWidth;
        ss << std::setw(32) << m_devinfo->mDMAThreads;
        ss << std::setw(16) << std::boolalpha << m_devinfo->mMigCalib << std::noboolalpha << "\n";
        ss << std::right << std::setw(80) << std::setfill('#') << std::left << "\n";
        lines.push_back(ss.str());
   }

    void m_devinfo_stringize_dynamics(const xclDeviceInfo2 *m_devinfo, std::vector<std::string> &lines) const
    {
        std::stringstream ss, subss;
        unsigned long long power;
        ss << std::left << "\n";
        unsigned i;

        if(m_devinfo->mDimmTemp[0]!=0){
            for(i= 0; i < m_devinfo->mDDRBankCount; ++i){
                ss << "DDR Temp" << std::setw(8) << i;
                subss << std::left << std::setw(16) << std::to_string(m_devinfo->mDimmTemp[i]).substr(0,3)+" C";
            }
            ss << "\n" << subss.str() << "\n\n";
            subss.str("");
        }
        if(m_devinfo->mSE98Temp[0]!=0){
            for(i= 0; i < 3; ++i){
                ss << std::setw(16) << "SE98 Temp"+std::to_string(i);
                subss << std::left << std::setw(16) << std::to_string(m_devinfo->mSE98Temp[i]).substr(0,3)+" C";
            }
            ss << "\n" << subss.str() << "\n\n";
        }

        ss << std::setw(16) << "OnChip Temp" << std::setw(16) << "Fan Temp" << std::setw(16) << "Fan Speed" << "\n";
        ss << std::setw(16) << std::to_string(m_devinfo->mOnChipTemp) +" C";

        if(m_devinfo->mFanTemp != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string(m_devinfo->mFanTemp) +" C";
        }
        else
            ss << std::setw(16) << "Not support";

        if(m_devinfo->mFanRpm != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string(m_devinfo->mFanRpm) +" rpm" << "\n\n";
        }
        else
            ss << std::setw(16) << "Not support" << "\n\n";

        ss << std::setw(16) << "12V PEX" << std::setw(16) << "12V AUX";
        ss << std::setw(16) << "12V PEX Current" << std::setw(16) << "12V AUX Current" << "\n";
  //      ss << std::setw(16) << "Power" << "\n";

        if(m_devinfo->m12VPex != INVAID_SENSOR_VAL){
            float vol = (float)m_devinfo->m12VPex/1000;
            ss << std::setw(16) << std::to_string(vol).substr(0,4) + "V";
        }
        else
            ss << std::setw(16) << "Not support";

        if(m_devinfo->m12VAux != INVAID_SENSOR_VAL){
            float vol = (float)m_devinfo->m12VAux/1000;
            ss << std::setw(16) << std::to_string(vol).substr(0,4) + "V";
        }
        else
            ss << std::setw(16) << "Not support";

        if(m_devinfo->mPexCurr != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string(m_devinfo->mPexCurr).substr(0,4) + "mA";
        }
        else
            ss << std::setw(16) << "Not support";

        if(m_devinfo->mAuxCurr != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string(m_devinfo->mAuxCurr).substr(0,4) + "mA" << "\n\n";
        }
        else
            ss << std::setw(16) << "Not support" << "\n\n";

        ss << std::setw(16) << "3V3 PEX" << std::setw(16) << "3V3 AUX";
        ss << std::setw(16) << "DDR VPP BOTTOM" << std::setw(16) << "DDR VPP TOP" << "\n";

        if(m_devinfo->m3v3Pex != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string((float)m_devinfo->m3v3Pex/1000).substr(0,4) + "V";
        }
        else
            ss << std::setw(16) << "Not support";

        if(m_devinfo->m3v3Aux != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string((float)m_devinfo->m3v3Aux/1000).substr(0,4) + "V";
        }
        else
            ss << std::setw(16) << "Not support";

        if(m_devinfo->mDDRVppBottom != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string((float)m_devinfo->mDDRVppBottom/1000).substr(0,4) + "V";
        }
        else
            ss << std::setw(16) << "Not support";

        if(m_devinfo->mDDRVppTop != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string((float)m_devinfo->mDDRVppTop/1000).substr(0,4) + "V" << "\n\n";
        }
        else
            ss << std::setw(16) << "Not support" << "\n\n";

        ss << std::setw(16) << "SYS 5V5" << std::setw(16) << "1V2 TOP";
        ss << std::setw(16) << "1V8 TOP" << std::setw(16) << "0V85" << "\n";


        if(m_devinfo->mSys5v5 != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string((float)m_devinfo->mSys5v5/1000).substr(0,4) + "V";
        }
        else
            ss << std::setw(16) << "Not support";

        if(m_devinfo->m1v2Top != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string((float)m_devinfo->m1v2Top/1000).substr(0,4) + "V";
        }
        else
            ss << std::setw(16) << "Not support";   

        if(m_devinfo->m1v8Top != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string((float)m_devinfo->m1v8Top/1000).substr(0,4) + "V";
        }
        else
            ss << std::setw(16) << "Not support";      

        if(m_devinfo->m0v85 != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string((float)m_devinfo->m0v85/1000).substr(0,4) + "V" << "\n\n";
        }
        else
            ss << std::setw(16) << "Not support" << "\n\n";

        ss << std::setw(16) << "MGT 0V9" << std::setw(16) << "12V SW";
        ss << std::setw(16) << "MGT VTT" << std::setw(16) << "1V2 BOTTOM" << "\n";


        if(m_devinfo->mMgt0v9 != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string((float)m_devinfo->mMgt0v9/1000).substr(0,4) + "V";
        }
        else
            ss << std::setw(16) << "Not support";    

        if(m_devinfo->m12vSW != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string((float)m_devinfo->m12vSW/1000).substr(0,4) + "V";
        }
        else
            ss << std::setw(16) << "Not support";     

        if(m_devinfo->mMgtVtt != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string((float)m_devinfo->mMgtVtt/1000).substr(0,4) + "V";
        }
        else
            ss << std::setw(16) << "Not support"; 

        if(m_devinfo->m1v2Bottom != INVAID_SENSOR_VAL){
            ss << std::setw(16) << std::to_string((float)m_devinfo->m1v2Bottom/1000).substr(0,4) + "V" << "\n\n";
        }
        else
            ss << std::setw(16) << "Not support" << "\n\n";

        ss << std::setw(16) << "Power (Beta)" << "\n";
        power = m_devinfo->mPexCurr*m_devinfo->m12VPex;
        if(power){
            ss << std::setw(16) << std::to_string((float)power/1000000).substr(0,4)+"W" << "\n\n";
        }
        else
            ss << std::setw(16) << "Not support" << "\n\n";

        ss << std::right << std::setw(80) << std::setfill('#') << std::left << "\n";
        lines.push_back(ss.str());         
    }

    void m_devinfo_stringize(const xclDeviceInfo2 *m_devinfo, std::vector<std::string> &lines) const
    {
        m_devinfo_stringize_statics(m_devinfo, lines);
        m_devinfo_stringize_dynamics(m_devinfo, lines);
    }

    void m_mem_usage_gui_dynamics(xclDeviceUsage &devstat, std::vector<std::string> &lines, int result, unsigned numSupportedMems) const
    {
        std::stringstream ss;
        std::ifstream ifs;;
        unsigned int numDDR;
        int nums_fiftieth;
        float percentage;
        std::string str;

        const std::string devPath = "/sys/bus/pci/devices/" + xcldev::pci_device_scanner::device_list[ m_idx ].user_name;
        std::string mem_path = devPath + "/mem_topology";

        ifs.open( mem_path.c_str(), std::ifstream::binary );

        int numBanks = 0;
        ifs.read((char*)&numBanks, sizeof(numBanks));
        ifs.seekg(0, ifs.beg);
        if( numBanks == 0 ) {
            ss << std::setw(40) << "-- none found --. See 'xbutil program'.";
        } else if( numBanks > 0 ) {
            int buf_size = sizeof(mem_topology)*numBanks + offsetof(mem_topology, m_mem_data) ;
            buf_size *= 2; //TODO: just double this for padding safety for now.
            char* buffer = new char[ buf_size ];
            memset(buffer, 0, buf_size);
            ifs.read( buffer, buf_size);
            mem_topology *map;
            map = (mem_topology *)buffer;
            numDDR = map->m_count;

            for( unsigned i = 0; i <numDDR; i++ ) {

                percentage = (float)devstat.ddrMemUsed[i]*100 / (map->m_mem_data[ i ].m_size<<10);
                nums_fiftieth = (int)percentage/2;

                str = std::to_string(percentage).substr(0,4)+"%";

                ss << " [" << i << "] " << std::setw(16-(std::to_string(i).length())-4) << std::left << map->m_mem_data[ i ].m_tag;
           //     ss << std::setw(20) << " [" +std::to_string(i)+ "] " + (map->m_mem_data[ i ].m_tag);

                ss << "[ " << std::right << std::setw(nums_fiftieth) << std::setfill('|') << (nums_fiftieth ? " ":"") <<  std::setw(56-nums_fiftieth);
                ss << std::setfill(' ') << str << " ]" << "\n";

            }
            delete[] buffer;
        } else { // mem_topology exists, but no data read or reported
            ss << "WARNING: 'mem_topology' invalid, unable to report topology. Has the bitstream been loaded? See 'xbutil program'." << std::endl;
        }
        ifs.close();

        ss << "\n";
        lines.push_back(ss.str());
    }


    void m_mem_usage_stringize_dynamics(xclDeviceUsage &devstat, std::vector<std::string> &lines, int result, unsigned numSupportedMems) const
    {
        
        std::stringstream ss;
        std::ifstream ifs;;
        unsigned int numDDR;


        const std::string devPath = "/sys/bus/pci/devices/" + xcldev::pci_device_scanner::device_list[ m_idx ].user_name;
        ss << std::left << std::setw(56) << "Mem Topology" << std::setw(24) << "Device Memory Usage" << "\n"; 
        std::string mem_path = devPath + "/mem_topology";

        ifs.open( mem_path.c_str(), std::ifstream::binary );

        int numBanks = 0;
        ifs.read((char*)&numBanks, sizeof(numBanks));
        ifs.seekg(0, ifs.beg);
        if( numBanks == 0 ) {
            ss << std::setw(40) << "-- none found --. See 'xbutil program'.";
        } else if( numBanks > 0 ) {
            int buf_size = sizeof(mem_topology)*numBanks + offsetof(mem_topology, m_mem_data) ;
            buf_size *= 2; //TODO: just double this for padding safety for now.
            char* buffer = new char[ buf_size ];
            memset(buffer, 0, buf_size);
            ifs.read( buffer, buf_size);
            mem_topology *map;
            map = (mem_topology *)buffer;
            ss << std::setw(16) << "Tag"  << std::setw(16) << "Type" << std::setw(16) << "Base Address" << std::setw(8) << "Size";

            ss << std::setw(16) << "Mem Usage" << std::setw(8) << "BO nums" << "\n";
            numDDR = map->m_count;
          //  if(numDDR <= 8) //Driver side limit, ddrMemUsed etc
               // numSupportedMems = numDDR;
            for( unsigned i = 0; i <numDDR; i++ ) {
                ss << " [" << i << "] " << std::setw(16-(std::to_string(i).length())-4) << std::left << map->m_mem_data[ i ].m_tag;
                std::string str;
                if( map->m_mem_data[ i ].m_used == 0 ) {
                    str = "**UNUSED**";
                } else {
                    std::map<MEM_TYPE, std::string> my_map = {
                        {MEM_DDR3, "MEM_DDR3"}, {MEM_DDR4, "MEM_DDR4"}
                        ,{MEM_DRAM, "MEM_DRAM"}, {MEM_STREAMING, "MEM_STREAMING"}
                        ,{MEM_PREALLOCATED_GLOB, "MEM_PREALLOCATED_GLOB"}, {MEM_ARE, "MEM_ARE"}
                        ,{MEM_HBM, "MEM_HBM"}, {MEM_BRAM, "MEM_BRAM"}
                        ,{MEM_URAM, "MEM_URAM"}
                    };
                    auto search = my_map.find( (MEM_TYPE)map->m_mem_data[ i ].m_type );
                    str = search->second;
                }


                ss << std::left << std::setw(16) << str;
                ss << "0x" << std::setw(14) << std::hex << map->m_mem_data[ i ].m_base_address;          // print base address

                ss << std::setw(8) << unitConvert(map->m_mem_data[ i ].m_size<<10);;
                ss << std::setw(16) << unitConvert(devstat.ddrMemUsed[i]);
                ss << std::setw(8) << std::dec << devstat.ddrBOAllocated[i] << "\n";                 // print size

            }
            delete[] buffer;
        } else { // mem_topology exists, but no data read or reported
            ss << "WARNING: 'mem_topology' invalid, unable to report topology. Has the bitstream been loaded? See 'xbutil program'." << std::endl;
        }
        ifs.close();

        ss << "\nTotal DMA Transfer Metrics:" << "\n";
        for (unsigned i = 0; !result & (i < 2); i++) {
            ss << "  Chan[" << i << "].h2c:  " << unitConvert(devstat.h2c[i]) << "\n";
            ss << "  Chan[" << i << "].c2h:  " << unitConvert(devstat.c2h[i]) << "\n";
        }
        lines.push_back(ss.str());
    }

    /*
     * validate
     */
    int validate()
    {
        std::vector<ip_data> computeUnits;
        int retVal = getComputeUnits( computeUnits );
        if( retVal < 0 ) {
            std::cout << "WARNING: 'ip_layout' invalid. Has the bitstream been loaded? See 'xbutil program'.\n";
            return retVal;
        }
        unsigned buf[ 16 ];
        for( unsigned int i = 0; i < computeUnits.size(); i++ ) {
            xclRead(m_handle, XCL_ADDR_KERNEL_CTRL, computeUnits.at( i ).m_base_address, &buf, 16);
            if (!((buf[0] == 0x0) || (buf[0] == 0x4) || (buf[0] == 0x6))) {
                return -EBUSY;
            }
        }
        return 0;
    }

    /*
     * dump
     *
     * TODO: Refactor to make function much shorter.
     */
    int dump(std::ostream& ostr) const {
        xclDeviceUsage devstat;
        int result = xclGetUsageInfo(m_handle, &devstat);
        unsigned numDDR = m_devinfo.mDDRBankCount;
        
        std::vector<std::string> lines, usage_lines;

        m_devinfo_stringize(&m_devinfo, lines);
 
        for(auto line:lines){
            ostr << line;
        }

#ifdef AXI_FIREWALL
        char cbuf[80];
        struct tm *ts;
        time_t temp;
        ostr << "\nFirewall Last Error Status:\n";
        for(unsigned i= 0; i < m_errinfo.mNumFirewalls; ++i) {
            ostr << "  " << std::setw(7) << i << ": 0x" << std::hex
                 << m_errinfo.mAXIErrorStatus[i].mErrFirewallStatus << std::dec << " "
                 << parseFirewallStatus(m_errinfo.mAXIErrorStatus[i].mErrFirewallStatus) ;
            if(m_errinfo.mAXIErrorStatus[i].mErrFirewallStatus != 0x0) {
                temp = (time_t)m_errinfo.mAXIErrorStatus[i].mErrFirewallTime;
                ts = localtime(&temp);
                strftime(cbuf, sizeof(cbuf), "%a %Y-%m-%d %H:%M:%S %Z",ts);
                ostr << ".\n";
                ostr << std::right << std::setw(11) << " " << "Error occurred on " << std::left << cbuf << "\n";
            }
            else{
                ostr << "\n";
            }
        }
        ostr << std::right << std::setw(80) << std::setfill('#') << std::left << "\n";
        ostr << std::setfill(' ');
#endif // AXI Firewall

        // report xclbinid
        const std::string devPath = "/sys/bus/pci/devices/" + xcldev::pci_device_scanner::device_list[ m_idx ].user_name;
        std::string binid_path = devPath + "/xclbinid";
        struct stat sb;
        if( stat( binid_path.c_str(), &sb ) < 0 ) {
            std::cout << "ERROR: failed to stat " << binid_path << std::endl;
            return errno;
        }
        std::ifstream ifs( binid_path.c_str(), std::ifstream::binary );
        if( !ifs.good() ) {
            return errno;
        }
        char* fileReadBuf = new char[sb.st_size];
        memset(fileReadBuf, 0, sb.st_size);
        ifs.read( fileReadBuf, sb.st_size );
        if( ifs.gcount() > 0 ) {
            ostr << std::setw(16) << "\nXclbin ID:" << "\n";
            ostr << "0x" << std::setw(14) << fileReadBuf << "\n";
        } else { // xclbinid exists, but no data read or reported
            ostr << "WARNING: 'xclbinid' invalid, unable to report xclbinid. Has the bitstream been loaded? See 'xbutil program'.\n";
        }
        delete [] fileReadBuf;
        ifs.close();

       // get DDR bank count from mem_topology if possible
       // unsigned numSupportedMems = numDDR;

        ostr << "Compute Unit Status:\n";
        std::vector<ip_data> computeUnits;
        if( getComputeUnits( computeUnits ) < 0 ) {
            ostr << "WARNING: 'ip_layout' invalid. Has the bitstream been loaded? See 'xbutil program'.\n";
        } else {
            for( unsigned int i = 0; i < computeUnits.size(); i++ ) {
                static int cuCnt = 0;
                if( computeUnits.at( i ).m_type == IP_KERNEL ) {
                    unsigned statusBuf;
                    xclRead(m_handle, XCL_ADDR_KERNEL_CTRL, computeUnits.at( i ).m_base_address, &statusBuf, 4);
                    ostr << "CU[" << cuCnt << "]: "
                         << computeUnits.at( i ).m_name
                         << "@0x" << std::hex << computeUnits.at( i ).m_base_address << " "
                         << std::dec << parseCUStatus( statusBuf ) << "\n";
                    cuCnt++;
                }
            }
            if(computeUnits.size() == 0) {
                ostr << std::setw(40) << "-- none found --. See 'xbutil program'.";
            }
        }
        ostr << std::right << std::setw(80) << std::setfill('#') << std::left << "\n";
        ostr << std::setfill(' ') << "\n";;

        m_mem_usage_stringize_dynamics(devstat, usage_lines, result, numDDR);

        for(auto line:usage_lines){
            ostr << line;
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
        if (result)
            return result;
        result = xclLoadXclBin(m_handle, header);
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
    int dmatest(size_t blockSize) {
        std::cout << "Total DDR size: " << m_devinfo.mDDRSize/(1024 * 1024) << " MB\n";
        unsigned numDDR = m_devinfo.mDDRBankCount;
        bool isAREDevice = false;
        if (strstr(m_devinfo.mName, "-xare")) {//This is ARE device
            isAREDevice = true;
        }

        int result = 0;
        unsigned long long addr = 0x0;
        unsigned long long sz = 0x1;
        unsigned int pattern = 'J';
        unsigned long long oneDDRSize = m_devinfo.mDDRSize / numDDR;
        unsigned xcl_bank[4] = {
            XCL_DEVICE_RAM_BANK0,
            XCL_DEVICE_RAM_BANK1,
            XCL_DEVICE_RAM_BANK2,
            XCL_DEVICE_RAM_BANK3
        };

        // get DDR bank count from mem_topology if possible
        std::string path = "/sys/bus/pci/devices/" + xcldev::pci_device_scanner::device_list[ m_idx ].user_name + "/mem_topology";
        std::ifstream ifs( path.c_str(), std::ifstream::binary );
        if( ifs.good() )
        {   // unified+ mode
            struct stat sb;
            if(stat( path.c_str(), &sb ) < 0) {
                std::cout << "WARNING: 'mem_topology' invalid, unable to perform DMA Test. Has the bitstream been loaded? See 'xbutil program'." << std::endl;
                return -1;
            }
            ifs.read((char*)&numDDR, sizeof(numDDR));
            ifs.seekg(0, ifs.beg);
            if( numDDR == 0 ) {
                std::cout << "WARNING: 'mem_topology' invalid, unable to perform DMA Test. Has the bitstream been loaded? See 'xbutil program'." << std::endl;
                return -1;
            }else {
                int buf_size = sizeof(mem_topology)*numDDR + offsetof(mem_topology, m_mem_data) ;
                buf_size *= 2; //TODO: just double this for padding safety for now.
                char* buffer = new char[buf_size];
                memset(buffer, 0, buf_size);
                ifs.read(buffer, buf_size);
                mem_topology *map;
                map = (mem_topology *)buffer;
                std::cout << "Reporting from mem_topology:" << std::endl;
                numDDR = map->m_count;
                for( unsigned i = 0; i < numDDR; i++ ) {
                    if( map->m_mem_data[i].m_used ) {
                        std::cout << "Data Validity & DMA Test on mem_data[" << i << "]\n";
                        addr = map->m_mem_data[i].m_base_address;

                        for( unsigned sz = 1; sz <= 256; sz *= 2 ) {
                            result = memwriteQuiet( addr, sz, pattern );
                            if( result < 0 )
                                break;
                            result = memreadCompare(addr, sz, pattern , false);
                            if( result < 0 )
                                break;
                        }
                        if( result >= 0 ) {
                            DMARunner runner( m_handle, blockSize, i);
                            result = runner.run();
                        }

                        if( result < 0 ) {
                            delete [] buffer;
                            ifs.close();
                            return result;
                        }
                    }
                }
                delete [] buffer;
            }
            ifs.close();
        } else { // legacy mode
            std::cout << "Reporting in legacy mode:" << std::endl;
            for (unsigned i = 0; i < numDDR; i++) {
                std::cout << "Data Validity & DMA Test on DDR[" << i << "]\n";
                addr = i * oneDDRSize;

                for( unsigned sz = 1; sz <= 256; sz *= 2 ) {
                    result = memwriteQuiet( addr, sz, pattern );
                    if( result < 0 )
                        return result;
                    result = memreadCompare(addr, sz, pattern);
                    if( result < 0 )
                        return result;
                }

                DMARunner runner( m_handle, blockSize, xcl_bank[i] );
                result = runner.run();
                if( result < 0 )
                    return result;
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
        return memaccess(m_handle, m_devinfo.mDDRSize, m_devinfo.mDataAlignment, xcldev::pci_device_scanner::device_list[ m_idx ].user_name  ).read(aFilename, aStartAddr, aSize);
    }


    int memDMATest(size_t blocksize, unsigned int aPattern = 'J') {
        return memaccess(m_handle, m_devinfo.mDDRSize, m_devinfo.mDataAlignment, xcldev::pci_device_scanner::device_list[ m_idx ].user_name).runDMATest(blocksize, aPattern);
    }

    int memreadCompare(unsigned long long aStartAddr = 0, unsigned long long aSize = 0, unsigned int aPattern = 'J', bool checks = true) {
        return memaccess(m_handle, m_devinfo.mDDRSize, m_devinfo.mDataAlignment, xcldev::pci_device_scanner::device_list[ m_idx ].user_name).readCompare(aStartAddr, aSize, aPattern, checks);
    }

    int memwrite(unsigned long long aStartAddr, unsigned long long aSize, unsigned int aPattern = 'J') {
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
        return memaccess(m_handle, m_devinfo.mDDRSize, m_devinfo.mDataAlignment, xcldev::pci_device_scanner::device_list[ m_idx ].user_name).write(aStartAddr, aSize, aPattern);
    }

    int memwrite( unsigned long long aStartAddr, unsigned long long aSize, char *srcBuf )
    {
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
        return memaccess(m_handle, m_devinfo.mDDRSize, m_devinfo.mDataAlignment, xcldev::pci_device_scanner::device_list[ m_idx ].user_name).write( aStartAddr, aSize, srcBuf );
    }

    int memwriteQuiet(unsigned long long aStartAddr, unsigned long long aSize, unsigned int aPattern = 'J') {
        return memaccess(m_handle, m_devinfo.mDDRSize, m_devinfo.mDataAlignment, xcldev::pci_device_scanner::device_list[ m_idx ].user_name).writeQuiet(aStartAddr, aSize, aPattern);
    }


   //Debug related functionality.
    uint32_t getIPCountAddrNames(int type, std::vector<uint64_t> *baseAddress, std::vector<std::string> * portNames);

    std::pair<size_t, size_t> getCUNamePortName (std::vector<std::string>& aSlotNames,
                             std::vector< std::pair<std::string, std::string> >& aCUNamePortNames);
    int readSPMCounters();
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
};

void printHelp(const std::string& exe);
int xclXbsak(int argc, char *argv[]);
int xclTop(int argc, char *argv[]);
std::unique_ptr<xcldev::device> xclGetDevice(unsigned index);

} // end namespace xcldev

#endif /* XBSAK_H */
