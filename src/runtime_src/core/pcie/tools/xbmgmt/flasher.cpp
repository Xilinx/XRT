/**
 * Copyright (C) 2018-2020 Xilinx, Inc
 * Author: Ryan Radjabi
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
#include "core/pcie/linux/scan.h"
#include "core/pcie/driver/linux/include/mgmt-reg.h"
#include <stddef.h>
#include <cassert>
#include <vector>
#include <string.h>

#define FLASH_BASE_ADDRESS BPI_FLASH_OFFSET
#define MAGIC_XLNX_STRING "xlnx" // from xclfeatures.h FeatureRomHeader

Flasher::E_FlasherType Flasher::getFlashType(std::string typeStr)
{
    std::string err;
    E_FlasherType type = E_FlasherType::UNKNOWN;

    if (typeStr.empty())
        mDev->sysfs_get("flash", "flash_type", err, typeStr);
    if (typeStr.empty())
        mDev->sysfs_get("", "flash_type", err, typeStr);

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
    else if (typeStr.compare("ospi_versal") == 0)
    {
        type = E_FlasherType::OSPIVERSAL;
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
    firmwareImage *primary, firmwareImage *secondary, firmwareImage *stripped)
{
    int retVal = -EINVAL;
    E_FlasherType type = getFlashType(flasherType);

    switch(type)
    {
    case SPI:
    {
        XSPI_Flasher xspi(mDev);
        if (primary == nullptr)
        {
            retVal = xspi.revertToMFG();
        }
        else if(secondary == nullptr)
        {
            retVal = xspi.xclUpgradeFirmware1(*primary, stripped);
        }
        else
        {
            retVal = xspi.xclUpgradeFirmware2(*primary, *secondary, stripped);
        }

        auto dev = mDev.get();
        std::string errmsg;
        std::string lvl = std::to_string(1);
        dev->sysfs_put("icap_controller", "enable", errmsg, lvl);
        if (errmsg.empty())
            std::cout << "Successfully enabled icap_controller" << std::endl;

        break;
    }
    case BPI:
    {
        BPI_Flasher bpi(mDev);
        if (primary == nullptr)
        {
            std::cout << "ERROR: BPI mode does not support reverting to MFG." << std::endl;
        }
        else if(secondary != nullptr)
        {
            std::cout << "ERROR: BPI mode does not support two mcs files." << std::endl;
        }
        else
        {
            retVal = bpi.xclUpgradeFirmware(*primary);
        }
        break;
    }
    case QSPIPS:
    {
        XQSPIPS_Flasher xqspi_ps(mDev);
        if (primary == nullptr)
        {
            retVal = xqspi_ps.revertToMFG();
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
        XOSPIVER_Flasher xospi_versal(mDev);
        if (primary == nullptr)
        {
            std::cout << "ERROR: OSPIVERSAL mode does not support reverting to MFG." << std::endl;
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
    XMC_Flasher flasher(mDev);

    if (!flasher.hasXMC())
        return -EOPNOTSUPP;

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

    for (unsigned i = 0; i < v.size() && v[i]!=0; i++)
    {
        ss << v[i];
    }
    return ss.str();
}

std::string int2PowerString(unsigned lvl)
{
    std::vector<std::string> powers{ "75W", "150W", "225W", "300W" };

    if (lvl < powers.size())
        return powers[lvl];

    return std::to_string(lvl);
}

int Flasher::getBoardInfo(BoardInfo& board)
{
    std::string unassigned_mac = "FF:FF:FF:FF:FF:FF";
    std::map<char, std::vector<char>> info;
    XMC_Flasher flasher(mDev);

    if (!flasher.hasXMC())
        return -EOPNOTSUPP;

    if (!flasher.probingErrMsg().empty())
    {
        std::cout << "ERROR: " << flasher.probingErrMsg() << std::endl;
        return -EOPNOTSUPP;
    }

    int ret = flasher.xclGetBoardInfo(info);
    if (ret != 0)
        return ret;

    board.mBMCVer = std::move(charVec2String(info[BDINFO_BMC_VER]));
    if (flasher.fixedSC())
        board.mBMCVer += "(FIXED)";
    board.mConfigMode = info.find(BDINFO_CONFIG_MODE) != info.end() ?
        info[BDINFO_CONFIG_MODE][0] : '\0';
    board.mFanPresence = info.find(BDINFO_FAN_PRESENCE) != info.end() ?
        info[BDINFO_FAN_PRESENCE][0] : '\0';

    if (info.find(BDINFO_MAC_DYNAMIC) != info.end() &&
        info[BDINFO_MAC_DYNAMIC].size() == 8) {
        uint16_t count;

        memcpy(&count, &info[BDINFO_MAC_DYNAMIC][0], sizeof(uint16_t));
        board.mMacContiguousNum = le16toh(count);

	for (unsigned i = 2; i < 8; i++) {
            board.mMacAddrFirst[i-2] = info[BDINFO_MAC_DYNAMIC][i];
	}
    } else {
        board.mMacContiguousNum = 0;
        board.mMacAddr0 = charVec2String(info[BDINFO_MAC0]).compare(unassigned_mac) ? 
            std::move(charVec2String(info[BDINFO_MAC0])) :
	    std::move(std::string("Unassigned"));
        board.mMacAddr1 = charVec2String(info[BDINFO_MAC1]).compare(unassigned_mac) ? 
            std::move(charVec2String(info[BDINFO_MAC1])) :
	    std::move(std::string("Unassigned"));
        board.mMacAddr2 = charVec2String(info[BDINFO_MAC2]).compare(unassigned_mac) ? 
            std::move(charVec2String(info[BDINFO_MAC2])) :
	    std::move(std::string("Unassigned"));
        board.mMacAddr3 = charVec2String(info[BDINFO_MAC3]).compare(unassigned_mac) ? 
            std::move(charVec2String(info[BDINFO_MAC3])) :
	    std::move(std::string("Unassigned"));
    }

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
    auto dev = pcidev::get_dev(index, false);
    if(dev == nullptr) {
        std::cout << "ERROR: Invalid card index:" << index << std::endl;
        return;
    }

    std::string err;
    bool is_mfg = false;
    dev->sysfs_get<bool>("", "mfg", err, is_mfg, false);

    std::vector<char> feature_rom;
    dev->sysfs_get("rom", "raw", err, feature_rom);
    if (err.empty())
    {
        memcpy(&mFRHeader, feature_rom.data(), sizeof(struct FeatureRomHeader));
        // Something funny going on here. There must be a strange line ending
        // character. Using "<" will check for a match that EntryPointString
        // starts with magic char sequence "xlnx".
        if(std::string(reinterpret_cast<const char*>(mFRHeader.EntryPointString))
            .compare(0, 4, MAGIC_XLNX_STRING) != 0)
        {
            std::cout << "ERROR: Failed to detect feature ROM." << std::endl;
        }
    }
    else if (is_mfg)
    {
        dev->sysfs_get<unsigned>("", "mfg_ver", err, mGoldenVer, 0);
    }
    else
    {
        std::string vbnv;
        dev->sysfs_get("rom", "VBNV", err, vbnv);
        if (err.empty())
            memcpy(mFRHeader.VBNVName, vbnv.c_str(), vbnv.size());
        else
            std::cout << "ERROR: card not supported." << std::endl;
    }

    mDev = dev; // Successfully initialized
}

/*
 * getProgrammingTypeFromDeviceName
 */
