/**
 * Copyright (C) 2020 Xilinx, Inc
 * Author(s): Larry Liu
 * ZNYQ XRT Library layered on top of ZYNQ zocl kernel driver
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

#include "aie.h"
#include "core/common/error.h"
#ifndef __AIESIM__
#include "core/common/message.h"
#endif

#include <iostream>
#include <cerrno>

namespace zynqaie {

static inline uint64_t
get_bd_high_addr(uint64_t addr)
{
  constexpr uint64_t hi_mask = 0xFFFF00000000L;
  return ((addr & hi_mask) >> 32);
}

static inline uint64_t
get_bd_low_addr(uint64_t addr)
{
  constexpr uint32_t low_mask = 0xFFFFFFFFL;
  return (addr & low_mask);
}

Aie::Aie(const std::shared_ptr<xrt_core::device>& device)
{
    /* TODO where are these number from */
    numRows = 8;
    numCols = 50;
    aieAddrArrayOff = 0x800;

    XAIEGBL_HWCFG_SET_CONFIG((&aieConfig), numRows, numCols, aieAddrArrayOff);
    XAieGbl_HwInit(&aieConfig);
    aieConfigPtr = XAieGbl_LookupConfig(XPAR_AIE_DEVICE_ID);

    int tileArraySize = numCols * (numRows + 1);
    tileArray.resize(tileArraySize);

    /*
     * Initialize AIE tile array.
     *
     * TODO is void good here?
     */
    (void) XAieGbl_CfgInitialize(&aieInst, tileArray.data(), aieConfigPtr);

    /* Initialize graph GMIO metadata */
    for (auto& gmio : xrt_core::edge::aie::get_gmios(device.get()))
        gmios.emplace_back(std::move(gmio));

    /*
     * Initialize AIE shim DMA on column base if there is one for
     * this column.
     */
    shim_dma.resize(numCols);
    for (auto& gmio : gmios) {
        auto dma = &shim_dma.at(gmio.shim_col);
        auto pos = getTilePos(gmio.shim_col, 0);
        if (!dma->configured) {
            XAieDma_ShimSoftInitialize(&(tileArray.at(pos)), &(dma->handle));
            XAieDma_ShimBdClearAll(&(dma->handle));
            dma->configured = true;
        }

        auto chan = gmio.channel_number;
        XAieDma_ShimChControl((&(dma->handle)), chan, XAIE_DISABLE, XAIE_DISABLE, XAIE_ENABLE);
        for (int i = 0; i < XAIEGBL_NOC_DMASTA_STARTQ_MAX; ++i) {
            /*
             * 16 BDs are allocated to 4 channels.
             * Channel0: BD0~BD3
             * Channel1: BD4~BD7
             * Channel2: BD8~BD11
             * Channel3: BD12~BD15
             */
            int bd_num = chan * XAIEGBL_NOC_DMASTA_STARTQ_MAX + i;
            BD bd;
            bd.bd_num = bd_num;
            dma->dma_chan[chan].idle_bds.push(bd);

            XAieDma_ShimBdSetAxi(&(dma->handle), bd_num, 0, gmio.burst_len, 0, 0, 0);
        }
    }

#ifndef __AIESIM__
    /* Disable AIE interrupts */
    u32 reg = XAieGbl_NPIRead32(XAIE_NPI_ISR);
    XAieGbl_NPIWrite32(XAIE_NPI_ISR, reg);

    if (XAieTile_EventsHandlingInitialize(&aieInst) != XAIE_SUCCESS)
        throw xrt_core::error("Failed to initialize AIE Events Handling.");

    /* Register all AIE error events */
    XAieTile_ErrorRegisterNotification(&aieInst, XAIEGBL_MODULE_ALL, XAIETILE_ERROR_ALL, error_cb, NULL);

    /* Enable AIE interrupts */
    XAieTile_EventsEnableInterrupt(&aieInst);
#endif
}

Aie::~Aie()
{
}

int Aie::getTilePos(int col, int row)
{
    return col * (numRows + 1) + row;
}

XAieGbl* Aie::getAieInst()
{
    return &aieInst;
}

