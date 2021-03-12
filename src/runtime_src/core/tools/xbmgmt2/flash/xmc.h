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
#ifndef _XMC_H_
#define _XMC_H_

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "core/common/device.h"

// System - Include Files
#include <list>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>

namespace xrt_tools {

// ------ S T A T I C   C O N S T A N T S   A N D   E N U M S ------------------

// Register offset in mgmt pf BAR 0
static const int xmc_reg_base = 0x120000;

static const int xmc_magic_num = 0x74736574;
static const int xmc_base_version = 2018201;

// Register offset in register map of XMC
enum class xmc_reg_offset
{
  magic         = 0x0,
  version       = 0x4,
  status        = 0x8,
  error         = 0xc,
  feature       = 0x10,
  control       = 0x18,
  packet_offset = 0x300,
  packet_status = 0x304
};

enum class xmc_mask
{
  ctrl_error_clear = 1 << 1,
  pkt_support      = 1 << 3,
  pkt_owner        = 1 << 5,
  pkt_error        = 1 << 26
};

enum class xmc_status
{
  ready    = 0x1 << 0,
  stoppped = 0x1 << 1,
  paused   = 0x1 << 2
};

enum class xmc_host_error_msg 
{
  success          = 0x00,
  bad_opcode       = 0x01,
  unknown          = 0x02,
  msp432_mode      = 0x03,
  msp432_fw_length = 0x04,
  brd_info_missing = 0x05
};

enum class bmc_state
{
  unknown = 0,
  ready,
  bsl_unsync,
  bsl_sync,
  bsl_sync_not_upgradable,
  ready_not_upgradable
};

enum class xmc_packet_op
{
  unknown = 0,
  msp432_sec_start,
  msp432_sec_data,
  msp432_image_end,
  board_info,
  msp432_erase_fw
};

static const size_t xmc_pkt_size = (1024 / sizeof(uint32_t)) * 4;
struct xmc_pkt
{
  // Make sure hdr is uint32_t aligned
  struct xmc_pkt_header
  {
    unsigned payloadSize : 12;
    unsigned reserved : 12;
    unsigned opCode : 8;
  } hdr;
  uint32_t data[xmc_pkt_size - sizeof(hdr) / sizeof(uint32_t)];
};
static const size_t xmc_max_payload = xmc_pkt_size - sizeof(xmc_pkt::xmc_pkt_header) / sizeof(uint32_t);

// ------------- C L A S S :  X M C _ F l a s h e r -----------------

class XMC_Flasher
{
  struct ELARecord
  {
    unsigned mStartAddress;
    unsigned mEndAddress;
    unsigned mDataCount;
    std::streampos mDataPos;
    ELARecord() : mStartAddress(0), mEndAddress(0), mDataCount(0), mDataPos(0) {}
  };

  using ELARecordList = std::list<ELARecord>;
  ELARecordList mRecordList;

public:
  XMC_Flasher(unsigned int device_index);
  ~XMC_Flasher();
  int xclUpgradeFirmware(std::istream &tiTxtStream);
  int xclGetBoardInfo(std::map<char, std::vector<char>> &info);
  const std::string probingErrMsg() { return mProbingErrMsg.str(); }
  bool hasXMC();
  bool fixedSC();

private:
  std::shared_ptr<xrt_core::device> m_device;
  unsigned mPktBufOffset;
  uint64_t mRegBase = 0;
  struct xmc_pkt mPkt;
  std::stringstream mProbingErrMsg;

  int program(std::istream &tiTxtStream, const ELARecord &record);
  int erase();
  int sendPkt(bool print_dot);
  void describePkt(struct xmc_pkt &pkt, bool send);
  int recvPkt();
  int waitTillIdle();
  unsigned readReg(unsigned RegOffset);
  int writeReg(unsigned RegOffset, unsigned value);
  bool isXMCReady();
  bool isBMCReady();
  bool hasSC();
  int xmc_mode();
  int bmc_mode();

  // Upgrade SC firmware via driver.
  std::FILE *mXmcDev = nullptr;
  int xclUpgradeFirmwareDrv(std::istream& tiTxtStream);
};

} //xrt_tools

#endif
