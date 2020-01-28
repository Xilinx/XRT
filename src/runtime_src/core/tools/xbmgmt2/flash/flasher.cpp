/**
 * Copyright (C) 2019 Xilinx, Inc
 *
 * This is a wrapper class that does the prep work required to program a flash
 * device. Flasher will create a specific flash object determined by the program
 * mode read from FeatureROM. Common functions between XSPI_Flasher and BPI_Flasher
 * are implemented here.
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
#include "flasher.h"
#include <limits>
#include <cstddef>
#include <cassert>
#include <vector>
#include <cstring>
#include <cstdarg>
#include "boost/format.hpp"

#include "core/common/error.h"

#define INVALID_ID      0xffff
#define MFG_REV_OFFSET  0x131008 // For obtaining Golden image version number

#define FLASH_BASE_ADDRESS BPI_FLASH_OFFSET
#define MAGIC_XLNX_STRING "xlnx" // from xclfeatures.h FeatureRomHeader

Flasher::E_FlasherType Flasher::getFlashType(std::string typeStr)
{
    std::string err;
    E_FlasherType type = E_FlasherType::UNKNOWN;
    if (typeStr.empty())
        typeStr = xrt_core::query_device<std::string>(m_device, xrt_core::device::QR_F_FLASH_TYPE);
    if (typeStr.empty())
        typeStr = xrt_core::query_device<std::string>(m_device, xrt_core::device::QR_FLASH_TYPE);

    if (typeStr.empty())
    {
        getProgrammingTypeFromDeviceName(mFRHeader.VBNVName, type);
    }
    else if (typeStr.compare("spi") == 0)
    {
        type = E_FlasherType::SPI;
    }
    else if (typeStr.compare("bpi") == 0)
    {
        type = E_FlasherType::BPI;
    }
    else if (typeStr.find("qspi_ps") == 0)
    {
        // Use find() for this type of flash.
        // Since it have variations
        type = E_FlasherType::QSPIPS;
    }
    else
    {
        std::cout << "Unknown flash type: " << typeStr << std::endl;
    }

    return type;
}

/*
 * upgradeFirmware
 */
int Flasher::upgradeFirmware(const std::string& flasherType,
    firmwareImage *primary, firmwareImage *secondary)
{
    int retVal = -EINVAL;
    E_FlasherType type = getFlashType(flasherType);

    switch(type)
    {
    case SPI:
    {
        XSPI_Flasher xspi(m_device);
        if (primary == nullptr)
        {
            retVal = xspi.revertToMFG();
        }
        else if(secondary == nullptr)
        {
            retVal = xspi.xclUpgradeFirmware1(*primary);
        }
        else
        {
            retVal = xspi.xclUpgradeFirmware2(*primary, *secondary);
        }
        break;
    }
    // case BPI:
    // {
    //     BPI_Flasher bpi(m_device);
    //     if (primary == nullptr)
    //     {
    //         std::cout << "ERROR: BPI mode does not support reverting to MFG." << std::endl;
    //     }
    //     else if(secondary != nullptr)
    //     {
    //         std::cout << "ERROR: BPI mode does not support two mcs files." << std::endl;
    //     }
    //     else
    //     {
    //         retVal = bpi.xclUpgradeFirmware(*primary);
    //     }
    //     break;
    // }
    // case QSPIPS:
    // {
    //     XQSPIPS_Flasher xqspi_ps(m_device);
    //     if (primary == nullptr)
    //     {
    //         std::cout << "ERROR: QSPIPS mode does not support reverting to MFG." << std::endl;
    //     }
    //     else if(secondary != nullptr)
    //     {
    //         std::cout << "ERROR: QSPIPS mode does not support two mcs files." << std::endl;
    //     }
    //     else
    //     {
    //         retVal = xqspi_ps.xclUpgradeFirmware(*primary);
    //     }
    //     break;
    // }
    default:
        break;
    }
    return retVal;
}

int Flasher::upgradeBMCFirmware(firmwareImage* bmc)
{
    XMC_Flasher flasher(m_device->get_device_id());
    const std::string e = flasher.probingErrMsg();

    if (!e.empty())
    {
        std::cout << "ERROR: " << e << std::endl;
        return -EOPNOTSUPP;
    }

    return flasher.xclUpgradeFirmware(*bmc);
}

