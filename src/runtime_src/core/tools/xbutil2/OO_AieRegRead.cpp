// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_AieRegRead.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>

namespace po = boost::program_options;
namespace qr = xrt_core::query;

// System - Include Files
#include <iostream>

// ----- C L A S S   M E T H O D S -------------------------------------------

const std::vector<std::string> OO_AieRegRead::regmap = {
  "Core_R0",
  "Core_R1",
  "Core_R2",
  "Core_R3",
  "Core_R4",
  "Core_R5",
  "Core_R6",
  "Core_R7",
  "Core_R8",
  "Core_R9",
  "Core_R10",
  "Core_R11",
  "Core_R12",
  "Core_R13",
  "Core_R14",
  "Core_R15",
  "Core_P0",
  "Core_P1",
  "Core_P2",
  "Core_P3",
  "Core_P4",
  "Core_P5",
  "Core_P6",
  "Core_P7",
  "Core_CL0",
  "Core_CH0",
  "Core_CL1",
  "Core_CH1",
  "Core_CL2",
  "Core_CH2",
  "Core_CL3",
  "Core_CH3",
  "Core_CL4",
  "Core_CH4",
  "Core_CL5",
  "Core_CH5",
  "Core_CL6",
  "Core_CH6",
  "Core_CL7",
  "Core_CH7",
  "Core_PC",
  "Core_FC",
  "Core_SP",
  "Core_LR",
  "Core_M0",
  "Core_M1",
  "Core_M2",
  "Core_M3",
  "Core_M4",
  "Core_M5",
  "Core_M6",
  "Core_M7",
  "Core_CB0",
  "Core_CB1",
  "Core_CB2",
  "Core_CB3",
  "Core_CB4",
  "Core_CB5",
  "Core_CB6",
  "Core_CB7",
  "Core_CS0",
  "Core_CS1",
  "Core_CS2",
  "Core_CS3",
  "Core_CS4",
  "Core_CS5",
  "Core_CS6",
  "Core_CS7",
  "Core_MD0",
  "Core_MD1",
  "Core_MC0",
  "Core_MC1",
  "Core_S0",
  "Core_S1",
  "Core_S2",
  "Core_S3",
  "Core_S4",
  "Core_S5",
  "Core_S6",
  "Core_S7",
  "Core_LS",
  "Core_LE",
  "Core_LC",
  "Performance_Ctrl0",
  "Performance_Ctrl1",
  "Performance_Ctrl2",
  "Performance_Counter0",
  "Performance_Counter1",
  "Performance_Counter2",
  "Performance_Counter3",
  "Performance_Counter0_Event_Value",
  "Performance_Counter1_Event_Value",
  "Performance_Counter2_Event_Value",
  "Performance_Counter3_Event_Value",
  "Core_Control",
  "Core_Status",
  "Enable_Events",
  "Reset_Event",
  "Debug_Control0",
  "Debug_Control1",
  "Debug_Control2",
  "Debug_Status",
  "PC_Event0",
  "PC_Event1",
  "PC_Event2",
  "PC_Event3",
  "Error_Halt_Control",
  "Error_Halt_Event",
  "ECC_Control",
  "ECC_Scrubbing_Event",
  "ECC_Failing_Address",
  "ECC_Instruction_Word_0",
  "ECC_Instruction_Word_1",
  "ECC_Instruction_Word_2",
  "ECC_Instruction_Word_3",
  "Timer_Control",
  "Event_Generate",
  "Event_Broadcast0",
  "Event_Broadcast1",
  "Event_Broadcast2",
  "Event_Broadcast3",
  "Event_Broadcast4",
  "Event_Broadcast5",
  "Event_Broadcast6",
  "Event_Broadcast7",
  "Event_Broadcast8",
  "Event_Broadcast9",
  "Event_Broadcast10",
  "Event_Broadcast11",
  "Event_Broadcast12",
  "Event_Broadcast13",
  "Event_Broadcast14",
  "Event_Broadcast15",
  "Event_Broadcast_Block_South_Set",
  "Event_Broadcast_Block_South_Clr",
  "Event_Broadcast_Block_South_Value",
  "Event_Broadcast_Block_West_Set",
  "Event_Broadcast_Block_West_Clr",
  "Event_Broadcast_Block_West_Value",
  "Event_Broadcast_Block_North_Set",
  "Event_Broadcast_Block_North_Clr",
  "Event_Broadcast_Block_North_Value",
  "Event_Broadcast_Block_East_Set",
  "Event_Broadcast_Block_East_Clr",
  "Event_Broadcast_Block_East_Value",
  "Trace_Control0",
  "Trace_Control1",
  "Trace_Status",
  "Trace_Event0",
  "Trace_Event1",
  "Timer_Trig_Event_Low_Value",
  "Timer_Trig_Event_High_Value",
  "Timer_Low",
  "Timer_High",
  "Event_Status0",
  "Event_Status1",
  "Event_Status2",
  "Event_Status3",
  "Combo_event_inputs",
  "Combo_event_control",
  "Event_Group_0_Enable",
  "Event_Group_PC_Enable",
  "Event_Group_Core_Stall_Enable",
  "Event_Group_Core_Program_Flow_Enable",
  "Event_Group_Errors0_Enable",
  "Event_Group_Errors1_Enable",
  "Event_Group_Stream_Switch_Enable",
  "Event_Group_Broadcast_Enable",
  "Event_Group_User_Event_Enable",
  "Tile_Control",
  "Tile_Control_Packet_Handler_Status",
  "Tile_Clock_Control",
  "CSSD_Trigger",
  "Spare_Reg",
  "Stream_Switch_Master_Config_ME_Core0",
  "Stream_Switch_Master_Config_ME_Core1",
  "Stream_Switch_Master_Config_DMA0",
  "Stream_Switch_Master_Config_DMA1",
  "Stream_Switch_Master_Config_Tile_Ctrl",
  "Stream_Switch_Master_Config_FIFO0",
  "Stream_Switch_Master_Config_FIFO1",
  "Stream_Switch_Master_Config_South0",
  "Stream_Switch_Master_Config_South1",
  "Stream_Switch_Master_Config_South2",
  "Stream_Switch_Master_Config_South3",
  "Stream_Switch_Master_Config_West0",
  "Stream_Switch_Master_Config_West1",
  "Stream_Switch_Master_Config_West2",
  "Stream_Switch_Master_Config_West3",
  "Stream_Switch_Master_Config_North0",
  "Stream_Switch_Master_Config_North1",
  "Stream_Switch_Master_Config_North2",
  "Stream_Switch_Master_Config_North3",
  "Stream_Switch_Master_Config_North4",
  "Stream_Switch_Master_Config_North5",
  "Stream_Switch_Master_Config_East0",
  "Stream_Switch_Master_Config_East1",
  "Stream_Switch_Master_Config_East2",
  "Stream_Switch_Master_Config_East3",
  "Stream_Switch_Slave_ME_Core0_Config",
  "Stream_Switch_Slave_ME_Core1_Config",
  "Stream_Switch_Slave_DMA_0_Config",
  "Stream_Switch_Slave_DMA_1_Config",
  "Stream_Switch_Slave_Tile_Ctrl_Config",
  "Stream_Switch_Slave_FIFO_0_Config",
  "Stream_Switch_Slave_FIFO_1_Config",
  "Stream_Switch_Slave_South_0_Config",
  "Stream_Switch_Slave_South_1_Config",
  "Stream_Switch_Slave_South_2_Config",
  "Stream_Switch_Slave_South_3_Config",
  "Stream_Switch_Slave_South_4_Config",
  "Stream_Switch_Slave_South_5_Config",
  "Stream_Switch_Slave_West_0_Config",
  "Stream_Switch_Slave_West_1_Config",
  "Stream_Switch_Slave_West_2_Config",
  "Stream_Switch_Slave_West_3_Config",
  "Stream_Switch_Slave_North_0_Config",
  "Stream_Switch_Slave_North_1_Config",
  "Stream_Switch_Slave_North_2_Config",
  "Stream_Switch_Slave_North_3_Config",
  "Stream_Switch_Slave_East_0_Config",
  "Stream_Switch_Slave_East_1_Config",
  "Stream_Switch_Slave_East_2_Config",
  "Stream_Switch_Slave_East_3_Config",
  "Stream_Switch_Slave_ME_Trace_Config",
  "Stream_Switch_Slave_Mem_Trace_Config",
  "Stream_Switch_Slave_ME_Core0_Slot0",
  "Stream_Switch_Slave_ME_Core0_Slot1",
  "Stream_Switch_Slave_ME_Core0_Slot2",
  "Stream_Switch_Slave_ME_Core0_Slot3",
  "Stream_Switch_Slave_ME_Core1_Slot0",
  "Stream_Switch_Slave_ME_Core1_Slot1",
  "Stream_Switch_Slave_ME_Core1_Slot2",
  "Stream_Switch_Slave_ME_Core1_Slot3",
  "Stream_Switch_Slave_DMA_0_Slot0",
  "Stream_Switch_Slave_DMA_0_Slot1",
  "Stream_Switch_Slave_DMA_0_Slot2",
  "Stream_Switch_Slave_DMA_0_Slot3",
  "Stream_Switch_Slave_DMA_1_Slot0",
  "Stream_Switch_Slave_DMA_1_Slot1",
  "Stream_Switch_Slave_DMA_1_Slot2",
  "Stream_Switch_Slave_DMA_1_Slot3",
  "Stream_Switch_Slave_Tile_Ctrl_Slot0",
  "Stream_Switch_Slave_Tile_Ctrl_Slot1",
  "Stream_Switch_Slave_Tile_Ctrl_Slot2",
  "Stream_Switch_Slave_Tile_Ctrl_Slot3",
  "Stream_Switch_Slave_FIFO_0_Slot0",
  "Stream_Switch_Slave_FIFO_0_Slot1",
  "Stream_Switch_Slave_FIFO_0_Slot2",
  "Stream_Switch_Slave_FIFO_0_Slot3",
  "Stream_Switch_Slave_FIFO_1_Slot0",
  "Stream_Switch_Slave_FIFO_1_Slot1",
  "Stream_Switch_Slave_FIFO_1_Slot2",
  "Stream_Switch_Slave_FIFO_1_Slot3",
  "Stream_Switch_Slave_South_0_Slot0",
  "Stream_Switch_Slave_South_0_Slot1",
  "Stream_Switch_Slave_South_0_Slot2",
  "Stream_Switch_Slave_South_0_Slot3",
  "Stream_Switch_Slave_South_1_Slot0",
  "Stream_Switch_Slave_South_1_Slot1",
  "Stream_Switch_Slave_South_1_Slot2",
  "Stream_Switch_Slave_South_1_Slot3",
  "Stream_Switch_Slave_South_2_Slot0",
  "Stream_Switch_Slave_South_2_Slot1",
  "Stream_Switch_Slave_South_2_Slot2",
  "Stream_Switch_Slave_South_2_Slot3",
  "Stream_Switch_Slave_South_3_Slot0",
  "Stream_Switch_Slave_South_3_Slot1",
  "Stream_Switch_Slave_South_3_Slot2",
  "Stream_Switch_Slave_South_3_Slot3",
  "Stream_Switch_Slave_South_4_Slot0",
  "Stream_Switch_Slave_South_4_Slot1",
  "Stream_Switch_Slave_South_4_Slot2",
  "Stream_Switch_Slave_South_4_Slot3",
  "Stream_Switch_Slave_South_5_Slot0",
  "Stream_Switch_Slave_South_5_Slot1",
  "Stream_Switch_Slave_South_5_Slot2",
  "Stream_Switch_Slave_South_5_Slot3",
  "Stream_Switch_Slave_West_0_Slot0",
  "Stream_Switch_Slave_West_0_Slot1",
  "Stream_Switch_Slave_West_0_Slot2",
  "Stream_Switch_Slave_West_0_Slot3",
  "Stream_Switch_Slave_West_1_Slot0",
  "Stream_Switch_Slave_West_1_Slot1",
  "Stream_Switch_Slave_West_1_Slot2",
  "Stream_Switch_Slave_West_1_Slot3",
  "Stream_Switch_Slave_West_2_Slot0",
  "Stream_Switch_Slave_West_2_Slot1",
  "Stream_Switch_Slave_West_2_Slot2",
  "Stream_Switch_Slave_West_2_Slot3",
  "Stream_Switch_Slave_West_3_Slot0",
  "Stream_Switch_Slave_West_3_Slot1",
  "Stream_Switch_Slave_West_3_Slot2",
  "Stream_Switch_Slave_West_3_Slot3",
  "Stream_Switch_Slave_North_0_Slot0",
  "Stream_Switch_Slave_North_0_Slot1",
  "Stream_Switch_Slave_North_0_Slot2",
  "Stream_Switch_Slave_North_0_Slot3",
  "Stream_Switch_Slave_North_1_Slot0",
  "Stream_Switch_Slave_North_1_Slot1",
  "Stream_Switch_Slave_North_1_Slot2",
  "Stream_Switch_Slave_North_1_Slot3",
  "Stream_Switch_Slave_North_2_Slot0",
  "Stream_Switch_Slave_North_2_Slot1",
  "Stream_Switch_Slave_North_2_Slot2",
  "Stream_Switch_Slave_North_2_Slot3",
  "Stream_Switch_Slave_North_3_Slot0",
  "Stream_Switch_Slave_North_3_Slot1",
  "Stream_Switch_Slave_North_3_Slot2",
  "Stream_Switch_Slave_North_3_Slot3",
  "Stream_Switch_Slave_East_0_Slot0",
  "Stream_Switch_Slave_East_0_Slot1",
  "Stream_Switch_Slave_East_0_Slot2",
  "Stream_Switch_Slave_East_0_Slot3",
  "Stream_Switch_Slave_East_1_Slot0",
  "Stream_Switch_Slave_East_1_Slot1",
  "Stream_Switch_Slave_East_1_Slot2",
  "Stream_Switch_Slave_East_1_Slot3",
  "Stream_Switch_Slave_East_2_Slot0",
  "Stream_Switch_Slave_East_2_Slot1",
  "Stream_Switch_Slave_East_2_Slot2",
  "Stream_Switch_Slave_East_2_Slot3",
  "Stream_Switch_Slave_East_3_Slot0",
  "Stream_Switch_Slave_East_3_Slot1",
  "Stream_Switch_Slave_East_3_Slot2",
  "Stream_Switch_Slave_East_3_Slot3",
  "Stream_Switch_Slave_ME_Trace_Slot0",
  "Stream_Switch_Slave_ME_Trace_Slot1",
  "Stream_Switch_Slave_ME_Trace_Slot2",
  "Stream_Switch_Slave_ME_Trace_Slot3",
  "Stream_Switch_Slave_Mem_Trace_Slot0",
  "Stream_Switch_Slave_Mem_Trace_Slot1",
  "Stream_Switch_Slave_Mem_Trace_Slot2",
  "Stream_Switch_Slave_Mem_Trace_Slot3",
  "Stream_Switch_Event_Port_Selection_0",
  "Stream_Switch_Event_Port_Selection_1"
};

