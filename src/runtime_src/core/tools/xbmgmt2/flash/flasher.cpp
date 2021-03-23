/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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
#include "core/common/error.h"
#include "core/common/query_requests.h"
#include <limits>
#include <cstddef>
#include <cassert>
#include <vector>
#include <cstring>
#include <cstdarg>
#include "boost/format.hpp"
#include <boost/algorithm/string.hpp>
#include "boost/filesystem.hpp"


#define INVALID_ID      0xffff

#define FLASH_BASE_ADDRESS BPI_FLASH_OFFSET
#define MAGIC_XLNX_STRING "xlnx" // from xclfeatures.h FeatureRomHeader

/* 
 * map flash_tyoe str to E_FlasherType
 */
Flasher::E_FlasherType Flasher::typeStr_to_E_FlasherType(const std::string& typeStr)
{    
    E_FlasherType type = E_FlasherType::UNKNOWN;
    if (typeStr.compare("spi") == 0) {
        type = E_FlasherType::SPI;
    }
    else if (typeStr.compare("bpi") == 0) {
        type = E_FlasherType::BPI;
    }
    else if (typeStr.find("qspi_ps") == 0) {
        // Use find() for this type of flash.
        // Since it have variations
        type = E_FlasherType::QSPIPS;
    }
    else if (typeStr.compare("ospi_versal") == 0) {
        type = E_FlasherType::OSPIVERSAL;
    }
    return type;
}

