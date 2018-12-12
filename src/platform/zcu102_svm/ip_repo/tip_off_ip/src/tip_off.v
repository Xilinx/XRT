// *************************************************************************
//    ____  ____
//   /   /\/   /
//  /___/  \  /
//  \   \   \/    © Copyright 2016-2017 Xilinx, Inc. All rights reserved.
//   \   \        This file contains confidential and proprietary
//   /   /        information of Xilinx, Inc. and is protected under U.S.
//  /___/   /\    and international copyright and other intellectual
//  \   \  /  \   property laws.
//   \___\/\___\
//
//
// *************************************************************************
//
// Disclaimer:
//
//       This disclaimer is not a license and does not grant any rights to
//       the materials distributed herewith. Except as otherwise provided in
//       a valid license issued to you by Xilinx, and to the maximum extent
//       permitted by applicable law: (1) THESE MATERIALS ARE MADE AVAILABLE
//       "AS IS" AND WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL
//       WARRANTIES AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY,
//       INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
//       NON-INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
//       (2) Xilinx shall not be liable (whether in contract or tort,
//       including negligence, or under any other theory of liability) for
//       any loss or damage of any kind or nature related to, arising under
//       or in connection with these materials, including for any direct, or
//       any indirect, special, incidental, or consequential loss or damage
//       (including loss of data, profits, goodwill, or any type of loss or
//       damage suffered as a result of any action brought by a third party)
//       even if such damage or loss was reasonably foreseeable or Xilinx
//       had been advised of the possibility of the same.
//
// Critical Applications:
//
//       Xilinx products are not designed or intended to be fail-safe, or
//       for use in any application requiring fail-safe performance, such as
//       life-support or safety devices or systems, Class III medical
//       devices, nuclear facilities, applications related to the deployment
//       of airbags, or any other applications that could lead to death,
//       personal injury, or severe property or environmental damage
//       (individually and collectively, "Critical Applications"). Customer
//       assumes the sole risk and liability of any use of Xilinx products
//       in Critical Applications, subject only to applicable laws and
//       regulations governing limitations on product liability.
//
// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS
// FILE AT ALL TIMES.
//
// *************************************************************************

`timescale 1ns / 1ps

module smmu_adapter # (
    parameter integer C_M_AXI_ADDR_WIDTH	  = 64,
    parameter integer C_M_AXI_DATA_WIDTH	  = 128
)(

input  wire                                      clk,
input  wire                                      aresetn,

// Ports of Axi Master Bus Interface M00_AXI
output wire [C_M_AXI_ADDR_WIDTH-1 : 0]           m_axi_awaddr,
output wire [7 : 0]                              m_axi_awlen,
output wire                                      m_axi_awvalid,
input  wire                                      m_axi_awready,
output wire [C_M_AXI_DATA_WIDTH-1 : 0]           m_axi_wdata,
output wire [C_M_AXI_DATA_WIDTH/8-1 : 0]         m_axi_wstrb,
output wire                                      m_axi_wlast,
output wire                                      m_axi_wvalid,
input  wire                                      m_axi_wready,
input  wire [1 : 0]                              m_axi_bresp,
input  wire                                      m_axi_bvalid,
output wire                                      m_axi_bready,
output wire [C_M_AXI_ADDR_WIDTH-1 : 0]           m_axi_araddr,
output wire [7 : 0]                              m_axi_arlen,
output wire                                      m_axi_arvalid,
input  wire                                      m_axi_arready,
input  wire [C_M_AXI_DATA_WIDTH-1 : 0]           m_axi_rdata,
input  wire [1 : 0]                              m_axi_rresp,
input  wire                                      m_axi_rlast,
input  wire                                      m_axi_rvalid,
output wire                                      m_axi_rready);

// Slave to Master signals
    assign m_axi_awaddr    = 'b0;
    assign m_axi_awlen     = 'b0;
    assign m_axi_awvalid   = 'b0;
    assign m_axi_wdata     = 'b0;
    assign m_axi_wstrb     = 'b0;
    assign m_axi_wlast     = 'b0;
    assign m_axi_wvalid    = 'b0;
    assign m_axi_bready    = 'b0;
    assign m_axi_araddr    = 'b0;
    assign m_axi_arlen     = 'b0;
    assign m_axi_arvalid   = 'b0;
    assign m_axi_rready    = 'b0;

endmodule


// 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689