OO_AieRegRead::OO_AieRegRead( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Read given aie register from given row and column" )
    , m_device("")
    , m_row(0)
    , m_col(0)
    , m_reg("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", po::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("row", po::value<decltype(m_row)>(&m_row)->required(), "Row of core tile")
    ("col", po::value<decltype(m_col)>(&m_col)->required(), "Column of core tile")
    ("reg", po::value<decltype(m_reg)>(&m_reg)->required(), "Register name to read from core tile")
    ("help", po::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_positionalOptions.
    add("row", 1 /* max_count */).
    add("col", 1 /* max_count */).
    add("reg", 1 /* max_count */)
  ;

  std::string extended_help = "Registers supported:";
  size_t size = regmap.size();
  for (size_t i = 0; i < size; i++)
    extended_help += boost::str(boost::format("\n  %s") % regmap[i].c_str());

  setExtendedHelp(extended_help);
}

void
OO_AieRegRead::execute(const SubCmdOptions& _options) const
{
  
  XBU::verbose("SubCommand option: aie_reg_read");

  XBU::verbose("Option(s):");
  for (auto & aString : _options)
    XBU::verbose(" " + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  // Exit if neither action or device specified
  if(m_help || m_device.empty()) {
    printHelp();
    return;
  }

  // Find device of interest
  std::shared_ptr<xrt_core::device> device;

  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  bool errorOccured = false;
  try {
    uint32_t val = xrt_core::device_query<qr::aie_reg_read>(device, m_row, m_col, m_reg);
    std::cout << boost::format("Register %s Value of Row:%d Column:%d is 0x%08x\n") % m_reg.c_str() % m_row %  m_col % val;
  } catch (const std::exception& e){
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    errorOccured = true;
  }

  if (errorOccured)
    throw xrt_core::error(std::errc::operation_canceled);

}