int Flasher::getProgrammingTypeFromDeviceName(unsigned char name[], E_FlasherType &type)
{
    std::string strDsaName( reinterpret_cast<const char*>(name) );
    bool typeFound = false;
    for( unsigned i = 0; i < flashPairs.size(); i++ )
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

    uint16_t vendor_id, device_id;
    mDev->sysfs_get<uint16_t>("", "vendor", err, vendor_id, -1);
    if (!err.empty())
    {
        std::cout << err << std::endl;
        return DSAs;
    }
    mDev->sysfs_get<uint16_t>("", "device", err, device_id, -1);
    if (!err.empty())
    {
        std::cout << err << std::endl;
        return DSAs;
    }

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

    std::string err;
    std::string board_name;
    std::string uuid;
    bool is_mfg = false;
    mDev->sysfs_get<bool>("", "mfg", err, is_mfg, false);
    mDev->sysfs_get("", "board_name", err, board_name);
    mDev->sysfs_get("rom", "uuid", err, uuid);
    if (is_mfg)
    {
        std::stringstream ss;

        ss << "xilinx_" << board_name << "_GOLDEN_" << mGoldenVer;
        vbnv = ss.str();
    }
    else if (mFRHeader.VBNVName[0] != '\0')
    {
        vbnv = std::string(reinterpret_cast<char *>(mFRHeader.VBNVName));
        ts = mFRHeader.TimeSinceEpoch;
    }
    else if (!uuid.size())
    {
        std::cout << "ERROR: Platform name not found" << std::endl;
    }

    BoardInfo info;
    int rc = getBoardInfo(info);
    if (rc == 0)
        bmc = info.mBMCVer; // Successfully read BMC version
    else if (rc == -EOPNOTSUPP)
        bmc = DSAInfo::INACTIVE; // BMC is not supported on DSA, state is inactive
    else
        bmc = DSAInfo::UNKNOWN; // BMC not ready, set it to an invalid version string

    return DSAInfo(vbnv, ts, uuid, bmc);
}

std::string Flasher::sGetDBDF()
{
    char cDBDF[128];

    sprintf(cDBDF, "%.4x:%.2x:%.2x.%.1x",
        mDev->domain, mDev->bus, mDev->dev, mDev->func);
    return std::string(cDBDF);
}

int Flasher::readData(std::vector<unsigned char>& data)
{
    XSPI_Flasher xspi(mDev);
    return xspi.xclReadData(data);
}

int Flasher::writeData(std::vector<unsigned char>& data)
{
    XSPI_Flasher xspi(mDev);
    return xspi.xclWriteData(data);
}
