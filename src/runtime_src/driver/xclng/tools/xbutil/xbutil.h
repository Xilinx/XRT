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
        ostr << "DSA name:       " << m_devinfo.mName << "\n";
        ostr << "Vendor:         " << std::hex << m_devinfo.mVendorId << std::dec << "\n";
        ostr << "Device:         " << std::hex << m_devinfo.mDeviceId << std::dec << "\n";
        ostr << "SDevice:        " << std::hex << m_devinfo.mSubsystemId << std::dec << "\n";
        ostr << "SVendor:        " << std::hex << m_devinfo.mSubsystemVendorId << std::dec << "\n";
        ostr << "DDR size:       " << "0x" << std::hex << m_devinfo.mDDRSize/1024 << std::dec << " KB\n";

        ostr << "DDR count:      " << numDDR << "\n";
        ostr << "OnChip Temp:    " << m_devinfo.mOnChipTemp << " C\n";
        //ostr << "Fan Temp:       " << m_devinfo.mFanTemp<< " C\n";

        PowerMetrics pm( m_idx );
        if( pm.getTotalPower_mW() <= 0 )
        {
            ostr << "Power(Beta):    **Unable to estimate power**\n";
        }
        else
        {
            ostr << "Power(Beta):    " << ( (float)pm.getTotalPower_mW() ) / 1000 << " W\n";
        }

        ostr << "OCL Frequency:\n";
        for(unsigned i= 0; i < m_devinfo.mNumClocks; ++i) {
            ostr << "  " << std::setw(7) << i << ":      " <<  m_devinfo.mOCLFrequency[i] << " MHz\n";
        }
        ostr << "PCIe:           " << "GEN" << m_devinfo.mPCIeLinkSpeed << " x " << m_devinfo.mPCIeLinkWidth << "\n";
        ostr << "DMA bi-directional threads:    " << m_devinfo.mDMAThreads << "\n";
        ostr << "MIG Calibrated: " << std::boolalpha << m_devinfo.mMigCalib << std::noboolalpha << "\n";

        ostr << "\nTotal DMA Transfer Metrics:" << "\n";
        for (unsigned i = 0; !result & (i < 2); i++) {
            ostr << "  Chan[" << i << "].h2c:  " << unitConvert(devstat.h2c[i]) << "\n";
            ostr << "  Chan[" << i << "].c2h:  " << unitConvert(devstat.c2h[i])  << "\n";
        }