XAieGbl_ErrorHandleStatus
Aie::error_cb(struct XAieGbl *aie_inst, XAie_LocType loc, u8 module, u8 error, void *arg)
{
#ifndef __AIESIM__
    auto severity = xrt_core::message::severity_level::XRT_INFO;

    switch (module) {
    case XAIEGBL_MODULE_CORE:
        switch (error) {
            case XAIETILE_EVENT_CORE_TLAST_IN_WSS_WORDS_0_2:
            case XAIETILE_EVENT_CORE_PM_REG_ACCESS_FAILURE:
            case XAIETILE_EVENT_CORE_STREAM_PKT_PARITY_ERROR:
            case XAIETILE_EVENT_CORE_CONTROL_PKT_ERROR:
            case XAIETILE_EVENT_CORE_INSTRUCTION_DECOMPRESSION_ERROR:
            case XAIETILE_EVENT_CORE_DM_ADDRESS_OUT_OF_RANGE:
            case XAIETILE_EVENT_CORE_AXI_MM_SLAVE_ERROR:
            case XAIETILE_EVENT_CORE_PM_ECC_ERROR_SCRUB_2BIT:
            case XAIETILE_EVENT_CORE_PM_ADDRESS_OUT_OF_RANGE:
            case XAIETILE_EVENT_CORE_DM_ACCESS_TO_UNAVAILABLE:
            case XAIETILE_EVENT_CORE_LOCK_ACCESS_TO_UNAVAILABLE:
                severity = xrt_core::message::severity_level::XRT_EMERGENCY;
                break;

            case XAIETILE_EVENT_CORE_FP_OVERFLOW:
            case XAIETILE_EVENT_CORE_FP_UNDERFLOW:
            case XAIETILE_EVENT_CORE_FP_INVALID:
            case XAIETILE_EVENT_CORE_FP_DIV_BY_ZERO:
            case XAIETILE_EVENT_CORE_INSTR_WARNING:
            case XAIETILE_EVENT_CORE_INSTR_ERROR:
                severity = xrt_core::message::severity_level::XRT_ERROR;
                break;

            case XAIETILE_EVENT_CORE_SRS_SATURATE:
            case XAIETILE_EVENT_CORE_UPS_SATURATE:
                severity = xrt_core::message::severity_level::XRT_NOTICE;
                break;

            default:
                break;
        }
        break;

    case XAIEGBL_MODULE_MEM:
        switch (error) {
            case XAIETILE_EVENT_MEM_DM_ECC_ERROR_2BIT:
            case XAIETILE_EVENT_MEM_DMA_S2MM_0_ERROR:
            case XAIETILE_EVENT_MEM_DMA_S2MM_1_ERROR:
            case XAIETILE_EVENT_MEM_DMA_MM2S_0_ERROR:
            case XAIETILE_EVENT_MEM_DMA_MM2S_1_ERROR:
                severity = xrt_core::message::severity_level::XRT_EMERGENCY;
                break;

            case XAIETILE_EVENT_MEM_DM_PARITY_ERROR_BANK_2:
            case XAIETILE_EVENT_MEM_DM_PARITY_ERROR_BANK_3:
            case XAIETILE_EVENT_MEM_DM_PARITY_ERROR_BANK_4:
            case XAIETILE_EVENT_MEM_DM_PARITY_ERROR_BANK_5:
            case XAIETILE_EVENT_MEM_DM_PARITY_ERROR_BANK_6:
            case XAIETILE_EVENT_MEM_DM_PARITY_ERROR_BANK_7:
                severity = xrt_core::message::severity_level::XRT_CRITICAL;
                break;

            default:
                break;
        }
        break;

    case XAIEGBL_MODULE_PL:
        switch (error) {
            case XAIETILE_EVENT_SHIM_AXI_MM_SLAVE_TILE_ERROR:
            case XAIETILE_EVENT_SHIM_CONTROL_PKT_ERROR:
            case XAIETILE_EVENT_SHIM_AXI_MM_DECODE_NSU_ERROR_NOC:
            case XAIETILE_EVENT_SHIM_AXI_MM_SLAVE_NSU_ERROR_NOC:
            case XAIETILE_EVENT_SHIM_AXI_MM_UNSUPPORTED_TRAFFIC_NOC:
            case XAIETILE_EVENT_SHIM_AXI_MM_UNSECURE_ACCESS_IN_SECURE_MODE_NOC:
            case XAIETILE_EVENT_SHIM_AXI_MM_BYTE_STROBE_ERROR_NOC:
            case XAIETILE_EVENT_SHIM_DMA_S2MM_0_ERROR_NOC:
            case XAIETILE_EVENT_SHIM_DMA_S2MM_1_ERROR_NOC:
            case XAIETILE_EVENT_SHIM_DMA_MM2S_0_ERROR_NOC:
            case XAIETILE_EVENT_SHIM_DMA_MM2S_1_ERROR_NOC:
                severity = xrt_core::message::severity_level::XRT_EMERGENCY;
                break;

            default:
                break;
        }
        break;

    default:
        break;
    }

    xrt_core::message::send(severity, "XRT", "AIE ERROR: module %d, error %d", module, error);
#endif

    return XAIETILE_ERROR_HANDLED;
}

