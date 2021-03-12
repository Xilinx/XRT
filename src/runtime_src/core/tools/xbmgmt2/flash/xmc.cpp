/**
 * Copyright (C) 2019-2021 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
// ------ I N C L U D E   F I L E S -------------------------------------------
#ifdef __GNUC__
#define XMC_UNUSED __attribute__((unused))
#else
#define XMC_UNUSED
#endif

#ifdef _WIN32
#pragma warning(disable : 4189 4100 4996)
#endif

// Local - Include Files
#include "xmc.h"
#include "flasher.h"
#include "core/common/utils.h"
#include "core/common/device.h"
#include "core/common/query_requests.h"
#include "core/tools/common/XBUtilities.h"
#include "core/tools/common/ProgressBar.h"

// 3rd Party Library - Include Files
#include "boost/format.hpp"

// System - Include Files
#include <thread>
#include <fcntl.h>

// ------ S T A T I C   V A R I A B L E S -------------------------------------
//#define XMC_DEBUG
static const int bmc_jump_address = 0x201; /* Hard-coded for now */

// ------ S T A T I C   F U N C T I O N S -------------------------------------
static std::map<int, std::string> scStatusMap = {
  {0, "NOT READY"},
  {1, "READY"},
  {2, "BSL_UNSYNCED"},
  {3, "BSL_SYNCED"},
  {4, "BSL_SYNCED_SC_NOT_UPGRADABLE"},
  {5, "READY_SC_NOT_UPGRADABLE"},
};

static std::map<int, std::string> cmcStatusMap = {
  {0, "NOT READY"},
  {1, "READY"},
  {2, "STOPPED"},
  {4, "PAUSED"},
};

static std::string getStatus(int status, std::map<int, std::string> &map)
{
  auto entry = map.find(status);
  std::ostringstream os;

  os << std::hex << status;

  if (entry != map.end())
    os << "(" << entry->second << ")";

  return os.str();
}

static void tiTxtStreamToBin(std::istream& tiTxtStream,
    unsigned int& currentAddr, std::vector<unsigned char>& buf)
{
  // offset to write to XMC device to set SC jump address.
  const unsigned int jumpOffset = 0xffffffff;
  // SC jump address is hard-coded.
  const unsigned int jumpAddr = 0x201;
  bool sectionEnd = false;

  buf.clear();

  while (!sectionEnd) {
    std::string line;
    std::string sectionEndChar("@qQ"); // any char will mark end of section

    // Check if we're done with current section.
    char nextChar = static_cast<unsigned char>(tiTxtStream.peek());
    sectionEnd = (
        (sectionEndChar.find(nextChar) != std::string::npos && !buf.empty())
        || (nextChar == EOF));
    if (sectionEnd)
        break;

    // Skip empty lines.
    std::getline(tiTxtStream, line);
    if (line.size() == 0)
        continue;

    switch (line[0]) {
    case '@':
      // Address line
      try {
          currentAddr = std::stoi(line.substr(1), NULL, 16);
      } catch (...){
          std::cout << "ERROR: Invalid address " << line.substr(1) << ". No action taken" << std::endl;
          return;
      }
      break;
    case 'q':
    case 'Q':
    {
      // End of image, return jump section.
      currentAddr = jumpOffset;
      auto *tmp = reinterpret_cast<const unsigned char *>(&jumpAddr);
      for (unsigned int i = 0; i < sizeof(jumpAddr); i++)
          buf.push_back(tmp[i]);
      sectionEnd = true;
      break;
    }
    default:
      // Data line
      std::stringstream ss(line);
      std::string token;
      unsigned char int_token;
      while (std::getline(ss, token, ' ')) {
        try {
          int_token = static_cast<unsigned char>(std::stoi(token, NULL, 16));
        } catch (...) {
          std::cout << "ERROR: Invalid address " << token << ". No action taken" << std::endl;
          return;
        }
        buf.push_back(int_token);
      }
      break;
    }
    }
}

