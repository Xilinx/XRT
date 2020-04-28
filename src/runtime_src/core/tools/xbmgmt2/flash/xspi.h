/**
 * Copyright (C) 2016-2019 Xilinx, Inc
 * Author(s) : Sonal Santan
 *           : Hem Neema
 *           : Ryan Radjabi
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
#ifndef _XSPI_H_
#define _XSPI_H_

#include <list>
#include <iostream>
#include "core/common/system.h"
#include "core/common/device.h"

class XSPI_Flasher
{
  struct ELARecord
  {
    unsigned int mStartAddress;
    unsigned int mEndAddress;
    unsigned int mDataCount;
    std::streampos mDataPos;

    ELARecord() : mStartAddress(0), mEndAddress(0), mDataCount(0), mDataPos(0) {}
  };

  typedef std::list<ELARecord> ELARecordList;
  ELARecordList recordList;

 public:
  XSPI_Flasher(std::shared_ptr<xrt_core::device> dev);
  ~XSPI_Flasher();
  int xclUpgradeFirmware1(std::istream& mcsStream1);
  int xclUpgradeFirmware2(std::istream& mcsStream1, std::istream& mcsStream2);
  int revertToMFG(void);

 private:
  std::shared_ptr<xrt_core::device> mDev;
  std::FILE *mFlashDev = nullptr;

  int parseMCS(std::istream& mcsStream);

  unsigned long long flash_base;
  int xclTestXSpi(int device_index);
  unsigned int readReg(unsigned int offset);
  int writeReg(unsigned int regOffset, unsigned int value);
  bool waitTxEmpty();
  bool isFlashReady();
  bool sectorErase(unsigned int Addr, uint8_t erase_cmd);
  bool writeBitstreamGuard(unsigned int Addr);
  bool clearBitstreamGuard(unsigned int Addr);
  bool bulkErase();
  bool writeEnable();
  bool getFlashId();
  bool finalTransfer(uint8_t *sendBufPtr, uint8_t *recvBufPtr, int byteCount);
  bool writePage(unsigned int addr, uint8_t writeCmd = 0xff);
  bool readPage(unsigned int addr, uint8_t readCmd = 0xff);
  bool prepareXSpi(uint8_t slave_sel);
  int programRecord(std::istream& mcsStream, const ELARecord& record);
  int programXSpi(std::istream& mcsStream, uint32_t bitstream_shift_addr);
  bool readRegister(uint8_t commandCode, unsigned int bytes);
  bool writeRegister(uint8_t commandCode, unsigned int value, unsigned int bytes);
  bool setSector(unsigned int address);
  unsigned int getSector(unsigned int address);

  // Upgrade firmware via driver.
  int upgradeFirmware1Drv(std::istream& mcsStream1);
  int upgradeFirmware2Drv(std::istream& mcsStream1, std::istream& mcsStream2);
};

#endif