#ifdef AXI_FIREWALL
        char cbuf[80];
        struct tm *ts;
        time_t temp;
        ostr << "\nFirewall Last Error Status:\n";
        for(unsigned i= 0; i < m_errinfo.mNumFirewalls; ++i) {
            ostr << "  " << std::setw(7) << i << ":      0x" << std::hex
                 << m_errinfo.mAXIErrorStatus[i].mErrFirewallStatus << std::dec << " "
                 << parseFirewallStatus(m_errinfo.mAXIErrorStatus[i].mErrFirewallStatus) ;
            if(m_errinfo.mAXIErrorStatus[i].mErrFirewallStatus != 0x0) {
                temp = (time_t)m_errinfo.mAXIErrorStatus[i].mErrFirewallTime;
                ts = localtime(&temp);
                strftime(cbuf, sizeof(cbuf), "%a %Y-%m-%d %H:%M:%S %Z",ts);
                ostr << ". Error occurred on " << cbuf;
            }
            ostr << "\n";
        }
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
            ostr << "\nXclbin ID:  0x" << fileReadBuf;
        } else { // xclbinid exists, but no data read or reported
            ostr << "WARNING: 'xclbinid' invalid, unable to report xclbinid. Has the bitstream been loaded? See 'xbutil program'.\n";
        }
        delete [] fileReadBuf;
        ifs.close();

        // get DDR bank count from mem_topology if possible
        unsigned numSupportedMems = numDDR;
        std::string mem_path = devPath + "/mem_topology";
        if( stat( mem_path.c_str(), &sb ) < 0 ) {
            std::cout << "ERROR: failed to stat " << mem_path << std::endl;
            return errno;
        }
        ifs.open( mem_path.c_str(), std::ifstream::binary );
        if( !ifs.good() ) {
            return errno;
        }
        int numBanks = 0;
        ifs.read((char*)&numBanks, sizeof(numBanks));
        ifs.seekg(0, ifs.beg);
        if( numBanks == 0 ) {
            ostr << "\nMem Topology:\n";
            ostr << "     -- none found --. See 'xbutil program'.\n";
        } else if( numBanks > 0 ) {
            int buf_size = sizeof(mem_topology)*numBanks + offsetof(mem_topology, m_mem_data) ;
            buf_size *= 2; //TODO: just double this for padding safety for now.
            const int fixed_w = 13;
            char* buffer = new char[ buf_size ];
            memset(buffer, 0, buf_size);
            ifs.read( buffer, buf_size);
            mem_topology *map;
            map = (mem_topology *)buffer;
            ostr << "\nMem Topology:\n";
            ostr << "     Tag" << "       Type" << "          Base Address" << "  Size (KB)\n";
            numDDR = map->m_count;
            if(numDDR <= 8) //Driver side limit, ddrMemUsed etc
                numSupportedMems = numDDR;
            for( unsigned i = 0; i < numDDR; i++ ) {
                ostr << " [" << i << "] " << std::setw(10) << std::left << map->m_mem_data[ i ].m_tag;
                //std::string str = "[" + std::to_string( i ) + "] ";
                //ostr << " " << std::setw( fixed_w - 6 ) << str.substr( 0, fixed_w ); // print bank
                //ostr << std::setw( fixed_w - 6 ) << map->m_mem_data[ i ].m_tag;
                //ostr << map->m_mem_data[ i ].m_tag;
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
                ostr << std::setw( fixed_w ) << str;
                std::stringstream ss;
                ss << "0x" << std::hex << map->m_mem_data[ i ].m_base_address;          // print base address
                ostr << " " << std::setw( fixed_w ) << ss.str().substr( 0, fixed_w );
                ss.str( "" );
                ss << "0x" << std::hex << map->m_mem_data[ i ].m_size;                 // print size
                ostr << " " << std::setw( fixed_w ) << ss.str().substr( 0, fixed_w ) << std::endl;
            }
            delete[] buffer;
        } else { // mem_topology exists, but no data read or reported
            ostr << "WARNING: 'mem_topology' invalid, unable to report topology. Has the bitstream been loaded? See 'xbutil program'." << std::endl;
        }
        ifs.close();

        ostr << "\nCompute Unit Status:\n";
        std::vector<ip_data> computeUnits;
        if( getComputeUnits( computeUnits ) < 0 ) {
            ostr << "WARNING: 'ip_layout' invalid. Has the bitstream been loaded? See 'xbutil program'.\n";
        } else {
            for( unsigned int i = 0; i < computeUnits.size(); i++ ) {
                static int cuCnt = 0;
                if( computeUnits.at( i ).m_type == IP_KERNEL ) {
                    unsigned statusBuf;
                    xclRead(m_handle, XCL_ADDR_KERNEL_CTRL, computeUnits.at( i ).m_base_address, &statusBuf, 4);
                    ostr << "  CU[" << cuCnt << "]: "
                         << computeUnits.at( i ).m_name
                         << "@0x" << std::hex << computeUnits.at( i ).m_base_address << " "
                         << std::dec << parseCUStatus( statusBuf ) << "\n";
                    cuCnt++;
                }
            }
            if(computeUnits.size() == 0) {
                ostr << "     -- none found --. See 'xbutil program'.\n";
            }
        }

        ostr << "\nDevice Memory Usage:" << "\n";
        for (unsigned i = 0; !result & (i < numSupportedMems); i++) {
            ostr << "  Bank[" << i << "].mem:  0x" << std::hex << devstat.ddrMemUsed[i] / 1024 << std::dec << " KB\n";
            ostr << "  Bank[" << i << "].bo:   " << devstat.ddrBOAllocated[i] << "\n";
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
            printf("Averaging ARE hardware latency over %lu hops\n", numIteration * numHops);
            printf("Latency per ARE hop for 128KB: %lu ns\n", delayPerHop);
            printf("Total latency over ARE: %lu ns\n", (timeARE - timeDDR));
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
