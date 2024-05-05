// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc.

#ifndef __PATCH_DDR_ADDRESS_H__
#define __PATCH_DDR_ADDRESS_H__

#include <cstdio>
#include <algorithm>
#include <iomanip>
#include <vector>
#include <cassert>
#include <array>
#include <inttypes.h>

// ***********************************************************************************************
// Here is an example of parsing DDR Address And RegID,
// In this case, the BaseAddress bit width is equals to 48 bits and the Reg_id
// bit width is equal to 4. then the BaseAddress mask value is 0xFFFFFFFFFFF and
// the Reg_ID is 0xF
//  47-44  43-40  39-36  35-32  31-28  27-24  23-20  19-16  15-12  11-8  7-4 3-0
// |Reg_ID|<------------------------ Base_Address
// ------------------------------>|
#define BASE_ADDRESS_LENGTH 48
#define REG_ID_LENGTH 4

uint32_t
getRegID(uint64_t BDData, uint64_t mask)
{
  // in this case reg ID mask = 0xF
  return (uint32_t)(((BDData >> (BASE_ADDRESS_LENGTH - REG_ID_LENGTH)) & mask));
}

uint64_t
getBaseAddress(uint64_t BDData, uint64_t mask)
{
  // in this case base address mask = 0xFFFFFFFFFFF;
  return BDData & mask;
}
// ******************************************************************************************** //

#define IFM_TYPE 0x0
#define PARAM_TYPE 0x1
#define OFM_TYPE 0x2
#define INTER_TYPE 0x3

struct DDRDataStartAddr
{
  uint64_t ifmStartAddr;
  uint64_t paramStartAddr;
  uint64_t ofmStartAddr;
  uint64_t interStartAddr;
  DDRDataStartAddr()
    : ifmStartAddr(0)
    ,  paramStartAddr(0)
    ,  ofmStartAddr(0)
    ,  interStartAddr(0)
  {}
};

#define DMA_BD_NUM 16
std::array<uint32_t, DMA_BD_NUM> DMABDx2RegAddr;

//  patch DDR address，this funtion is from interpreter in LX6.
int32_t
patchDDRAddrFromLogicToPhysic(uint32_t &BDData1, uint32_t &BDData2,
                              struct DDRDataStartAddr DDRAddr)
{
  uint32_t addrLow = BDData1;
  uint32_t addrHigh = (BDData2 & 0x00000FFF);
  uint32_t regID = ((BDData2 >> 12) & 0xf);
  uint64_t tensorAddr = ((((uint64_t)addrHigh) << 32) | addrLow);

  switch (regID) {
    case IFM_TYPE :
      tensorAddr += DDRAddr.ifmStartAddr;
      //printf("ifmStartAddr = 0x%" PRIx64 "\n", DDRAddr.ifmStartAddr);
      break;
    case PARAM_TYPE :
      tensorAddr += DDRAddr.paramStartAddr;
      //printf("paramStartAddr = 0x%" PRIx64 "\n", DDRAddr.paramStartAddr);
      break;
    case OFM_TYPE :
      tensorAddr += DDRAddr.ofmStartAddr;
      //printf("ofmStartAddr = 0x%" PRIx64 "\n", DDRAddr.ofmStartAddr);
      break;
    case INTER_TYPE :
      tensorAddr += DDRAddr.interStartAddr;
      //printf("interStartAddr = 0x%" PRIx64 "\n", DDRAddr.interStartAddr);
      break;
    default:
      break;
  }

  BDData1 = ((tensorAddr)&0xFFFFFFFFC); // unused 2-LSB
  BDData2 = ((BDData2 & 0xFFFF0000) | (tensorAddr >> 32));
  return 0;
}

uint32_t
patchddrAddress(uint32_t *BDData, uint32_t len, uint32_t addr,
                struct DDRDataStartAddr DDRAddr)
{
  // check if shim tile BD register contains DDR address.
  // This is to support variable number of DMA_BDx register configurations, but this function needs to be checked.
  // Now we write register from DMA_BDx_0 to DMA_BDx_7 every time, for more efficiency, we may only write part of eight DMA_BDx later.
  // One thing to note is that we cannot only write the Base_Address_High of DMA_BDx_2, which also means that the address of DMA_BDx_2
  // cannot be in the Local Byte Address of control packet(CP). So we start traversing from addr plus 4.
  // Taking DMA_BD0 as an examle, now we fully configure from 0x1D000 to 0x1D01C, later we may only config five registers,
  // say from 0x1D00C to 0x1D01C. the position of Base_Address_High in BD data is variable, and may even not exist.
  // so We need to check if the shim tile DMA_BDx register contains the DDR address.
  for (int i = 1; i < len + 1; i++) {
    addr += 4;
    if (DMABDx2RegAddr.end() != std::find(DMABDx2RegAddr.begin(), DMABDx2RegAddr.end(), addr)) {
      // patch DDR Addrese from offset to phisical address
      patchDDRAddrFromLogicToPhysic(BDData[i - 1], BDData[i], DDRAddr);
    }
  }
  return 0;
}

uint32_t
readMcCodeFile(char *file_name, uint32_t *data)
{
  assert((file_name != NULL) && "invalid read file name!");

  FILE *fp = nullptr;
#if __linux__
  fp = fopen(file_name, "rb");
#elif _WIN32  
  fopen_s(&fp, file_name, "rb");
#else

#endif
  fseek(fp, 0, SEEK_END);
  uint32_t size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  fread(data, 1, size, fp);
  fclose(fp);

  return size;
}

void
dumpMcCodeFile(char *file_name, uint32_t *data, uint32_t size)
{
  assert((file_name != NULL) && "invalid dump file name!");

  FILE *fp = nullptr;
#if __linux__
  fp = fopen(file_name, "wb");
#elif _WIN32  
  fopen_s(&fp, file_name, "wb");
#else

#endif
  fwrite(data, 1, size, fp);
  fclose(fp);
  return;
}

int
patchMcCodeDDR(uint64_t ddr_base_ifm, uint64_t ddr_base_param, uint64_t ddr_base_ofm,
               uint64_t ddr_base_inter, uint32_t *mc_code_ddr, uint32_t mc_code_ddr_size_bytes,
               int pad_control_packet)
{
  struct DDRDataStartAddr DDRAddr;
  DDRAddr.ifmStartAddr = ddr_base_ifm;
  DDRAddr.paramStartAddr = ddr_base_param;
  DDRAddr.ofmStartAddr = ddr_base_ofm;
  DDRAddr.interStartAddr = ddr_base_inter;

  // list all shim tile BD registers DDR address need to be processed
  for (int i = 0; i < 16; i++) {
    DMABDx2RegAddr[i] = 0x0001D008 + 0x20 * i;
  }

  uint32_t dataSize = 0;
  uint32_t localByteAddress = 0;
  uint32_t pc = 0;
  // Traverse all mc code ddr instructions
  while (pc < mc_code_ddr_size_bytes / 4) {
    // read packet header and control packet, parse the data size and BD register addr
    pc += 2;
    dataSize = ((mc_code_ddr[pc - 1] >> 20) & 0x3);
    localByteAddress = (mc_code_ddr[pc - 1] & 0xfffff);

    // patch shim tile register DMA_BDx DDR address
    patchddrAddress(&mc_code_ddr[pc], dataSize, localByteAddress, DDRAddr);
    pc += (dataSize + 1);

    // control packets aligned to 256 bits
    if (pad_control_packet) {
      pc += (8 - (pc % 8)) % 8;
    }
  }
  return 0;
}
#endif
