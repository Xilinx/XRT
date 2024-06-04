/******************************************************************************
* Copyright (c) 2018-2022 Xilinx, Inc.  All rights reserved.
* Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file load_pdi.c
* @addtogroup load PDI implementation
* @{
* @cond load_pdi
* This is the file which contains PDI loading implementation
*
* @note
* @endcond
*
******************************************************************************/

/***************************** Include Files *********************************/
#include <stdio.h>
#include <string.h>
#include "cdo_cmd.h"
#include "load_pdi.h"
#ifndef _ENABLE_IPU_LX6_
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
uint8_t XPdi_Cmd_Match(uint32_t CmdId)
{
  uint8_t ret = 0;
  switch (CmdId) {
    case XCDO_CMD_MASK_WRITE:
    case XCDO_CMD_WRITE:
    case XCDO_CMD_DMAWRITE:
    case XCDO_CMD_MASKWRITE64:
    case XCDO_CMD_WRITE64:
      ret = 1;
      break;
    default:
      ret = 0;
  }
  return ret;
}

char* XPdi_Parse_Cmd(XCdoCmd* Cmd, uint32_t* prevId, 
                     uint32_t CmdId, char* cur_pdi_buf,
                     uint32_t** cmd_num, uint32_t const * obuf)
{
    
  if (!XPdi_Cmd_Match(CmdId)) return cur_pdi_buf;

  if (*prevId !=CmdId) {
    // if (prevId != -1 && *cmd_num != NULL) {
    if ((uint64_t)prevId != UINT64_MAX && *cmd_num != NULL) {
      // printf("get cmd id %x cmd_num is %d \n", *prevId, *(*cmd_num));
      XCdo_Print("get cmd id %x cmd_num is %d \n", *prevId, *(*cmd_num));
    }
    *prevId = CmdId;
    //printf("get cmd id %x\n", CmdId);
    *((uint32_t*)cur_pdi_buf) = CmdId;
    cur_pdi_buf += sizeof(uint32_t);
    *cmd_num = (uint32_t*)cur_pdi_buf;
    *(*cmd_num) = 0;
    cur_pdi_buf += sizeof(uint32_t);
  }
  (*(*cmd_num))++;
  switch (CmdId) {
    case XCDO_CMD_WRITE64:
    case XCDO_CMD_MASKWRITE64:
    case XCDO_CMD_MASK_WRITE:
    case XCDO_CMD_WRITE:
      *((uint32_t*)cur_pdi_buf) = Cmd->Payload[0U];
      cur_pdi_buf += sizeof(uint32_t);
      *((uint32_t*)cur_pdi_buf) = Cmd->Payload[1U];
      cur_pdi_buf += sizeof(uint32_t);
      if (CmdId == XCDO_CMD_WRITE64 || CmdId == XCDO_CMD_MASK_WRITE || CmdId == XCDO_CMD_MASKWRITE64){
        *((uint32_t*)cur_pdi_buf) = Cmd->Payload[2U];
        cur_pdi_buf += sizeof(uint32_t);
        if (CmdId == XCDO_CMD_MASKWRITE64) {
          *((uint32_t*)cur_pdi_buf) = Cmd->Payload[3U];
          cur_pdi_buf += sizeof(uint32_t);
        }
      }
      break;
    case XCDO_CMD_DMAWRITE:
      //store the low dst addr
      *((uint32_t*)cur_pdi_buf) = Cmd->Payload[0U];
      cur_pdi_buf += sizeof(uint32_t);
      //store the high dst addr
      *((uint32_t*)cur_pdi_buf) = Cmd->Payload[1U];
      cur_pdi_buf += sizeof(uint32_t);
      //store the original pdi source address used to copy data to new pdi
      // *((uint32_t*)cur_pdi_buf) = (uint32_t)(&Cmd->Payload[2U]) - (uint32_t)obuf;
      *((uint32_t*)cur_pdi_buf) = (uint32_t)((uintptr_t)(&Cmd->Payload[2U]) - (uintptr_t)obuf);
      cur_pdi_buf += sizeof(uint32_t);
      // minus the dst addr payload, and the unit is 4B
      *((uint32_t*)cur_pdi_buf) = (Cmd->PayloadLen - 2);
      cur_pdi_buf += sizeof(uint32_t);
      break;
    default:
      break;
  }


  return cur_pdi_buf;
}

