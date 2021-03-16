/**
 * Copyright (C) 2020-2021 Xilinx, Inc
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
#include "aie_event.h"
#ifndef __AIESIM__
#include "core/common/message.h"
#include "core/edge/user/shim.h"
#include "xaiengine/xlnx-ai-engine.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#endif

#include <cerrno>
#include <iostream>

namespace zynqaie {

XAie_InstDeclare(DevInst, &ConfigPtr);   // Declare global device instance

Aie::Aie(const std::shared_ptr<xrt_core::device>& device)
{
    adf::driver_config driver_config = xrt_core::edge::aie::get_driver_config(device.get());

    XAie_SetupConfig(ConfigPtr,
        driver_config.hw_gen,
        driver_config.base_address,
        driver_config.column_shift,
        driver_config.row_shift,
        driver_config.num_columns,
        driver_config.num_rows,
        driver_config.shim_row,
        driver_config.reserved_row_start,
        driver_config.reserved_num_rows,
        driver_config.aie_tile_row_start,
        driver_config.aie_tile_num_rows);

#ifndef __AIESIM__
    auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

    /* TODO get partition id and uid from XCLBIN or PDI */
    uint32_t partition_id = 1;
    uint32_t uid = 0;
    drm_zocl_aie_fd aiefd = { partition_id, uid, 0 };
    int ret = drv->getPartitionFd(aiefd);
    if (ret)
        throw xrt_core::error(ret, "Create AIE failed. Can not get AIE fd");
    fd = aiefd.fd;

    ConfigPtr.PartProp.Handle = fd;
#endif

    AieRC rc;
    if ((rc = XAie_CfgInitialize(&DevInst, &ConfigPtr)) != XAIE_OK)
        throw xrt_core::error(-EINVAL, "Failed to initialize AIE configuration: " + std::to_string(rc));
    devInst = &DevInst;
    adf::config_manager::initialize(devInst, driver_config.reserved_num_rows, false);

    auto core_device = xrt_core::get_userpf_device(device->get_device_handle());
    if(core_device->is_register_axlf()) {
    /* Initialize PLIO metadata */
        for (auto& plio : xrt_core::edge::aie::get_plios(device.get()))
            plios.emplace_back(std::move(plio));

    /* Initialize graph GMIO metadata */
        gmios = xrt_core::edge::aie::get_old_gmios(device.get());

    /* Initialize gmio api instances */
        gmio_configs = xrt_core::edge::aie::get_gmios(device.get());
        for (auto config_itr = gmio_configs.begin(); config_itr != gmio_configs.end(); config_itr++)
        {
            auto p_gmio_api = std::make_shared<adf::gmio_api>(&config_itr->second);
            p_gmio_api->configure();
            gmio_apis[config_itr->first] = p_gmio_api;
        }
    }

    Resources::AIE::initialize(driver_config.num_columns, driver_config.aie_tile_num_rows);
}

Aie::~Aie()
{
#ifndef __AIESIM__
  if (devInst)
    XAie_Finish(devInst);
#endif
}

XAie_DevInst* Aie::getDevInst()
{
  if (!devInst)
    throw xrt_core::error(-EINVAL, "AIE is not initialized");

  return devInst;
}