std::string charVec2String(std::vector<char>& v)
{
    std::stringstream ss;

    for (unsigned int i = 0; i < v.size() && v[i]!=0; i++)
    {
        ss << v[i];
    }
    return ss.str();
}

std::string int2PowerString(unsigned int lvl)
{
    std::vector<std::string> powers{ "75W", "150W", "225W" };

    if (lvl < powers.size())
        return powers[lvl];

    return std::to_string(lvl);
}

int Flasher::getBoardInfo(BoardInfo& board)
{
    std::map<char, std::vector<char>> info;
    XMC_Flasher flasher(m_device->get_device_id());

    if (!flasher.probingErrMsg().empty())
    {
        std::cout << "ERROR: " << flasher.probingErrMsg() << std::endl;
        return -EOPNOTSUPP;
    }

    int ret = flasher.xclGetBoardInfo(info);
    if (ret != 0)
        return ret;

    board.mBMCVer = std::move(charVec2String(info[BDINFO_BMC_VER]));
    board.mConfigMode = info[BDINFO_CONFIG_MODE][0];
    board.mFanPresence = info[BDINFO_FAN_PRESENCE][0];
    board.mMacAddr0 = std::move(charVec2String(info[BDINFO_MAC0]));
    board.mMacAddr1 = std::move(charVec2String(info[BDINFO_MAC1]));
    board.mMacAddr2 = std::move(charVec2String(info[BDINFO_MAC2]));
    board.mMacAddr3 = std::move(charVec2String(info[BDINFO_MAC3]));
    board.mMaxPower = int2PowerString(info[BDINFO_MAX_PWR][0]);
    board.mName = std::move(charVec2String(info[BDINFO_NAME]));
    board.mRev = std::move(charVec2String(info[BDINFO_REV]));
    board.mSerialNum = std::move(charVec2String(info[BDINFO_SN]));

    return ret;
}

/*
 * constructor
 */
Flasher::Flasher(unsigned int index) : mFRHeader{}
{
    auto dev = xrt_core::get_mgmtpf_device(index);
    if(dev == nullptr) {
        std::cout << "ERROR: Invalid card index:" << index << std::endl;
        return;
    }

    bool is_mfg = false;
    is_mfg = xrt_core::query_device<bool>(dev, xrt_core::device::QR_IS_MFG);

    // std::vector<char> feature_rom;
    // feature_rom = xrt_core::query_device<std::vector<char>>(dev, xrt_core::device::QR_ROM_RAW);
    // if (feature_rom != xrt_core::invalid_query_value<std::vector<char>>())
    // {
    //     memcpy(&mFRHeader, feature_rom.data(), sizeof(struct FeatureRomHeader));
    //     // Something funny going on here. There must be a strange line ending
    //     // character. Using "<" will check for a match that EntryPointString
    //     // starts with magic char sequence "xlnx".
    //     if(std::string(reinterpret_cast<const char*>(mFRHeader.EntryPointString))
    //         .compare(0, 4, MAGIC_XLNX_STRING) != 0)
    //     {
    //         std::cout << "ERROR: Failed to detect feature ROM." << std::endl;
    //     }
    // }
    // else if (is_mfg)
    if (is_mfg)
    {
       dev->read(MFG_REV_OFFSET, &mGoldenVer, sizeof(mGoldenVer));
    }
    //else
    //{
    //    std::cout << "ERROR: card not supported." << std::endl;
    //}
    m_device = dev; // Successfully initialized
}

/*
 * getProgrammingTypeFromDeviceName
 */
int Flasher::getProgrammingTypeFromDeviceName(unsigned char name[], E_FlasherType &type)
{
    std::string strDsaName( reinterpret_cast<const char*>(name) );
    bool typeFound = false;
    for( unsigned int i = 0; i < flashPairs.size(); i++ )
    {
        auto pair = flashPairs.at( i );
        if( strDsaName.find( pair.first ) != std::string::npos )
        {
            type = pair.second;
            typeFound = true;
            break;
        }
    }
    if( !typeFound )
    {
        return -EINVAL;
    }
    return 0;
}

/*
 * Obtain all DSA installed on the system for this board
 */