uint32_t XPdi_Cmd_Parse(char* pdi_buf, uint32_t BufLen, const uint32_t *Buf)
{
  uint32_t const * Orignal_Buf = Buf;
  char* cur_pdi_buf = pdi_buf;
  static uint32_t prevId = -1;
  static uint32_t* cmd_num = NULL;
  {
    // reset static variables for each pdi file to be transformed
    prevId = -1;
    cmd_num = NULL; 
  }
   
  while (BufLen) {
    XCdoCmd Cmd;
    uint32_t CmdId = Buf[0] & XCDO_CMD_API_ID_MASK;
    XCdo_CmdSize((uint32_t*)Buf, &Cmd);

    if (Cmd.Size > BufLen) {
      XCdo_PError("Invalid CDO command length %u,%u.\n\r", Cmd.Size, BufLen);
      return 0;
    }

    cur_pdi_buf = XPdi_Parse_Cmd(&Cmd, &prevId, CmdId, cur_pdi_buf, &cmd_num, Orignal_Buf);

    if (CmdId != XCDO_CMD_NOP && !XPdi_Cmd_Match(CmdId)) {
      XCdo_PError("Invalid cdoset clipboard=unnamed command %u\n", CmdId);
      break;
    }
    Buf += Cmd.Size;
    BufLen -= Cmd.Size;
  }
  // printf("buf len is %ld\n", cur_pdi_buf - pdi_buf);
  XCdo_Print("buf len is %ld\n", cur_pdi_buf - pdi_buf);
  return cur_pdi_buf - pdi_buf;
}

uint8_t is_bss(uint32_t dst_addr)
{
#define XAIE_ROW_SHIFT 20
#define XAIE_COL_SHIFT 25
  uint32_t addr = dst_addr &(((1<<XAIE_ROW_SHIFT))-1);
  // printf("addr = %x\n",addr);
  XCdo_Print("addr = %x\n",addr);
  const uint32_t dm_start_addr = 0;
  const uint32_t dm_size = 1024*64;
  return addr > dm_start_addr && addr < dm_start_addr + dm_size;
}

uint8_t allZero(void* mem, size_t len)
{
  do { if (*((char*)mem++) != 0 ) return 0; } while(--len);
  return 1;
}
//prepare the data zone
uint32_t XPdi_Buf_Parse(char* pdi_buf, uint32_t CBufLen, uint32_t BufLen, const char *Buf)
{
  uint32_t dma_zero_data_size = 0;
  char const * obuf =  pdi_buf;
  char* data_buf = pdi_buf + CBufLen, *odata_buf = data_buf;
  while(pdi_buf - obuf < CBufLen) {
    uint32_t CmdId = *((uint32_t*)pdi_buf);
    pdi_buf += sizeof(uint32_t);
    uint32_t num = *((uint32_t*)pdi_buf);
    pdi_buf += sizeof(uint32_t);
    uint32_t cmd_len = 0;
    switch (CmdId) {
      case XCDO_CMD_WRITE64:
      case XCDO_CMD_MASKWRITE64:
      case XCDO_CMD_MASK_WRITE:
      case XCDO_CMD_WRITE:
        cmd_len += sizeof(uint32_t) * 2;
        if (CmdId == XCDO_CMD_WRITE64 || CmdId == XCDO_CMD_MASK_WRITE ||CmdId == XCDO_CMD_MASKWRITE64){
          cmd_len += sizeof(uint32_t);
          if (CmdId == XCDO_CMD_MASKWRITE64) {
            cmd_len += sizeof(uint32_t);
          }
        }
        pdi_buf += (size_t)(num * cmd_len);
        break;
      case XCDO_CMD_DMAWRITE:
        cmd_len += sizeof(uint32_t) * 4;
        for (uint32_t i = 0; i < num; i++) {
          uint32_t src_offset = ((uint32_t*)pdi_buf)[2];
          const uint32_t mem_len = (0xFFFF & ((uint32_t*)pdi_buf)[3]) * 4;
          memcpy(data_buf, Buf + src_offset, mem_len);
          // TODO
          // If the data all zero, then seems like be the BSS section or
          // DM memory zeroize operation, need to check whether this is
          // DM related, if yes then we can replace such DMA transfer into
          // do TILE dm clear at LOAD PDI beginning, for 1x3, can reduce
          // 53K of 118k data move.
          // uint8_t bss_zero = 0;
          if (is_bss( ((uint32_t*)pdi_buf)[1]) && allZero(data_buf, mem_len)) {
            dma_zero_data_size += mem_len;
            // printf("mem_len = %d dst high %x dst low %x\n", mem_len, ((uint32_t*)pdi_buf)[0], 
            XCdo_Print("mem_len = %d dst high %x dst low %x\n", mem_len, ((uint32_t*)pdi_buf)[0], 
                                                           ((uint32_t*)pdi_buf)[1]);
            ((uint32_t*)pdi_buf)[3] = ((uint32_t*)pdi_buf)[3] | (1 << 16);
          }
          //point to the offset location of data.
          ((uint32_t*)pdi_buf)[2] = data_buf - odata_buf;
          data_buf += mem_len;
          //move cmd pointer
          pdi_buf += cmd_len;
          // printf("dma len %d \n", mem_len);
          XCdo_Print("dma len %d \n", mem_len);
        }
        break;
      default:
        break;
    } 
    // printf("cmd_id %d num %d , new buf %ldB, origin buf %uB\n", CmdId, num, data_buf - obuf, BufLen * 4);
    XCdo_Print("cmd_id %d num %d , new buf %ldB, origin buf %uB\n", CmdId, num, data_buf - obuf, BufLen * 4);
  }
  // printf("the all zeror dma data length is  %d\n", dma_zero_data_size);
  XCdo_Print("the all zeror dma data length is  %d\n", dma_zero_data_size);

  return data_buf - obuf;

}