void
Aie::
sync_bo(xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  if (!devInst)
    throw xrt_core::error(-EINVAL, "Can't sync BO: AIE is not initialized");

  auto gmio_itr = gmio_apis.find(gmioName);
  if (gmio_itr == gmio_apis.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  auto gmio_config_itr = gmio_configs.find(gmioName);
  if (gmio_config_itr == gmio_configs.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  submit_sync_bo(bo, gmio_itr->second, gmio_config_itr->second, dir, size, offset);
  gmio_itr->second->wait();
}

void
Aie::
sync_bo_nb(xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  if (!devInst)
    throw xrt_core::error(-EINVAL, "Can't sync BO: AIE is not initialized");

  auto gmio_itr = gmio_apis.find(gmioName);
  if (gmio_itr == gmio_apis.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  auto gmio_config_itr = gmio_configs.find(gmioName);
  if (gmio_config_itr == gmio_configs.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  submit_sync_bo(bo, gmio_itr->second, gmio_config_itr->second, dir, size, offset);
}

void
Aie::
wait_gmio(const std::string& gmioName)
{
  if (!devInst)
    throw xrt_core::error(-EINVAL, "Can't wait GMIO: AIE is not initialized");

  auto gmio_itr = gmio_apis.find(gmioName);
  if (gmio_itr == gmio_apis.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  gmio_itr->second->wait();
}

void
Aie::
submit_sync_bo(xrt::bo& bo, std::shared_ptr<adf::gmio_api>& gmio_api, adf::gmio_config& gmio_config, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  switch (dir) {
  case XCL_BO_SYNC_BO_GMIO_TO_AIE:
    if (gmio_config.type != 0)
      throw xrt_core::error(-EINVAL, "Sync BO direction does not match GMIO type");
    break;
  case XCL_BO_SYNC_BO_AIE_TO_GMIO:
    if (gmio_config.type != 1)
      throw xrt_core::error(-EINVAL, "Sync BO direction does not match GMIO type");
    break;
  default:
    throw xrt_core::error(-EINVAL, "Can't sync BO: unknown direction.");
  }

  if (size & XAIEDMA_SHIM_TXFER_LEN32_MASK != 0)
    throw xrt_core::error(-EINVAL, "Sync AIE Bo fails: size is not 32 bits aligned.");

  BD bd;
  prepare_bd(bd, bo);
#ifndef __AIESIM__
  gmio_api->enqueueBD((uint64_t)bd.vaddr + offset, size);
#else
  gmio_api->enqueueBD((uint64_t)bo.address() + offset, size);
#endif
  clear_bd(bd);
}

void
Aie::
prepare_bd(BD& bd, xrt::bo& bo)
{
#ifndef __AIESIM__
  auto buf_fd = bo.export_buffer();
  if (buf_fd == XRT_NULL_BO_EXPORT)
    throw xrt_core::error(-errno, "Sync AIE Bo: fail to export BO.");
  bd.buf_fd = buf_fd;

  auto ret = ioctl(fd, AIE_ATTACH_DMABUF_IOCTL, buf_fd);
  if (ret)
    throw xrt_core::error(-errno, "Sync AIE Bo: fail to attach DMA buf.");

  auto bosize = bo.size();
  bd.size = bosize;

  bd.vaddr = reinterpret_cast<char *>(mmap(NULL, bosize, PROT_READ | PROT_WRITE, MAP_SHARED, buf_fd, 0));
#endif
}

void
Aie::
clear_bd(BD& bd)
{
#ifndef __AIESIM__
  munmap(bd.vaddr, bd.size);
  bd.vaddr = nullptr;
  auto ret = ioctl(fd, AIE_DETACH_DMABUF_IOCTL, bd.buf_fd);
  if (ret)
    throw xrt_core::error(-errno, "Sync AIE Bo: fail to detach DMA buf.");
  close(bd.buf_fd);
#endif
}

void
Aie::
reset(const xrt_core::device* device)
{
#ifndef __AIESIM__
    if (!devInst)
        throw xrt_core::error(-EINVAL, "Can't Reset AIE: AIE is not initialized");

    XAie_Finish(devInst);
    devInst = nullptr;

    auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

    /* TODO get partition id and uid from XCLBIN or PDI */
    uint32_t partition_id = 1;

    drm_zocl_aie_reset reset = { partition_id };
    int ret = drv->resetAIEArray(reset);
    if (ret)
        throw xrt_core::error(ret, "Fail to reset AIE Array");
#endif
}

int
Aie::
start_profiling(int option, const std::string& port1_name, const std::string& port2_name, uint32_t value)
{
  if (!devInst)
    throw xrt_core::error(-EINVAL, "Start profiling fails: AIE is not initialized");

  switch (option) {

  case IO_TOTAL_STREAM_RUNNING_TO_IDLE_CYCLE:
    return start_profiling_run_idle(port1_name);

  case IO_STREAM_START_TO_BYTES_TRANSFERRED_CYCLES:
    return start_profiling_start_bytes(port1_name, value);

  case IO_STREAM_START_DIFFERENCE_CYCLES:
    return start_profiling_diff_cycles(port1_name, port2_name);

  case IO_STREAM_RUNNING_EVENT_COUNT:
    return start_profiling_event_count(port1_name);

  default:
    throw xrt_core::error(-EINVAL, "Start profiling fails: unknown profiling option.");
  }
}

uint64_t
Aie::
read_profiling(int phdl)
{
  uint64_t value = 0;

  std::vector<Resources::AcquiredResource>& acquiredResourcesForThisHandle = eventRecords[phdl].acquiredResources;

  Resources::AcquiredResource& acquiredResource = acquiredResourcesForThisHandle[0];
  XAie_ModuleType XAieModuleType = AIEResourceModuletoXAieModuleTypeMap[acquiredResource.module];

  if (acquiredResource.resource == Resources::performance_counter)
    XAie_PerfCounterGet(devInst, acquiredResource.loc, XAieModuleType, acquiredResource.id, (u32*)(&value));
  else
    throw xrt_core::error(-EAGAIN, "Can't read profiling: The acquired resources order does not match the profiling option.");

  return value;
}

std::map<std::string , uint32_t> Aie::regmap = {
{"Core_R0", 0x00030000},
{"Core_R1", 0x00030010},
{"Core_R2", 0x00030020},
{"Core_R3", 0x00030030},
{"Core_R4", 0x00030040},
{"Core_R5", 0x00030050},
{"Core_R6", 0x00030060},
{"Core_R7", 0x00030070},
{"Core_R8", 0x00030080},
{"Core_R9", 0x00030090},
{"Core_R10", 0x000300A0},
{"Core_R11", 0x000300B0},
{"Core_R12", 0x000300C0},
{"Core_R13", 0x000300D0},
{"Core_R14", 0x000300E0},
{"Core_R15", 0x000300F0},
{"Core_P0", 0x00030100},
{"Core_P1", 0x00030110},
{"Core_P2", 0x00030120},
{"Core_P3", 0x00030130},
{"Core_P4", 0x00030140},
{"Core_P5", 0x00030150},
{"Core_P6", 0x00030160},
{"Core_P7", 0x00030170},
{"Core_CL0", 0x00030180},
{"Core_CH0", 0x00030190},
{"Core_CL1", 0x000301A0},
{"Core_CH1", 0x000301B0},
{"Core_CL2", 0x000301C0},
{"Core_CH2", 0x000301D0},
{"Core_CL3", 0x000301E0},
{"Core_CH3", 0x000301F0},
{"Core_CL4", 0x00030200},
{"Core_CH4", 0x00030210},
{"Core_CL5", 0x00030220},
{"Core_CH5", 0x00030230},
{"Core_CL6", 0x00030240},
{"Core_CH6", 0x00030250},
{"Core_CL7", 0x00030260},
{"Core_CH7", 0x00030270},
{"Core_PC", 0x00030280},
{"Core_FC", 0x00030290},
{"Core_SP", 0x000302A0},
{"Core_LR", 0x000302B0},
{"Core_M0", 0x000302C0},
{"Core_M1", 0x000302D0},
{"Core_M2", 0x000302E0},
{"Core_M3", 0x000302F0},
{"Core_M4", 0x00030300},
{"Core_M5", 0x00030310},
{"Core_M6", 0x00030320},
{"Core_M7", 0x00030330},
{"Core_CB0", 0x00030340},
{"Core_CB1", 0x00030350},
{"Core_CB2", 0x00030360},
{"Core_CB3", 0x00030370},
{"Core_CB4", 0x00030380},
{"Core_CB5", 0x00030390},
{"Core_CB6", 0x000303A0},
{"Core_CB7", 0x000303B0},
{"Core_CS0", 0x000303C0},
{"Core_CS1", 0x000303D0},
{"Core_CS2", 0x000303E0},
{"Core_CS3", 0x000303F0},
{"Core_CS4", 0x00030400},
{"Core_CS5", 0x00030410},
{"Core_CS6", 0x00030420},
{"Core_CS7", 0x00030430},
{"Core_MD0", 0x00030440},
{"Core_MD1", 0x00030450},
{"Core_MC0", 0x00030460},
{"Core_MC1", 0x00030470},
{"Core_S0", 0x00030480},
{"Core_S1", 0x00030490},
{"Core_S2", 0x000304A0},
{"Core_S3", 0x000304B0},
{"Core_S4", 0x000304C0},
{"Core_S5", 0x000304D0},
{"Core_S6", 0x000304E0},
{"Core_S7", 0x000304F0},
{"Core_LS", 0x00030500},
{"Core_LE", 0x00030510},
{"Core_LC", 0x00030520},
{"Performance_Ctrl0", 0x00031000},
{"Performance_Ctrl1", 0x00031004},
{"Performance_Ctrl2", 0x00031008},
{"Performance_Counter0", 0x00031020},
{"Performance_Counter1", 0x00031024},
{"Performance_Counter2", 0x00031028},
{"Performance_Counter3", 0x0003102C},
{"Performance_Counter0_Event_Value", 0x00031080},
{"Performance_Counter1_Event_Value", 0x00031084},
{"Performance_Counter2_Event_Value", 0x00031088},
{"Performance_Counter3_Event_Value", 0x0003108C},
{"Core_Control", 0x00032000},
{"Core_Status", 0x00032004},
{"Enable_Events", 0x00032008},
{"Reset_Event", 0x0003200C},
{"Debug_Control0", 0x00032010},
{"Debug_Control1", 0x00032014},
{"Debug_Control2", 0x00032018},
{"Debug_Status", 0x0003201C},
{"PC_Event0", 0x00032020},
{"PC_Event1", 0x00032024},
{"PC_Event2", 0x00032028},
{"PC_Event3", 0x0003202C},
{"Error_Halt_Control", 0x00032030},
{"Error_Halt_Event", 0x00032034},
{"ECC_Control", 0x00032100},
{"ECC_Scrubbing_Event", 0x00032110 },
{"ECC_Failing_Address", 0x00032120},
{"ECC_Instruction_Word_0", 0x00032130},
{"ECC_Instruction_Word_1", 0x00032134 },
{"ECC_Instruction_Word_2", 0x00032138 },
{"ECC_Instruction_Word_3", 0x0003213C },
{"Timer_Control", 0x00034000},
{"Event_Generate", 0x00034008 },
{"Event_Broadcast0", 0x00034010 },
{"Event_Broadcast1", 0x00034014 },
{"Event_Broadcast2", 0x00034018 },
{"Event_Broadcast3", 0x0003401C },
{"Event_Broadcast4", 0x00034020 },
{"Event_Broadcast5", 0x00034024 },
{"Event_Broadcast6", 0x00034028 },
{"Event_Broadcast7", 0x0003402C },
{"Event_Broadcast8", 0x00034030 },
{"Event_Broadcast9", 0x00034034 },
{"Event_Broadcast10", 0x00034038 },
{"Event_Broadcast11", 0x0003403C },
{"Event_Broadcast12", 0x00034040 },
{"Event_Broadcast13", 0x00034044 },
{"Event_Broadcast14", 0x00034048 },
{"Event_Broadcast15", 0x0003404C},
{"Event_Broadcast_Block_South_Set", 0x00034050},
{"Event_Broadcast_Block_South_Clr", 0x00034054},
{"Event_Broadcast_Block_South_Value", 0x00034058},
{"Event_Broadcast_Block_West_Set", 0x00034060},
{"Event_Broadcast_Block_West_Clr", 0x00034064 },
{"Event_Broadcast_Block_West_Value", 0x00034068},
{"Event_Broadcast_Block_North_Set", 0x00034070 },
{"Event_Broadcast_Block_North_Clr", 0x00034074 },
{"Event_Broadcast_Block_North_Value", 0x00034078},
{"Event_Broadcast_Block_East_Set", 0x00034080 },
{"Event_Broadcast_Block_East_Clr", 0x00034084 },
{"Event_Broadcast_Block_East_Value", 0x00034088},
{"Trace_Control0", 0x000340D0},
{"Trace_Control1", 0x000340D4},
{"Trace_Status", 0x000340D8},
{"Trace_Event0", 0x000340E0},
{"Trace_Event1", 0x000340E4},
{"Timer_Trig_Event_Low_Value", 0x000340F0},
{"Timer_Trig_Event_High_Value", 0x000340F4},
{"Timer_Low", 0x000340F8},
{"Timer_High", 0x000340FC },
{"Event_Status0", 0x00034200 },
{"Event_Status1", 0x00034204 },
{"Event_Status2", 0x00034208 },
{"Event_Status3", 0x0003420C },
{"Combo_event_inputs", 0x00034400 },
{"Combo_event_control", 0x00034404 },
{"Event_Group_0_Enable", 0x00034500 },
{"Event_Group_PC_Enable", 0x00034504 },
{"Event_Group_Core_Stall_Enable", 0x00034508},
{"Event_Group_Core_Program_Flow_Enable", 0x0003450C},
{"Event_Group_Errors0_Enable", 0x00034510},
{"Event_Group_Errors1_Enable", 0x00034514},
{"Event_Group_Stream_Switch_Enable", 0x00034518 },
{"Event_Group_Broadcast_Enable", 0x0003451C },
{"Event_Group_User_Event_Enable", 0x00034520},
{"Tile_Control", 0x00036030},
{"Tile_Control_Packet_Handler_Status", 0x00036034},
{"Tile_Clock_Control", 0x00036040},
{"CSSD_Trigger", 0x00036044},
{"Spare_Reg", 0x00036050},
{"Stream_Switch_Master_Config_ME_Core0", 0x0003F000},
{"Stream_Switch_Master_Config_ME_Core1", 0x0003F004 },
{"Stream_Switch_Master_Config_DMA0", 0x0003F008},
{"Stream_Switch_Master_Config_DMA1", 0x0003F00C },
{"Stream_Switch_Master_Config_Tile_Ctrl", 0x0003F010},
{"Stream_Switch_Master_Config_FIFO0", 0x0003F014},
{"Stream_Switch_Master_Config_FIFO1", 0x0003F018},
{"Stream_Switch_Master_Config_South0", 0x0003F01C},
{"Stream_Switch_Master_Config_South1", 0x0003F020},
{"Stream_Switch_Master_Config_South2", 0x0003F024},
{"Stream_Switch_Master_Config_South3", 0x0003F028},
{"Stream_Switch_Master_Config_West0", 0x0003F02C},
{"Stream_Switch_Master_Config_West1", 0x0003F030},
{"Stream_Switch_Master_Config_West2", 0x0003F034},
{"Stream_Switch_Master_Config_West3", 0x0003F038},
{"Stream_Switch_Master_Config_North0", 0x0003F03C},
{"Stream_Switch_Master_Config_North1", 0x0003F040},
{"Stream_Switch_Master_Config_North2", 0x0003F044},
{"Stream_Switch_Master_Config_North3", 0x0003F048},
{"Stream_Switch_Master_Config_North4", 0x0003F04C},
{"Stream_Switch_Master_Config_North5", 0x0003F050},
{"Stream_Switch_Master_Config_East0", 0x0003F054},
{"Stream_Switch_Master_Config_East1", 0x0003F058},
{"Stream_Switch_Master_Config_East2", 0x0003F05C},
{"Stream_Switch_Master_Config_East3", 0x0003F060},
{"Stream_Switch_Slave_ME_Core0_Config", 0x0003F100},
{"Stream_Switch_Slave_ME_Core1_Config", 0x0003F104},
{"Stream_Switch_Slave_DMA_0_Config", 0x0003F108},
{"Stream_Switch_Slave_DMA_1_Config", 0x0003F10C},
{"Stream_Switch_Slave_Tile_Ctrl_Config", 0x0003F110},
{"Stream_Switch_Slave_FIFO_0_Config", 0x0003F114},
{"Stream_Switch_Slave_FIFO_1_Config", 0x0003F118},
{"Stream_Switch_Slave_South_0_Config", 0x0003F11C},
{"Stream_Switch_Slave_South_1_Config", 0x0003F120},
{"Stream_Switch_Slave_South_2_Config", 0x0003F124},
{"Stream_Switch_Slave_South_3_Config", 0x0003F128},
{"Stream_Switch_Slave_South_4_Config", 0x0003F12C},
{"Stream_Switch_Slave_South_5_Config", 0x0003F130},
{"Stream_Switch_Slave_West_0_Config", 0x0003F134},
{"Stream_Switch_Slave_West_1_Config", 0x0003F138},
{"Stream_Switch_Slave_West_2_Config", 0x0003F13C},
{"Stream_Switch_Slave_West_3_Config", 0x0003F140},
{"Stream_Switch_Slave_North_0_Config", 0x0003F144},
{"Stream_Switch_Slave_North_1_Config", 0x0003F148},
{"Stream_Switch_Slave_North_2_Config", 0x0003F14C},
{"Stream_Switch_Slave_North_3_Config", 0x0003F150},
{"Stream_Switch_Slave_East_0_Config", 0x0003F154},
{"Stream_Switch_Slave_East_1_Config", 0x0003F158},
{"Stream_Switch_Slave_East_2_Config", 0x0003F15C},
{"Stream_Switch_Slave_East_3_Config", 0x0003F160},
{"Stream_Switch_Slave_ME_Trace_Config", 0x0003F164},
{"Stream_Switch_Slave_Mem_Trace_Config", 0x0003F168},
{"Stream_Switch_Slave_ME_Core0_Slot0", 0x0003F200},
{"Stream_Switch_Slave_ME_Core0_Slot1", 0x0003F204},
{"Stream_Switch_Slave_ME_Core0_Slot2", 0x0003F208},
{"Stream_Switch_Slave_ME_Core0_Slot3", 0x0003F20C},
{"Stream_Switch_Slave_ME_Core1_Slot0", 0x0003F210},
{"Stream_Switch_Slave_ME_Core1_Slot1", 0x0003F214},
{"Stream_Switch_Slave_ME_Core1_Slot2", 0x0003F218},
{"Stream_Switch_Slave_ME_Core1_Slot3", 0x0003F21C},
{"Stream_Switch_Slave_DMA_0_Slot0", 0x0003F220},
{"Stream_Switch_Slave_DMA_0_Slot1", 0x0003F224},
{"Stream_Switch_Slave_DMA_0_Slot2", 0x0003F228},
{"Stream_Switch_Slave_DMA_0_Slot3", 0x0003F22C},
{"Stream_Switch_Slave_DMA_1_Slot0", 0x0003F230},
{"Stream_Switch_Slave_DMA_1_Slot1", 0x0003F234},
{"Stream_Switch_Slave_DMA_1_Slot2", 0x0003F238},
{"Stream_Switch_Slave_DMA_1_Slot3", 0x0003F23C},
{"Stream_Switch_Slave_Tile_Ctrl_Slot0", 0x0003F240},
{"Stream_Switch_Slave_Tile_Ctrl_Slot1", 0x0003F244},
{"Stream_Switch_Slave_Tile_Ctrl_Slot2", 0x0003F248},
{"Stream_Switch_Slave_Tile_Ctrl_Slot3", 0x0003F24C},
{"Stream_Switch_Slave_FIFO_0_Slot0", 0x0003F250},
{"Stream_Switch_Slave_FIFO_0_Slot1", 0x0003F254},
{"Stream_Switch_Slave_FIFO_0_Slot2", 0x0003F258},
{"Stream_Switch_Slave_FIFO_0_Slot3", 0x0003F25C},
{"Stream_Switch_Slave_FIFO_1_Slot0", 0x0003F260},
{"Stream_Switch_Slave_FIFO_1_Slot1", 0x0003F264},
{"Stream_Switch_Slave_FIFO_1_Slot2", 0x0003F268},
{"Stream_Switch_Slave_FIFO_1_Slot3", 0x0003F26C},
{"Stream_Switch_Slave_South_0_Slot0", 0x0003F270},
{"Stream_Switch_Slave_South_0_Slot1", 0x0003F274},
{"Stream_Switch_Slave_South_0_Slot2", 0x0003F278},
{"Stream_Switch_Slave_South_0_Slot3", 0x0003F27C},
{"Stream_Switch_Slave_South_1_Slot0", 0x0003F280},
{"Stream_Switch_Slave_South_1_Slot1", 0x0003F284},
{"Stream_Switch_Slave_South_1_Slot2", 0x0003F288},
{"Stream_Switch_Slave_South_1_Slot3", 0x0003F28C},
{"Stream_Switch_Slave_South_2_Slot0", 0x0003F290},
{"Stream_Switch_Slave_South_2_Slot1", 0x0003F294},
{"Stream_Switch_Slave_South_2_Slot2", 0x0003F298},
{"Stream_Switch_Slave_South_2_Slot3", 0x0003F29C},
{"Stream_Switch_Slave_South_3_Slot0", 0x0003F2A0},
{"Stream_Switch_Slave_South_3_Slot1", 0x0003F2A4},
{"Stream_Switch_Slave_South_3_Slot2", 0x0003F2A8},
{"Stream_Switch_Slave_South_3_Slot3", 0x0003F2AC},
{"Stream_Switch_Slave_South_4_Slot0", 0x0003F2B0},
{"Stream_Switch_Slave_South_4_Slot1", 0x0003F2B4},
{"Stream_Switch_Slave_South_4_Slot2", 0x0003F2B8},
{"Stream_Switch_Slave_South_4_Slot3", 0x0003F2BC},
{"Stream_Switch_Slave_South_5_Slot0", 0x0003F2C0},
{"Stream_Switch_Slave_South_5_Slot1", 0x0003F2C4},
{"Stream_Switch_Slave_South_5_Slot2", 0x0003F2C8},
{"Stream_Switch_Slave_South_5_Slot3", 0x0003F2CC},
{"Stream_Switch_Slave_West_0_Slot0", 0x0003F2D0},
{"Stream_Switch_Slave_West_0_Slot1", 0x0003F2D4},
{"Stream_Switch_Slave_West_0_Slot2", 0x0003F2D8},
{"Stream_Switch_Slave_West_0_Slot3", 0x0003F2DC},
{"Stream_Switch_Slave_West_1_Slot0", 0x0003F2E0},
{"Stream_Switch_Slave_West_1_Slot1", 0x0003F2E4},
{"Stream_Switch_Slave_West_1_Slot2", 0x0003F2E8},
{"Stream_Switch_Slave_West_1_Slot3", 0x0003F2EC},
{"Stream_Switch_Slave_West_2_Slot0", 0x0003F2F0},
{"Stream_Switch_Slave_West_2_Slot1", 0x0003F2F4},
{"Stream_Switch_Slave_West_2_Slot2", 0x0003F2F8},
{"Stream_Switch_Slave_West_2_Slot3", 0x0003F2FC},
{"Stream_Switch_Slave_West_3_Slot0", 0x0003F300},
{"Stream_Switch_Slave_West_3_Slot1", 0x0003F304},
{"Stream_Switch_Slave_West_3_Slot2", 0x0003F308},
{"Stream_Switch_Slave_West_3_Slot3", 0x0003F30C},
{"Stream_Switch_Slave_North_0_Slot0", 0x0003F310},
{"Stream_Switch_Slave_North_0_Slot1", 0x0003F314},
{"Stream_Switch_Slave_North_0_Slot2", 0x0003F318},
{"Stream_Switch_Slave_North_0_Slot3", 0x0003F31C},
{"Stream_Switch_Slave_North_1_Slot0", 0x0003F320},
{"Stream_Switch_Slave_North_1_Slot1", 0x0003F324},
{"Stream_Switch_Slave_North_1_Slot2", 0x0003F328},
{"Stream_Switch_Slave_North_1_Slot3", 0x0003F32C},
{"Stream_Switch_Slave_North_2_Slot0", 0x0003F330},
{"Stream_Switch_Slave_North_2_Slot1", 0x0003F334},
{"Stream_Switch_Slave_North_2_Slot2", 0x0003F338},
{"Stream_Switch_Slave_North_2_Slot3", 0x0003F33C},
{"Stream_Switch_Slave_North_3_Slot0", 0x0003F340},
{"Stream_Switch_Slave_North_3_Slot1", 0x0003F344},
{"Stream_Switch_Slave_North_3_Slot2", 0x0003F348},
{"Stream_Switch_Slave_North_3_Slot3", 0x0003F34C},
{"Stream_Switch_Slave_East_0_Slot0", 0x0003F350},
{"Stream_Switch_Slave_East_0_Slot1", 0x0003F354},
{"Stream_Switch_Slave_East_0_Slot2", 0x0003F358},
{"Stream_Switch_Slave_East_0_Slot3", 0x0003F35C},
{"Stream_Switch_Slave_East_1_Slot0", 0x0003F360},
{"Stream_Switch_Slave_East_1_Slot1", 0x0003F364},
{"Stream_Switch_Slave_East_1_Slot2", 0x0003F368},
{"Stream_Switch_Slave_East_1_Slot3", 0x0003F36C},
{"Stream_Switch_Slave_East_2_Slot0", 0x0003F370},
{"Stream_Switch_Slave_East_2_Slot1", 0x0003F374},
{"Stream_Switch_Slave_East_2_Slot2", 0x0003F378},
{"Stream_Switch_Slave_East_2_Slot3", 0x0003F37C},
{"Stream_Switch_Slave_East_3_Slot0", 0x0003F380},
{"Stream_Switch_Slave_East_3_Slot1", 0x0003F384},
{"Stream_Switch_Slave_East_3_Slot2", 0x0003F388},
{"Stream_Switch_Slave_East_3_Slot3", 0x0003F38C},
{"Stream_Switch_Slave_ME_Trace_Slot0", 0x0003F390},
{"Stream_Switch_Slave_ME_Trace_Slot1", 0x0003F394},
{"Stream_Switch_Slave_ME_Trace_Slot2", 0x0003F398},
{"Stream_Switch_Slave_ME_Trace_Slot3", 0x0003F39C},
{"Stream_Switch_Slave_Mem_Trace_Slot0", 0x0003F3A0},
{"Stream_Switch_Slave_Mem_Trace_Slot1", 0x0003F3A4},
{"Stream_Switch_Slave_Mem_Trace_Slot2", 0x0003F3A8},
{"Stream_Switch_Slave_Mem_Trace_Slot3", 0x0003F3AC},
{"Stream_Switch_Event_Port_Selection_0", 0x0003FF00},
{"Stream_Switch_Event_Port_Selection_1", 0x0003FF04} };

void
Aie::
read_core_reg(int row, int col, std::string regName, uint32_t* value)
{
#ifndef __AIESIM__
    row+=1;
    auto it = regmap.find(regName);
    if (it == regmap.end())
        throw xrt_core::error(-EINVAL,"invalid register");
    if(row > 0 && row <= XAIE_NUM_ROWS && col >= 0 && col < XAIE_NUM_COLS) {
        if(devInst) {
            if(XAie_Read32(devInst,it->second + _XAie_GetTileAddr(devInst,row,col),value)) {
                throw xrt_core::error(-EINVAL,"error reading register");
            }
        } else {
            throw xrt_core::error(-EINVAL,"invalid aie object");
        }
   } else
       throw xrt_core::error(-EINVAL,"invalid row or column");
#endif
}

void
Aie::
stop_profiling(int phdl)
{
  if (phdl < eventRecords.size() && eventRecords[phdl].option >= 0) {
    std::vector<Resources::AcquiredResource>& acquiredResourcesForThisHandle = eventRecords[phdl].acquiredResources;
    for (int i = 0; i < acquiredResourcesForThisHandle.size(); i++) {
      Resources::AcquiredResource& acquiredResource = acquiredResourcesForThisHandle[i];
      XAie_ModuleType XAieModuleType = AIEResourceModuletoXAieModuleTypeMap[acquiredResource.module];

      if (acquiredResource.resource == Resources::performance_counter) {
        u8 counterId = acquiredResource.id;

        XAie_PerfCounterReset(devInst, acquiredResource.loc, XAieModuleType, counterId);
        XAie_PerfCounterResetControlReset(devInst, acquiredResource.loc, XAieModuleType, counterId);

        if (acquiredResource.module == Resources::pl_module)
          Resources::AIE::getShimTile(acquiredResource.loc.Col)->plModule.releasePerformanceCounter(phdl, counterId);
        else if (acquiredResource.module == Resources::core_module)
          Resources::AIE::getAIETile(acquiredResource.loc.Col, acquiredResource.loc.Row - 1)->coreModule.releasePerformanceCounter(phdl, counterId);
      } else if (acquiredResource.resource == Resources::stream_switch_event_port) {
        u8 eventPortId = acquiredResource.id;

        XAie_EventSelectStrmPortReset(devInst, acquiredResource.loc, eventPortId);

        if (acquiredResource.module == Resources::pl_module)
          Resources::AIE::getShimTile(acquiredResource.loc.Col)->plModule.releaseStreamEventPort(phdl, eventPortId);
      }
    }
  }
}

void
Aie::
get_profiling_config(const std::string& port_name, XAie_LocType& out_shim_tile, XAie_StrmPortIntf& out_mode, uint8_t& out_stream_id)
{
  auto gmio = std::find_if(gmios.begin(), gmios.end(),
            [&port_name](auto& it) { return it.name.compare(port_name) == 0; });

  // For PLIO inside graph, there is no name property.
  // So we need to match logical name too
  auto plio = std::find_if(plios.begin(), plios.end(),
            [&port_name](auto& it) { return it.name.compare(port_name) == 0; });
  if (plio == plios.end()) {
    plio = std::find_if(plios.begin(), plios.end(),
            [&port_name](auto& it) { return it.logical_name.compare(port_name) == 0; });
  }

  if (gmio == gmios.end() && plio == plios.end())
    throw xrt_core::error(-EINVAL, "Can't start profiling: port name '" + port_name + "' not found");

  if (gmio != gmios.end() && plio != plios.end())
    throw xrt_core::error(-EINVAL, "Can't start profiling: ambiguous port name '" + port_name + "'");

  XAie_LocType shim_tile;
  XAie_StrmPortIntf mode;
  uint8_t stream_id;
  if (gmio != gmios.end()) {
    shim_tile = XAie_TileLoc(gmio->shim_col, 0);
    /* type 0: GM->AIE; type 1: AIE->GM */
    mode = gmio->type == 1 ? XAIE_STRMSW_MASTER : XAIE_STRMSW_SLAVE;
    stream_id = gmio->stream_id;
  } else {
    shim_tile = XAie_TileLoc(plio->shim_col, 0);
    mode = plio->is_master ? XAIE_STRMSW_MASTER: XAIE_STRMSW_SLAVE;
    stream_id = plio->stream_id;
  }

  out_shim_tile = shim_tile;
  out_mode = mode;
  out_stream_id = stream_id;
}

int
Aie::
start_profiling_run_idle(const std::string& port_name)
{
  int handle = -1;

  XAie_LocType shim_tile;
  XAie_StrmPortIntf mode;
  uint8_t stream_id;
  get_profiling_config(port_name, shim_tile, mode, stream_id);

  int handleId = eventRecords.size();
  int eventPortId = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestStreamEventPort(handleId);
  int counterId = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestPerformanceCounter(handleId);
  if (counterId >= 0 && eventPortId >= 0) {
    XAie_EventSelectStrmPort(devInst, shim_tile, (uint8_t)eventPortId, mode, SOUTH, stream_id);
    XAie_PerfCounterControlSet(devInst, shim_tile, XAIE_PL_MOD, (uint8_t)counterId, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId], XAIETILE_EVENT_SHIM_PORT_IDLE[eventPortId]);
    eventRecords.push_back( { IO_TOTAL_STREAM_RUNNING_TO_IDLE_CYCLE,
                { { shim_tile, Resources::pl_module, Resources::performance_counter, (size_t)counterId },
                { shim_tile, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId } } } );
    handle = handleId;
  } else {
    if (counterId >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releasePerformanceCounter(handleId, counterId);
    if (eventPortId >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releaseStreamEventPort(handleId, eventPortId);
    throw xrt_core::error(-EAGAIN, "Can't start profiling: Failed to request performance counter or stream switch event port resources.");
  }

  return handle;
}

int
Aie::
start_profiling_start_bytes(const std::string& port_name, uint32_t value)
{
  int handle = -1;

  XAie_LocType shim_tile;
  XAie_StrmPortIntf mode;
  uint8_t stream_id;

  get_profiling_config(port_name, shim_tile, mode, stream_id);

  int handleId = eventRecords.size();
  int eventPortId = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestStreamEventPort(handleId);
  int counterId0 = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestPerformanceCounter(handleId);
  int counterId1 = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestPerformanceCounter(handleId);

  if (counterId0 >= 0 && counterId1 >= 0 && eventPortId >= 0) {
    XAie_EventSelectStrmPort(devInst, shim_tile, (uint8_t)eventPortId, mode, SOUTH, stream_id);
    XAie_PerfCounterEventValueSet(devInst, shim_tile, XAIE_PL_MOD, (uint8_t)counterId1, value / 4);
    XAie_PerfCounterControlSet(devInst, shim_tile, XAIE_PL_MOD, (uint8_t)counterId0, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId], XAIE_EVENT_PERF_CNT_1_PL);
    XAie_PerfCounterControlSet(devInst, shim_tile, XAIE_PL_MOD, (uint8_t)counterId1, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId], XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId]);
    eventRecords.push_back( { IO_STREAM_START_TO_BYTES_TRANSFERRED_CYCLES,
                { { shim_tile, Resources::pl_module, Resources::performance_counter, (size_t)counterId0 },
                { shim_tile, Resources::pl_module, Resources::performance_counter, (size_t)counterId1 },
                { shim_tile, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId } } } );

    handle = handleId;
  } else {
    if (counterId0 >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releasePerformanceCounter(handleId, counterId0);
    if (counterId1 >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releasePerformanceCounter(handleId, counterId1);
    if (eventPortId >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releaseStreamEventPort(handleId, eventPortId);
    throw xrt_core::error(-EAGAIN, "Can't start profiling: Failed to request performance counter or stream switch event port resources.");
  }

  return handle;
}

int
Aie::
start_profiling_diff_cycles(const std::string& port1_name, const std::string& port2_name)
{
  int handle = -1;

  XAie_LocType shim_tile1;
  XAie_StrmPortIntf mode1;
  uint8_t stream_id1;
  XAie_LocType shim_tile2;
  XAie_StrmPortIntf mode2;
  uint8_t stream_id2;

  get_profiling_config(port1_name, shim_tile1, mode1, stream_id1);
  get_profiling_config(port2_name, shim_tile2, mode2, stream_id2);

  int handleId = eventRecords.size();
  int eventPortId1 = Resources::AIE::getShimTile(shim_tile1.Col)->plModule.requestStreamEventPort(handleId);
  int counterId1 = Resources::AIE::getShimTile(shim_tile1.Col)->plModule.requestPerformanceCounter(handleId);
  int eventPortId2 = Resources::AIE::getShimTile(shim_tile2.Col)->plModule.requestStreamEventPort(handleId);
  int counterId2 = Resources::AIE::getShimTile(shim_tile2.Col)->plModule.requestPerformanceCounter(handleId);

  if (counterId1 >= 0 && eventPortId1 >= 0 && counterId2 >= 0 && eventPortId2 >= 0) {
    if (shim_tile1.Col == shim_tile2.Col) {
      XAie_EventSelectStrmPort(devInst, shim_tile1, (uint8_t)eventPortId1, mode1, SOUTH, stream_id1);
      XAie_PerfCounterControlSet(devInst, shim_tile1, XAIE_PL_MOD, (uint8_t)counterId1, XAIE_EVENT_USER_EVENT_0_PL, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId1]);
      XAie_EventSelectStrmPort(devInst, shim_tile2, (uint8_t)eventPortId2, mode2, SOUTH, stream_id2);
      XAie_PerfCounterControlSet(devInst, shim_tile2, XAIE_PL_MOD, (uint8_t)counterId2, XAIE_EVENT_USER_EVENT_0_PL, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId2]);
      XAie_EventGenerate(devInst, shim_tile1, XAIE_PL_MOD, XAIE_EVENT_USER_EVENT_0_PL);
      eventRecords.push_back({ IO_STREAM_START_DIFFERENCE_CYCLES,
                  { { shim_tile1, Resources::pl_module, Resources::performance_counter, (size_t)counterId1 },
                  { shim_tile2, Resources::pl_module, Resources::performance_counter, (size_t)counterId2 },
                  { shim_tile1, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId1 },
                  { shim_tile2, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId2 } } });
      handle = handleId;
    } else {
      int westShimColumn = (shim_tile1.Col < shim_tile2.Col) ? shim_tile1.Col : shim_tile2.Col;
      int eastShimColumn = (shim_tile1.Col < shim_tile2.Col) ? shim_tile2.Col : shim_tile1.Col;
      int numBcastShimColumns = eastShimColumn - westShimColumn + 1;
      int broadcastId = -1;

      std::vector<std::vector<short>> eventBroadcastResourcesOnShimColumns(numBcastShimColumns);
      for (int i = 0; i < numBcastShimColumns; i++)
        eventBroadcastResourcesOnShimColumns[i] = Resources::AIE::getShimTile(westShimColumn + i)->plModule.availableEventBroadcast();
      int largestBroadcastIndexAvailableForAllShimColumns;
      for (largestBroadcastIndexAvailableForAllShimColumns = NUM_EVENT_BROADCASTS - 1; largestBroadcastIndexAvailableForAllShimColumns >= 0;
                  largestBroadcastIndexAvailableForAllShimColumns--) {
        bool allAvailable = true;
        for (int i = 0; i < numBcastShimColumns; i++) {
          if (eventBroadcastResourcesOnShimColumns[i][largestBroadcastIndexAvailableForAllShimColumns] != -1) {
            allAvailable = false;
            break;
          }
        }
        if (allAvailable)
          break;
      }
      broadcastId = largestBroadcastIndexAvailableForAllShimColumns;

      if (broadcastId >= 0) {
        for (int i = 0; i < numBcastShimColumns; i++)
          Resources::AIE::getShimTile(westShimColumn + i)->plModule.requestEventBroadcast(handleId, broadcastId);
      }

      if (broadcastId >= 0) {
        XAie_EventSelectStrmPort(devInst, shim_tile1, (uint8_t)eventPortId1, mode1, SOUTH, stream_id1);
        XAie_PerfCounterControlSet(devInst, shim_tile1, XAIE_PL_MOD, (uint8_t)counterId1, XAIE_EVENT_USER_EVENT_0_PL, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId1]);
        XAie_EventSelectStrmPort(devInst, shim_tile2, (uint8_t)eventPortId2, mode2, SOUTH, stream_id2);
        XAie_PerfCounterControlSet(devInst, shim_tile2, XAIE_PL_MOD, (uint8_t)counterId2, XAIETILE_EVENT_SHIM_BROADCAST_A[broadcastId], XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId2]);

        u16 bcastMask = (1 << broadcastId);
        XAie_LocType westTileLoc = XAie_TileLoc(westShimColumn, 0);
        XAie_EventBroadcastBlockMapDir(devInst, westTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, bcastMask, XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH);
        XAie_EventBroadcastBlockMapDir(devInst, westTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, bcastMask, XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH);

        for (int i = 1; i < numBcastShimColumns - 1; i++) {
          XAie_LocType intermediateTileLoc = XAie_TileLoc(westShimColumn + i, 0);
          XAie_EventBroadcastBlockMapDir(devInst, intermediateTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, bcastMask, XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH);
          XAie_EventBroadcastBlockMapDir(devInst, intermediateTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, bcastMask, XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH);
        }

        XAie_LocType eastTileLoc = XAie_TileLoc(eastShimColumn, 0);
        XAie_EventBroadcastBlockMapDir(devInst, eastTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, bcastMask, XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH);

        XAie_EventBroadcast(devInst, shim_tile1, XAIE_PL_MOD, (uint8_t)broadcastId, XAIE_EVENT_USER_EVENT_0_PL);
        XAie_EventGenerate(devInst, shim_tile1, XAIE_PL_MOD, XAIE_EVENT_USER_EVENT_0_PL);

        eventRecords.push_back({ IO_STREAM_START_DIFFERENCE_CYCLES,
                    { { shim_tile1, Resources::pl_module, Resources::performance_counter, (size_t)counterId1 },
                    { shim_tile2, Resources::pl_module, Resources::performance_counter, (size_t)counterId2 },
                    { shim_tile1, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId1 },
                    { shim_tile2, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId2 },
                    { shim_tile1, Resources::pl_module, Resources::event_broadcast, (size_t)broadcastId } } });

        handle = handleId;
      } else
        throw xrt_core::error(-EAGAIN, "Can't start profiling: Failed to request event broadcast resources across shim tiles.");
    }
  } else
    throw xrt_core::error(-EAGAIN, "Can't start profiling: Failed to request performance counter or stream switch event port resources.");

  return handle;
}

int
Aie::
start_profiling_event_count(const std::string& port_name)
{
  int handle = -1;

  XAie_LocType shim_tile;
  XAie_StrmPortIntf mode;
  uint8_t stream_id;

  get_profiling_config(port_name, shim_tile, mode, stream_id);

  int handleId = eventRecords.size();
  int eventPortId = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestStreamEventPort(handleId);
  int counterId = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestPerformanceCounter(handleId);

  if (counterId >= 0 && eventPortId >= 0) {
    XAie_EventSelectStrmPort(devInst, shim_tile, (uint8_t)eventPortId, mode, SOUTH, stream_id);
    XAie_PerfCounterControlSet(devInst, shim_tile, XAIE_PL_MOD, (uint8_t)counterId, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId],                                     XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId]);
    eventRecords.push_back({ IO_STREAM_RUNNING_EVENT_COUNT,
                { { shim_tile, Resources::pl_module, Resources::performance_counter, (size_t)counterId },
                { shim_tile, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId } } });
    handle = handleId;
  } else {
    if (counterId >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releasePerformanceCounter(handleId, counterId);
    if (eventPortId >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releaseStreamEventPort(handleId, eventPortId);
    throw xrt_core::error(-EAGAIN, "Can't start profiling: Failed to request performance counter or stream switch event port resources.");
  }

  return handle;
}

}