Flasher::E_FlasherType Flasher::getFlashType(std::string typeStr)
{
    std::string err;
    E_FlasherType type = E_FlasherType::UNKNOWN;

    // check various locations for flash_type
    // the node could either be present in flash subdev or exist independently
    // if the node is not found, then look in feature rom header
    try {
        if (typeStr.empty())
            typeStr = xrt_core::device_query<xrt_core::query::f_flash_type>(m_device);
    } catch (...) {}
    try {
        if (typeStr.empty())
          typeStr = xrt_core::device_query<xrt_core::query::flash_type>(m_device);
    } catch (...) {}
    try {
        if (typeStr.empty())
            getProgrammingTypeFromDeviceName(mFRHeader.VBNVName, type);
    } catch (...) {}
    
    type = typeStr_to_E_FlasherType(typeStr);
    if(type == E_FlasherType::UNKNOWN)
        throw xrt_core::error(boost::str(boost::format("Unknown flash type: %s") % typeStr));

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
    case BPI:
    {
        std::cout << "ERROR: BPI mode is no longer supported." << std::endl;
        break;
    }
    case QSPIPS:
    {
        XQSPIPS_Flasher xqspi_ps(m_device);
        if (primary == nullptr)
        {
            std::string golden_file = getQspiGolden();
            if (golden_file.empty()) {
                std::cout << "ERROR: Golden image not found in base package. Can't revert to golden" << std::endl;
                return -ECANCELED;
            }
            std::shared_ptr<firmwareImage> golden_image;
            golden_image = std::make_shared<firmwareImage>(golden_file.c_str(), MCS_FIRMWARE_PRIMARY);
            retVal = xqspi_ps.revertToMFG(*golden_image);
        }
        else
        {
	    if(secondary != nullptr)
            	std::cout << "Warning: QSPIPS mode does not support secondary file." << std::endl;
            retVal = xqspi_ps.xclUpgradeFirmware(*primary);
        }
        break;
    }
    case OSPIVERSAL:
    {
        XOSPIVER_Flasher xospi_versal(m_device);
        if (primary == nullptr)
        {
            std::cout << "ERROR: OSPIVERSAL mode does not support reverting to MFG." << std::endl;
        }
        else if(secondary != nullptr)
        {
            std::cout << "ERROR: OSPIVERSAL mode does not support two mcs files." << std::endl;
        }
        else
        {
            retVal = xospi_versal.xclUpgradeFirmware(*primary);
        }
        break;
    }
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
        throw xrt_core::error(e);

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
    std::vector<std::string> powers{ "75W", "150W", "225W", "300W" };

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

    std::string unassigned_mac = "FF:FF:FF:FF:FF:FF";
    board.mBMCVer = std::move(charVec2String(info[BDINFO_BMC_VER]));
    if (flasher.fixedSC())
        board.mBMCVer += "(FIXED)";
    board.mConfigMode = info.find(BDINFO_CONFIG_MODE) != info.end() ?
        info[BDINFO_CONFIG_MODE][0] : '\0';
    board.mFanPresence = info.find(BDINFO_FAN_PRESENCE) != info.end() ?
        info[BDINFO_FAN_PRESENCE][0] : '\0';
    board.mMacAddr0 = charVec2String(info[BDINFO_MAC0]).compare(unassigned_mac) ? 
        std::move(charVec2String(info[BDINFO_MAC0])) : std::move(std::string("Unassigned"));
    board.mMacAddr1 = charVec2String(info[BDINFO_MAC1]).compare(unassigned_mac) ? 
        std::move(charVec2String(info[BDINFO_MAC1])) : std::move(std::string("Unassigned"));
    board.mMacAddr2 = charVec2String(info[BDINFO_MAC2]).compare(unassigned_mac) ? 
        std::move(charVec2String(info[BDINFO_MAC2])) : std::move(std::string("Unassigned"));
    board.mMacAddr3 = charVec2String(info[BDINFO_MAC3]).compare(unassigned_mac) ? 
        std::move(charVec2String(info[BDINFO_MAC3])) : std::move(std::string("Unassigned"));
    board.mMaxPower = info.find(BDINFO_MAX_PWR) != info.end() ?
        int2PowerString(info[BDINFO_MAX_PWR][0]) : "N/A";
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

    bool is_mfg = xrt_core::device_query<xrt_core::query::is_mfg>(dev);

    // std::vector<char> feature_rom;
    // auto feature_rom = xrt_core::device_device<xrt_core::query::rom_raw>(dev);
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
    if (is_mfg) {
        try {
            mGoldenVer = xrt_core::device_query<xrt_core::query::mfg_ver>(dev);
        } catch (...) {}
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
    }

    auto vendor_id = xrt_core::device_query<xrt_core::query::pcie_vendor>(m_device);
    auto device_id = xrt_core::device_query<xrt_core::query::pcie_device>(m_device);

    // Obtain installed DSA info.
    // std::cout << "ON Board: " << onBoard.vendor << " " << onBoard.board << " " << vendor_id << " " << device_id << std::endl;
    auto installedDSAs = firmwareImage::getIntalledDSAs();
    for (const auto& dsa : installedDSAs)
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

    std::string uuid;

    bool is_mfg = xrt_core::device_query<xrt_core::query::is_mfg>(m_device);
    std::string board_name = xrt_core::device_query<xrt_core::query::board_name>(m_device);

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
      vbnv = xrt_core::device_query<xrt_core::query::rom_vbnv>(m_device);
      ts = xrt_core::device_query<xrt_core::query::rom_time_since_epoch>(m_device);
      uuid = xrt_core::device_query<xrt_core::query::rom_uuid>(m_device);
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

/*
 * For card with qspi flash like u30, u25, the golden image is supposed to
 * be flashed at offset 96MB when the card is shipped. however, for some
 * historical reason, the golden on some board is flashed at offset 0. As
 * a result, the default revert_to_golden, which erases 0-96MB will brick the
 * board. To avoid that, before revert_to_golden, flasher will check whether
 * there is golden at 96MB. To do that, the base pkg will also install the
 * golden file at, eg 
 * /opt/xilinx/firmware/u30/gen3x4/base/data/BOOT_golden.BIN
 * then flasher will compare the contents on flash at 96MB with the file to
 * see whether the golden is there.
 * Note: xrt doesn't support flash golden. and once the card is shipped with
 * golden, we assume all versions of shell pkg have same golden -- new pkg
 * just installs the golden at same location
 * If the golden file is not found on disk, or the golden image is not found 
 * at 96MB, we don't do revert_to_golden.
 */
std::string Flasher::getQspiGolden()
{
    std::string board_name = xrt_core::device_query<xrt_core::query::board_name>(m_device);
    if (board_name.empty())
        return "";

    std::string start = FORMATTED_FW_DIR;
    start += "/";
    start += board_name;
    boost::filesystem::recursive_directory_iterator dir(start), end;
    while (dir != end) {
        std::string fn = dir->path().filename().string();
        if (!fn.compare(QSPI_GOLDEN_IMAGE)) {
            return dir->path().string();
        }
        dir++;
    }
    return "";
}

std::string Flasher::sGetDBDF()
{
  auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(m_device);
  return xrt_core::query::pcie_bdf::to_string(bdf);
}