void XPdi_Export(const XPdiLoad* PdiLoad, const char* pdi_file_out)
{
  const char* pdi_file = pdi_file_out;
  char* Buf = (char *)PdiLoad->PdiPtr;
  uint32_t len = PdiLoad->PdiLen;
  FILE * fp = fopen (pdi_file,"w");
  //int fd = open(pdi_file, O_WRONLY | O_CREAT | O_TRUNC);
  if (fp == NULL)
  {
    printf("%s create failed Error Number % d\n", pdi_file, errno);
    return;
  }
  size_t elements_written = fwrite(Buf,sizeof(char),len,fp);
  if (fclose(fp) != 0) {
    printf("Failed to close file %s\n", pdi_file);
    return;
  }
  if (elements_written < len) {
    printf("Failed to write the buffer to %s\n", pdi_file);
    return;
  }
  // printf("the new transform file %s created!\n ", pdi_file);
  XCdo_Print("the new transform file %s created!\n ", pdi_file);
}

enum cdoFormat {
  NUMOFWORDS,
  IDWRD,
  VERSION,
  LENGTH,
  CHECKSUM
};

static char* cdoHeader[] = {
  [NUMOFWORDS] = "Number of words",
  [IDWRD] = "Identification Word",
  [VERSION] = "Version",
  [LENGTH] = "Length",
  [CHECKSUM] = "Checksum"
};

void XPdi_CdoHeader_String(uint32_t* header) {
  
  for(uint32_t itr = 0; itr < XCDO_CDO_HDR_LEN; itr++) {
    // printf("%s = %u\n", cdoHeader[itr], header[itr]);
    XCdo_Print("%s = %u\n", cdoHeader[itr], header[itr]);
  }
}

/*******************************************************************************
********************************************************************************/
void XPdi_Compress_Transform(XPdiLoad* PdiLoad, const char* pdi_file_out)
{
  uint32_t BufLen = 0/*, Ret*/;
  /*const*/ uint32_t *Buf = NULL;
  //Check whether the PDI already get transformed or not.
  if (XPdi_Header_Transform_Type(PdiLoad, NULL) != NOTRANFORM) {
    printf("PDI is in tranform format can not compress again, do normal pdi load.");
    XPdi_Load(PdiLoad);
    return;
  }
  // printf("start to transform !");
  XCdo_Print("start to transform !");
  XCdoLoad CdoLoad;
  XPdi_GetFirstPrtn(PdiLoad, &CdoLoad);
  ParseBufFromCDO(&Buf, &BufLen, &CdoLoad);
  //Prepare the new PDI memory
  char* pdi_buf = (char *)malloc((size_t)BufLen * 2 * 4);
  uint32_t HdrLen = PDI_IMAGE_HDR_TABLE_OFFSET + sizeof(XilPdi_ImgHdrTbl) +
        sizeof(XilPdi_ImgHdr) + sizeof(XilPdi_PrtnHdr);
  //copy the PDI header
  XPdiLoad newPdiLoad;
  newPdiLoad.PdiPtr = pdi_buf;
  newPdiLoad.BasePtr = PdiLoad->BasePtr;
  //set this later,because we only know the new length after tranform
  newPdiLoad.PdiLen = 0;
  memcpy(pdi_buf, ((char *)PdiLoad->PdiPtr), HdrLen);
  //memset(((char *)pdi_buf), 0, HdrLen);
  char* cdo_buf = pdi_buf + HdrLen;
  memcpy(cdo_buf, ((char *)PdiLoad->PdiPtr + HdrLen),
             XCDO_CDO_HDR_LEN * sizeof(uint32_t));
  XPdi_CdoHeader_String((uint32_t*)cdo_buf);
  //Parse and generate the command zone
  uint32_t Cmd_len = XPdi_Cmd_Parse(cdo_buf +
             (XCDO_CDO_HDR_LEN * sizeof(uint32_t)), BufLen, Buf);
  //Change the pdi header to mark that this is a tranform/compress pdi
  XPdi_Header_Set_Transfrom_Type(&newPdiLoad, CMDDATASPERATE, Cmd_len);
  //Parse and generate the data zone
  uint32_t TotalCdoLen = XPdi_Buf_Parse(cdo_buf  +
             (XCDO_CDO_HDR_LEN * sizeof(uint32_t)), Cmd_len, BufLen, (const char*)Buf);
  //Update the pdi length.
  newPdiLoad.PdiLen = TotalCdoLen + HdrLen;
  // printf("new cdo len is %d\n", TotalCdoLen);
  XCdo_Print("new cdo len is %d\n", TotalCdoLen);
  //test load new pdi
  XPdi_Load(&newPdiLoad);
  //Export the tranform pdi into a file (generate the new pdi file)
  XPdi_Export(&newPdiLoad, pdi_file_out);
  //Release the PDI memory
  free(pdi_buf);
  return;
}
#endif