static int writeImage(std::FILE *xmcDev,
    unsigned int addr, std::vector<unsigned char>& buf)
{
  int ret = 0;
  size_t len = 0;
  const size_t max_write = 4050; // Max size per write

  ret = std::fseek(xmcDev, addr, SEEK_SET);
  if (ret)
    return ret;

  // Write SC image to xmc and print '.' for each write as progress indicator
  for (size_t i = 0; ret == 0 && i < buf.size(); i += len) {
    len = std::min(max_write, buf.size() - i);

    std::cout << "." << std::flush;

    std::size_t s = std::fwrite(buf.data() + i, 1, len, xmcDev);
    if (s != len)
      ret = -ferror(xmcDev);
    if (std::fflush(xmcDev))
      ret = -ferror(xmcDev);
  }
  return ret;
}

namespace xrt_tools {

// ------ M E M B E R   F U N C T I O N S -------------------------------------
int XMC_Flasher::xmc_mode()
{
  return readReg(static_cast<unsigned int>(xmc_reg_offset::status)) & 0x3;
}

int XMC_Flasher::bmc_mode()
{
  return readReg(static_cast<unsigned int>(xmc_reg_offset::status)) >> 28;
}

XMC_Flasher::XMC_Flasher(unsigned int device_index)
  : m_device(xrt_core::get_mgmtpf_device(device_index))
{
  uint64_t val = 0;
  mPktBufOffset = 0;
  mPkt = {};

  /*
   * If xmc subdev is not online, do not allow xmc flash operations.  In the
   * future, we will use xmc subdev to do xmc validation and flashing at one
   * place.
   *
   * NOTE: we don't build mProbingErrMsg to differentiate "no xmc subdev" and
   * "other errors". Caller can treat no error message as just not support.
   */
  if (!hasXMC())
    return;

  bool is_mfg = xrt_core::device_query<xrt_core::query::is_mfg>(m_device);
  if (!is_mfg) {
    try {
      val = xrt_core::device_query<xrt_core::query::xmc_status>(m_device);
    }
    catch (...) {
      return;
    }
    if (!(val & 1)) {
      mProbingErrMsg << "Failed to detect XMC, xmc.bin not loaded";
      return;
    }
  }

  try {
    mRegBase = xrt_core::device_query<xrt_core::query::xmc_reg_base>(m_device);
  }
  catch (...) { }
  if (mRegBase == 0)
    mRegBase = xmc_reg_base;

  try {
    val = readReg(static_cast<unsigned int>(xmc_reg_offset::magic));
  } catch (...) {
    // Xoclv2 driver does not support mmap'ed BAR access from
    // user space any more. We must use driver to update SC image.
    xrt_core::scope_value_guard<int, std::function<void()>> fd { 0, nullptr };
    try {
      fd = m_device->file_open("xmc", O_RDWR); 
    } catch (...) { }
    if (fd.get() >= 0)
      mXmcDev = fdopen(fd.get(), "r+");
    if (mXmcDev == nullptr)
        std::cout << "Failed to open XMC device" << std::endl;
    return;
  }

  if (val != xmc_magic_num) {
    mProbingErrMsg << "Failed to detect XMC, bad magic number: "
             << std::hex << val << std::dec;
    return;
  }

  val = readReg(static_cast<unsigned int>(xmc_reg_offset::version));
  if (val < xmc_base_version) {
    mProbingErrMsg << "Found unsupported XMC version: " << val;
    return;
  }

  val = readReg(static_cast<unsigned int>(xmc_reg_offset::feature));
  if (val & static_cast<unsigned int>(xmc_mask::pkt_support)) {
    mProbingErrMsg << "XMC packet buffer is not supported";
    return;
  }

  mPktBufOffset = readReg(static_cast<unsigned int>(xmc_reg_offset::packet_offset));

  mXmcDev = nullptr;
  xrt_core::scope_value_guard<int, std::function<void()>> fd { 0, nullptr };
  if (std::getenv("FLASH_VIA_USER") == NULL) {
    try {
      fd = m_device->file_open("xmc", O_RDWR); 
    } catch (...) { }
    if (fd.get() >= 0)
      mXmcDev = fdopen(fd.get(), "r+");
    if (mXmcDev == nullptr) {
      try {
        fd = m_device->file_open("xmc.u2", O_RDWR); 
      } catch (...) { }
      if (fd.get() >= 0)
        mXmcDev = fdopen(fd.get(), "r+");
    }
    if (mXmcDev == nullptr)
      std::cout << "Failed to open XMC device" << std::endl;
  }
}

XMC_Flasher::~XMC_Flasher()
{
}

/*
 * xclUpgradeFirmware
 */
int XMC_Flasher::xclUpgradeFirmware(std::istream &tiTxtStream)
{
  if (mXmcDev)
    return xclUpgradeFirmwareDrv(tiTxtStream);
  
  std::string startAddress;
  ELARecord record;
  bool endRecordFound = false;
  bool errorFound = false;
  int retries = 5;
  int ret = 0;

  if (!hasSC()) {
    std::cout << "ERROR: SC is not present on platform" << std::endl;
    return -EINVAL;
  }

  if (!isXMCReady())
    return -EINVAL;

  while (!tiTxtStream.eof() && !endRecordFound && !errorFound) {
    std::string line;
    std::getline(tiTxtStream, line);
    if (line.size() == 0) {
      continue;
    }

    switch (line[0]) {
    case 'q':
    case 'Q': {
      if (startAddress.size()) {
        // Finish the last record
        mRecordList.push_back(record);
        startAddress.clear();
      }
      // Create and append the end-of-image record (mDataCount must be 0).
      record.mStartAddress = bmc_jump_address;
      record.mDataPos = tiTxtStream.tellg();
      record.mEndAddress = record.mStartAddress;
      record.mDataCount = 0;
      mRecordList.push_back(record);
      endRecordFound = true;
      break;
    }
    case '@': {
      std::string newAddress = line.substr(1);
      if (startAddress.size())
      {
        // Finish the last record
        mRecordList.push_back(record);
        startAddress.clear();
      }
      // Start a new record
      record.mStartAddress = std::stoi(newAddress, 0, 16);
      record.mDataPos = tiTxtStream.tellg();
      record.mEndAddress = record.mStartAddress;
      record.mDataCount = 0;
      startAddress = newAddress;
      break;
    }
    default: {
      int spaces = 0;
      int digits = 0;
      std::locale loc;

      if (startAddress.size() == 0) {
        errorFound = true;
      }

      for (unsigned int i = 0; i < line.size() && !errorFound; i++) {
        if (line[i] == ' ')
        {
          spaces++;
        }
        else if (std::isxdigit(line[i], loc))
        {
          digits++;
        }
        else
        {
          errorFound = true;
        }
      }

      // Each line has at most 16-byte of data represented as hex in ASCII
      if (((digits % 2) != 0) || digits > 16 * 2) {
        errorFound = true;
      }

      if (!errorFound) {
        int bytes = digits / 2;

        record.mDataCount += bytes;
        record.mEndAddress += bytes;
        if (bytes < 16) {
          // Finish the last record
          mRecordList.push_back(record);
          startAddress.clear();
        }
      }
    }
    }
  }

  tiTxtStream.seekg(0);

  if (errorFound)
    throw xrt_core::error("Bad firmware file format.");

  // Start of flashing BMC firmware
  std::cout << boost::format("%-8s : %s %s %s\n") % "INFO" % "found" % mRecordList.size() % "sections";
  while (retries != 0) {
    retries--;

    ret = erase();
    XBUtilities::ProgressBar sc_flash("Programming SC", static_cast<unsigned int>(mRecordList.size()), XBUtilities::is_esc_enabled(), std::cout);
    int counter = 0;
    for (auto i = mRecordList.begin(); ret == 0 && i != mRecordList.end(); ++i) {
      ret = program(tiTxtStream, *i);
      sc_flash.update(counter);
      counter++;
    }

    if (ret == 0) {
      sc_flash.finish(true, "SC successfully updated");
      break;
    }
    else {
      sc_flash.finish(false, "WARN: Failed to flash firmware, retrying...");
    }
  }
  // End of flashing BMC firmware

  if (ret != 0)
    return ret;

  // Waiting for BMC to come back online.
  // It should not take more than 10 sec, but wait for 1 min to be safe.
  std::cout << boost::format("%-8s : %s\n") % "INFO" % "Loading new firmware on SC";
  for (int i = 0; i < 60; i++) {
    if (bmc_mode() == static_cast<unsigned int>(bmc_state::ready))
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "." << std::flush;
  }
  std::cout << std::endl;

  if (!isBMCReady())
    throw xrt_core::error("Time'd out waiting for SC to come back online");
  return 0;
}

int XMC_Flasher::erase()
{
  int ret = 0;

  mPkt = {0};
  mPkt.hdr.opCode = static_cast<unsigned int>(xmc_packet_op::msp432_erase_fw);

  if ((ret = sendPkt(true)) != 0)
    return ret;

  // Flush the last packet sent to XMC
  return waitTillIdle();
}

int XMC_Flasher::xclGetBoardInfo(std::map<char, std::vector<char>> &info)
{
  int ret = 0;

  if (!hasSC())
    return -EOPNOTSUPP;
  std::vector<char> board_info;
  char *byte;
  size_t size;
  
  try {
    board_info = xrt_core::device_query<xrt_core::query::xmc_board_info>(m_device);
  } 
  catch (...) { }
  if(board_info.empty()) {
    if (!isXMCReady() || !isBMCReady())
      return -EINVAL;
    mPkt = {0};
    mPkt.hdr.opCode = static_cast<unsigned int>(xmc_packet_op::board_info);

    if ((ret = sendPkt(false)) != 0) {
      if (ret == static_cast<unsigned int>(xmc_host_error_msg::brd_info_missing)) {
        std::cout << "Unable to get card info, need to upgrade firmware"
            << std::endl;
      }
    return ret;
    }

    ret = recvPkt();
    if (ret != 0)
      return ret;
    byte = reinterpret_cast<char *>(mPkt.data);
    size = mPkt.hdr.payloadSize;
  }
  else {
    byte = reinterpret_cast<char *>(board_info.data());
    size = board_info.size();
  }
  info.clear();
  for (unsigned int i = 0; i < size;) {
    char key = byte[i++];
    uint8_t len = byte[i++];
    std::vector<char> content(len, 0);
    for (int n = 0; n < len; n++)
      content[n] = byte[i++];
    info[key] = content;
  }
  return 0;
}

int XMC_Flasher::program(std::istream& tiTxtStream, const ELARecord &record)
{
  std::string byteStr;
  int ret = 0;
  unsigned int ndigit = 0;
  int pos;
  char c;
  uint8_t *data;
  const int charPerByte = 2;
  std::locale loc;

#ifdef XMC_DEBUG
  std::cout << std::hex;
  std::cout << "Address=0x" << record.mStartAddress
        << std::dec << ", Length=" << record.mDataCount;
  std::cout << std::endl;
#endif
  tiTxtStream.seekg(record.mDataPos, std::ios_base::beg);

  byteStr.clear();
  mPkt.hdr.opCode =
    record.mDataCount ? static_cast<unsigned int>(xmc_packet_op::msp432_sec_start) : 
                        static_cast<unsigned int>(xmc_packet_op::msp432_image_end);
  mPkt.hdr.reserved = 0;

  const int maxDataSize = sizeof(mPkt.data);
  data = reinterpret_cast<uint8_t *>(&mPkt.data[0]);
  // First uint32_t in payload is always the address
  mPkt.data[0] = record.mStartAddress;
  mPkt.data[1] = record.mDataCount;
  pos = sizeof(uint32_t) * 2;

  while (ndigit < record.mDataCount * charPerByte) {
    if (!tiTxtStream.get(c)) {
      std::cout << "Cannot read data from firmware file" << std::endl;
      return -EIO;
    }
    if (!std::isxdigit(c, loc))
      continue;
    ndigit++;

    byteStr.push_back(c);
    if (byteStr.size() < charPerByte)
      continue;

    uint8_t n = static_cast<uint8_t>(std::stoi(byteStr, 0, 16));
    byteStr.clear();

    data[pos++] = n;
    if (pos < maxDataSize)
      continue;

    // Send out a fully loaded pkt
    mPkt.hdr.payloadSize = pos;
    if ((ret = sendPkt(true)) != 0) //* if prog bar val=true sendPkt false
      return ret;
    // Reset opcode and pos for next data pkt
    mPkt.hdr.opCode = static_cast<unsigned int>(xmc_packet_op::msp432_sec_data);
    pos = 0;
  }

  // Send the last partially loaded pkt
  if (pos) {
    mPkt.hdr.payloadSize = pos;
    if ((ret = sendPkt(true)) != 0)
      return ret;
  }

  // Flush the last packet sent to XMC
  return waitTillIdle();
}

void XMC_Flasher::describePkt(struct xmc_pkt &pkt, bool send)
{
  int lenInUint32 = (sizeof(pkt.hdr) + pkt.hdr.payloadSize +
             sizeof(uint32_t) - 1) /
            sizeof(uint32_t);

  auto format = xrt_core::utils::ios_restore(std::cout);

  if (send)
    std::cout << "Sending XMC packet: ";
  else
    std::cout << "Receiving XMC packet: ";
  std::cout << std::dec << lenInUint32 << " DWORDs..." << std::endl;

  uint32_t *h = reinterpret_cast<uint32_t *>(&pkt.hdr);
  std::cout << "opcode=" << static_cast<unsigned>(pkt.hdr.opCode)
        << " payload_size=" << pkt.hdr.payloadSize
        << " (0x" << std::hex << std::uppercase << std::setfill('0')
        << std::setw(8) << *h << std::dec << ")"
        << std::endl;

#ifdef XMC_DEBUG_VERBOSE
  uint8_t *data = reinterpret_cast<uint8_t *>(&pkt.data[0]);
  std::cout << std::hex;
  int nbytes = 0;
  for (unsigned i = 0; i < pkt.hdr.payloadSize; i++)
  {
    std::cout << std::uppercase << std::setfill('0') << std::setw(2)
          << static_cast<unsigned>(data[i]) << " ";
    nbytes++;
    if ((nbytes % 16) == 0)
      std::cout << std::endl;
  }
  std::cout << std::endl;
#endif
}

int XMC_Flasher::recvPkt()
{
  uint32_t *pkt = reinterpret_cast<uint32_t *>(&mPkt);
  *pkt = readReg(mPktBufOffset);
  unsigned int lenInUint32 =
    (mPkt.hdr.payloadSize + sizeof(uint32_t) - 1) / sizeof(uint32_t);

  if (lenInUint32 <= 0 || lenInUint32 > xmc_max_payload) {
    std::cout << "ERROR: Received bad XMC packet" << std::endl;
    return -EINVAL;
  }

  for (unsigned int i = 0; i < lenInUint32; i++)
    mPkt.data[i] = readReg(mPktBufOffset + (i + 1) * sizeof(uint32_t));

#ifdef XMC_DEBUG
  describePkt(mPkt, false);
#endif
  return waitTillIdle();
}

int XMC_Flasher::sendPkt(bool print_dot)
{
  int lenInUint32 = (sizeof(mPkt.hdr) + mPkt.hdr.payloadSize +
             sizeof(uint32_t) - 1) /
            sizeof(uint32_t);

#ifdef XMC_DEBUG
  describePkt(mPkt, true);
#else
#endif

  uint32_t *pkt = reinterpret_cast<uint32_t *>(&mPkt);

  for (int i = 0; i < lenInUint32; i++) {
    writeReg(mPktBufOffset + i * sizeof(uint32_t), pkt[i]);
  }

  // Flip pkt buffer ownership bit
  writeReg(static_cast<unsigned int>(xmc_reg_offset::control), 
            readReg(static_cast<unsigned int>(xmc_reg_offset::control)) | 
            static_cast<unsigned int>(xmc_mask::pkt_owner));
  return waitTillIdle();
}

int XMC_Flasher::waitTillIdle()
{
  // In total, wait for 500 * 10ms
  int retry = 500;
  unsigned int err = 0;

#if XMC_DEBUG
  std::cout << "INFO: Waiting until idle" << std::endl;
#endif
  while ((retry-- > 0) && (readReg(static_cast<unsigned int>(xmc_reg_offset::control)) 
                            & static_cast<unsigned int>(xmc_mask::pkt_owner))) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  if (retry == 0) {
    std::cout << "ERROR: Time'd out while waiting for XMC packet to be idle"
          << std::endl;
    return -ETIMEDOUT;
  }

  if (readReg(static_cast<unsigned int>(xmc_reg_offset::error)) & static_cast<unsigned int>(xmc_mask::pkt_error))
    err = readReg(static_cast<unsigned int>(xmc_reg_offset::packet_status));

  if (err) {
    std::cout << "ERROR: XMC packet error: " << err << std::endl;
    writeReg(static_cast<unsigned int>(xmc_reg_offset::control), 
                readReg(static_cast<unsigned int>(xmc_reg_offset::control)) | 
                static_cast<unsigned int>(xmc_mask::ctrl_error_clear));
    return -EINVAL;
  }

  return 0;
}

unsigned int XMC_Flasher::readReg(unsigned int RegOffset)
{
  unsigned int value = 0;
  RegOffset = RegOffset;
  m_device->read(mRegBase + RegOffset, &value, 4);
  return value;
}

int XMC_Flasher::writeReg(unsigned int RegOffset, unsigned int value)
{
  value = value;
  m_device->write(mRegBase + RegOffset, &value, 4);
  return 0;
}

bool XMC_Flasher::isXMCReady()
{
  bool xmcReady;
  try {
    xmcReady = (xmc_mode() == static_cast<unsigned int>(xmc_status::ready));
  }
  catch(...) {
    // Xoclv2 driver does not support mmap'ed BAR access from
    // user space any more.
    return true;
  }

  if (!xmcReady) { //start here
    auto format = xrt_core::utils::ios_restore(std::cout);
    if(hasXMC())
      std::cout << "ERROR: XMC is not ready: 0x" << getStatus(xmc_mode(), cmcStatusMap) << std::endl;
  }

  return xmcReady;
}

bool XMC_Flasher::isBMCReady()
{
  bool bmcReady = (bmc_mode() == static_cast<unsigned int>(bmc_state::ready)) || 
                    (bmc_mode() == static_cast<unsigned int>(bmc_state::ready_not_upgradable));

  if (!bmcReady) {
    auto format = xrt_core::utils::ios_restore(std::cout);
    std::cout << "ERROR: SC is not ready: 0x" << getStatus(bmc_mode(), scStatusMap) << std::endl;
  }

  return bmcReady;
}

bool XMC_Flasher::hasXMC()
{
  bool xmc_presence = false;
  try {
    xrt_core::device_query<xrt_core::query::xmc_sc_version>(m_device);
    xmc_presence = true;
  }
  catch (...) { }
  return xmc_presence;
}

bool XMC_Flasher::hasSC()
{
  if (!hasXMC())
    return false;
  
  bool sc_presence = false;
  try {
    sc_presence = xrt_core::device_query<xrt_core::query::xmc_sc_presence>(m_device);
  }
  catch (const std::exception& ex) {
    std::cerr << "ERROR:" << ex.what() << std::endl;
   }
  return sc_presence;
}

bool XMC_Flasher::fixedSC()
{
  if (!hasXMC())
    return false;
  bool is_sc_fixed = false;
  try {
    is_sc_fixed = xrt_core::device_query<xrt_core::query::is_sc_fixed>(m_device);
  }
  catch (const std::exception& ex) {
    std::cerr << "ERROR:" << ex.what() << std::endl;
   }
  return is_sc_fixed;
}

int XMC_Flasher::xclUpgradeFirmwareDrv(std::istream& tiTxtStream)
{
  int ret = 0;

  // Parse Ti-TXT data and write each contiguous chunk to XMC.
  std::vector<unsigned char> buf;
  unsigned int curAddr = UINT_MAX;
  while (ret == 0) {
    tiTxtStreamToBin(tiTxtStream, curAddr, buf);
    if (buf.empty())
      break;
#ifdef  XMC_DEBUG
    std::cout << "Extracted " << buf.size() << "B firmware image @0x"
        << std::hex << curAddr << std::dec << std::endl;
#endif
    ret = writeImage(mXmcDev, curAddr, buf);
  }
  std::cout << std::endl;
  if (ret) {
    std::cout << "ERROR: Failed to update SC firmware, err=" << ret
        << std::endl;
    std::cout << "ERROR: Please refer to dmesg for more details"
        << std::endl;
  }

  return ret;
}

} //xrt_tools
