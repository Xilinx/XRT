// Copyright (C) 2016-2017 Xilinx, Inc

// Licensed under the Apache License, Version 2.0 (the "License"). You may
// not use this file except in compliance with the License. A copy of the
// License is located at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.

// Copyright 2017 Xilinx, Inc. All rights reserved.


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