void
Aie::
sync_bo(uint64_t paddr, const char *gmioName, enum xclBOSyncDirection dir, size_t size)
{
  auto gmio = std::find_if(gmios.begin(), gmios.end(),
            [gmioName](gmio_type it) { return it.name.compare(gmioName) == 0; });

  if (gmio == gmios.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  submit_sync_bo(paddr, gmio, dir, size);

  ShimDMA *dmap = &shim_dma.at(gmio->shim_col);
  auto chan = gmio->channel_number;
  wait_sync_bo(dmap, chan, 0);
}

void
Aie::
sync_bo_nb(uint64_t paddr, const char *gmioName, enum xclBOSyncDirection dir, size_t size)
{
  auto gmio = std::find_if(gmios.begin(), gmios.end(),
            [gmioName](gmio_type it) { return it.name.compare(gmioName) == 0; });

  if (gmio == gmios.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  submit_sync_bo(paddr, gmio, dir, size);
}

void
Aie::
wait_gmio(const std::string& gmioName)
{
  auto gmio = std::find_if(gmios.begin(), gmios.end(),
            [gmioName](gmio_type it) { return it.name.compare(gmioName) == 0; });

  if (gmio == gmios.end())
    throw xrt_core::error(-EINVAL, "Can't wait GMIO: GMIO name not found");

  ShimDMA *dmap = &shim_dma.at(gmio->shim_col);
  auto chan = gmio->channel_number;
  wait_sync_bo(dmap, chan, 0);
}

void
Aie::
submit_sync_bo(uint64_t paddr, std::vector<gmio_type>::iterator& gmio, enum xclBOSyncDirection dir, size_t size)
{
  switch (dir) {
  case XCL_BO_SYNC_BO_GMIO_TO_AIE:
    if (gmio->type != 0)
      throw xrt_core::error(-EINVAL, "Sync BO direction does not match GMIO type");
    break;
  case XCL_BO_SYNC_BO_AIE_TO_GMIO:
    if (gmio->type != 1)
      throw xrt_core::error(-EINVAL, "Sync BO direction does not match GMIO type");
    break;
  default:
    throw xrt_core::error(-EINVAL, "Can't sync BO: unknown direction.");
  }

  if (size & XAIEDMA_SHIM_TXFER_LEN32_MASK != 0)
    throw xrt_core::error(-EINVAL, "Sync AIE Bo fails: size is not 32 bits aligned.");

  if (paddr & XAIEDMA_SHIM_ADDRLOW_ALIGN_MASK != 0)
    throw xrt_core::error(-EINVAL, "Sync AIE Bo fails: address is not 128 bits aligned.");

  ShimDMA *dmap = &shim_dma.at(gmio->shim_col);
  auto chan = gmio->channel_number;

  /* Find a free BD. Busy wait until we get one. */
  while (dmap->dma_chan[chan].idle_bds.empty()) {
    uint8_t npend = XAieDma_ShimPendingBdCount(&(dmap->handle), chan);
    int num_comp = XAIEGBL_NOC_DMASTA_STARTQ_MAX - npend;

    /* Pending BD is completed by order per Shim DMA spec. */
    for (int i = 0; i < num_comp; ++i) {
      BD bd = dmap->dma_chan[chan].pend_bds.front();
      dmap->dma_chan[chan].pend_bds.pop();
      dmap->dma_chan[chan].idle_bds.push(bd);
    }
  }

  BD bd = dmap->dma_chan[chan].idle_bds.front();
  dmap->dma_chan[chan].idle_bds.pop();
  bd.addr_high = get_bd_high_addr(paddr);
  bd.addr_low = get_bd_low_addr(paddr);

  XAieDma_ShimBdSetAddr(&(dmap->handle), bd.bd_num, bd.addr_high, bd.addr_low, size);

  /* Set BD lock */
  XAieDma_ShimBdSetLock(&(dmap->handle), bd.bd_num, bd.bd_num, 1, XAIEDMA_SHIM_LKACQRELVAL_INVALID, 1, XAIEDMA_SHIM_LKACQRELVAL_INVALID);

  /* Write BD */
  XAieDma_ShimBdWrite(&(dmap->handle), bd.bd_num);

  /* Enqueue BD */
  XAieDma_ShimSetStartBd((&(dmap->handle)), chan, bd.bd_num);
  dmap->dma_chan[chan].pend_bds.push(bd);
}

void
Aie::
wait_sync_bo(ShimDMA* const dmap, uint32_t chan, uint32_t timeout)
{
  while ((XAieDma_ShimWaitDone(&(dmap->handle), chan, timeout) != XAIEGBL_NOC_DMASTA_STA_IDLE));

  while (!dmap->dma_chan[chan].pend_bds.empty()) {
    BD bd = dmap->dma_chan[chan].pend_bds.front();
    dmap->dma_chan[chan].pend_bds.pop();
    dmap->dma_chan[chan].idle_bds.push(bd);
  }
}


}