std::vector<DSAInfo> Flasher::getInstalledDSA()
{
    std::vector<DSAInfo> DSAs;

    // Obtain board info.
    DSAInfo onBoard = getOnBoardDSA();
    std::string err;

    if (onBoard.name.empty() && onBoard.uuids.empty())
    {
        std::cout << "Shell on FPGA is unknown" << std::endl;
        return DSAs;
    }

    uint64_t vendor_id, device_id;
    vendor_id = xrt_core::query_device<uint64_t>(m_device, xrt_core::device::QR_PCIE_VENDOR);
    if (vendor_id == xrt_core::invalid_query_value<uint64_t>()) 
        return DSAs;
    device_id = xrt_core::query_device<uint64_t>(m_device, xrt_core::device::QR_PCIE_DEVICE);
    if (device_id == xrt_core::invalid_query_value<uint64_t>()) 
        return DSAs;

    // Obtain installed DSA info.
    // std::cout << "ON Board: " << onBoard.vendor << " " << onBoard.board << " " << vendor_id << " " << device_id << std::endl;
    auto installedDSAs = firmwareImage::getIntalledDSAs();
    for (DSAInfo& dsa : installedDSAs)
    {
        // std::cout << "DSA " << dsa.name << ": " << dsa.vendor << " " << dsa.board << " " << dsa.vendor_id << " " << dsa.device_id << "TS: " << dsa.timestamp << std::endl;
        if (!dsa.hasFlashImage || dsa.timestamp == NULL_TIMESTAMP)
	       continue;

        if (!onBoard.vendor.empty() && !onBoard.board.empty() &&
            (onBoard.vendor == dsa.vendor) &&
            (onBoard.board == dsa.board))
        {
            DSAs.push_back(dsa);
        }
	else if (!dsa.name.empty() && (vendor_id == dsa.vendor_id) && (device_id == dsa.device_id))
        {
            DSAs.push_back(dsa);
        } else if (onBoard.name.empty())
            DSAs.push_back(dsa);

    }

    return DSAs;
}

DSAInfo Flasher::getOnBoardDSA()
{
    std::string vbnv;
    std::string bmc;
    uint64_t ts = NULL_TIMESTAMP;

    std::string board_name;
    std::string uuid;
    bool is_mfg = false;

    is_mfg = xrt_core::query_device<bool>(m_device, xrt_core::device::QR_IS_MFG);
    board_name = xrt_core::query_device<std::string>(m_device, xrt_core::device::QR_BOARD_NAME);
    
    if (is_mfg)
    {
        std::stringstream ss;

        ss << "xilinx_" << board_name << "_GOLDEN_" << mGoldenVer;
        vbnv = ss.str();
    }
    //else if (mFRHeader.VBNVName[0] != '\0')
    //{
    //    vbnv = std::string(reinterpret_cast<char *>(mFRHeader.VBNVName));
    //    ts = mFRHeader.TimeSinceEpoch;
    //}
    else{
        vbnv = xrt_core::query_device<std::string>(m_device, xrt_core::device::QR_ROM_VBNV);
        ts = xrt_core::query_device<uint64_t>(m_device, xrt_core::device::QR_ROM_TIME_SINCE_EPOCH);
        uuid = xrt_core::query_device<std::string>(m_device, xrt_core::device::QR_ROM_UUID);
        if (vbnv.empty())
            throw xrt_core::error("Platform not found. Invalid device name.");
        if(ts == std::numeric_limits<uint64_t>::max())
            throw xrt_core::error("Platform not found. Invalid timestamp");
    }

    BoardInfo info;
    int rc = getBoardInfo(info);
    if (rc == 0)
        bmc = info.mBMCVer; // Successfully read BMC version
    else if (rc == -EOPNOTSUPP)
        bmc.clear(); // BMC is not supported on DSA
    else
        bmc = "UNKNOWN"; // BMC not ready, set it to an invalid version string

    return DSAInfo(vbnv, ts, uuid, bmc);
}

std::string Flasher::sGetDBDF()
{
    auto bus = xrt_core::query_device<uint16_t>(m_device, xrt_core::device::QR_PCIE_BDF_BUS);
    auto dev = xrt_core::query_device<uint16_t>(m_device, xrt_core::device::QR_PCIE_BDF_DEVICE);
    auto func = xrt_core::query_device<uint16_t>(m_device, xrt_core::device::QR_PCIE_BDF_FUNCTION);
    return boost::str(boost::format("%04x:%02x:%02x.%01x") % 0 % bus % dev % func);
}
