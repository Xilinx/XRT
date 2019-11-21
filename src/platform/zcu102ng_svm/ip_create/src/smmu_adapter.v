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


`timescale 1ns / 1ps

module smmu_adapter # (
    parameter integer C_S_AXI_ADDR_WIDTH	  = 64,
    parameter integer C_S_AXI_DATA_WIDTH	  = 128,
    parameter integer C_S_AXI_ID_WIDTH	      = 6,
    parameter integer C_S_AXI_AWUSER_WIDTH    = 1,
    parameter integer C_S_AXI_ARUSER_WIDTH    = 1,
    parameter integer C_S_AXI_WUSER_WIDTH	  = 1,
    parameter integer C_S_AXI_RUSER_WIDTH	  = 1,
    parameter integer C_S_AXI_BUSER_WIDTH	  = 1,
    parameter integer C_M_AXI_ADDR_WIDTH	  = 64,
    parameter integer C_M_AXI_DATA_WIDTH	  = 128,
    parameter integer C_M_AXI_ID_WIDTH	      = 6,
    parameter integer C_M_AXI_AWUSER_WIDTH    = 1,
    parameter integer C_M_AXI_ARUSER_WIDTH    = 1,
    parameter integer C_M_AXI_WUSER_WIDTH	  = 1,
    parameter integer C_M_AXI_RUSER_WIDTH	  = 1,
    parameter integer C_M_AXI_BUSER_WIDTH	  = 1
)(

input  wire                                      clk,
input  wire                                      aresetn,

// Ports of AXI Slave Bus Interface S00_AXI
input  wire [C_S_AXI_ID_WIDTH-1 : 0]             s_axi_awid,
input  wire [C_S_AXI_ADDR_WIDTH-1 : 0]           s_axi_awaddr,
input  wire [7 : 0]                              s_axi_awlen,
input  wire [2 : 0]                              s_axi_awsize,
input  wire [1 : 0]                              s_axi_awburst,
input  wire                                      s_axi_awlock,
input  wire [3 : 0]                              s_axi_awcache,
input  wire [2 : 0]                              s_axi_awprot,
input  wire [3 : 0]                              s_axi_awqos,
input  wire [3 : 0]                              s_axi_awregion,
input  wire [C_S_AXI_AWUSER_WIDTH-1 : 0]         s_axi_awuser,
input  wire                                      s_axi_awvalid,
output wire                                      s_axi_awready,
input  wire [C_S_AXI_ID_WIDTH-1 : 0]             s_axi_wid,
input  wire [C_S_AXI_DATA_WIDTH-1 : 0]           s_axi_wdata,
input  wire [(C_S_AXI_DATA_WIDTH/8)-1 : 0]       s_axi_wstrb,
input  wire                                      s_axi_wlast,
input  wire [C_S_AXI_WUSER_WIDTH-1 : 0]          s_axi_wuser,
input  wire                                      s_axi_wvalid,
output wire                                      s_axi_wready,
output wire [C_S_AXI_ID_WIDTH-1 : 0]             s_axi_bid,
output wire [1 : 0]                              s_axi_bresp,
output wire [C_S_AXI_BUSER_WIDTH-1 : 0]          s_axi_buser,
output wire                                      s_axi_bvalid,
input  wire                                      s_axi_bready,
input  wire [C_S_AXI_ID_WIDTH-1 : 0]             s_axi_arid,
input  wire [C_S_AXI_ADDR_WIDTH-1 : 0]           s_axi_araddr,
input  wire [7 : 0]                              s_axi_arlen,
input  wire [2 : 0]                              s_axi_arsize,
input  wire [1 : 0]                              s_axi_arburst,
input  wire                                      s_axi_arlock,
input  wire [3 : 0]                              s_axi_arcache,
input  wire [2 : 0]                              s_axi_arprot,
input  wire [3 : 0]                              s_axi_arqos,
input  wire [3 : 0]                              s_axi_arregion,
input  wire [C_S_AXI_ARUSER_WIDTH-1 : 0]         s_axi_aruser,
input  wire                                      s_axi_arvalid,
output wire                                      s_axi_arready,
output wire [C_S_AXI_ID_WIDTH-1 : 0]             s_axi_rid,
output wire [C_S_AXI_DATA_WIDTH-1 : 0]           s_axi_rdata,
output wire [1 : 0]                              s_axi_rresp,
output wire                                      s_axi_rlast,
output wire [C_S_AXI_RUSER_WIDTH-1 : 0]          s_axi_ruser,
output wire                                      s_axi_rvalid,
input  wire                                      s_axi_rready,

// Ports of Axi Master Bus Interface M00_AXI
output wire [C_M_AXI_ID_WIDTH-1 : 0]             m_axi_awid,
output wire [C_M_AXI_ADDR_WIDTH-1 : 0]           m_axi_awaddr,
output wire [7 : 0]                              m_axi_awlen,
output wire [2 : 0]                              m_axi_awsize,
output wire [1 : 0]                              m_axi_awburst,
output wire                                      m_axi_awlock,
output wire [3 : 0]                              m_axi_awcache,
output wire [2 : 0]                              m_axi_awprot,
output wire [3 : 0]                              m_axi_awqos,
output wire [3 : 0]                              m_axi_awregion,
output wire [C_M_AXI_AWUSER_WIDTH-1 : 0]         m_axi_awuser,
output wire                                      m_axi_awvalid,
input  wire                                      m_axi_awready,
output wire [C_M_AXI_ID_WIDTH-1 : 0]             m_axi_wid,
output wire [C_M_AXI_DATA_WIDTH-1 : 0]           m_axi_wdata,
output wire [C_M_AXI_DATA_WIDTH/8-1 : 0]         m_axi_wstrb,
output wire                                      m_axi_wlast,
output wire [C_M_AXI_WUSER_WIDTH-1 : 0]          m_axi_wuser,
output wire                                      m_axi_wvalid,
input  wire                                      m_axi_wready,
input  wire [C_M_AXI_ID_WIDTH-1 : 0]             m_axi_bid,
input  wire [1 : 0]                              m_axi_bresp,
input  wire [C_M_AXI_BUSER_WIDTH-1 : 0]          m_axi_buser,
input  wire                                      m_axi_bvalid,
output wire                                      m_axi_bready,
output wire [C_M_AXI_ID_WIDTH-1 : 0]             m_axi_arid,
output wire [C_M_AXI_ADDR_WIDTH-1 : 0]           m_axi_araddr,
output wire [7 : 0]                              m_axi_arlen,
output wire [2 : 0]                              m_axi_arsize,
output wire [1 : 0]                              m_axi_arburst,
output wire                                      m_axi_arlock,
output wire [3 : 0]                              m_axi_arcache,
output wire [2 : 0]                              m_axi_arprot,
output wire [3 : 0]                              m_axi_arqos,
output wire [3 : 0]                              m_axi_arregion,
output wire [C_M_AXI_ARUSER_WIDTH-1 : 0]         m_axi_aruser,
output wire                                      m_axi_arvalid,
input  wire                                      m_axi_arready,
input  wire [C_M_AXI_ID_WIDTH-1 : 0]             m_axi_rid,
input  wire [C_M_AXI_DATA_WIDTH-1 : 0]           m_axi_rdata,
input  wire [1 : 0]                              m_axi_rresp,
input  wire                                      m_axi_rlast,
input  wire [C_M_AXI_RUSER_WIDTH-1 : 0]          m_axi_ruser,
input  wire                                      m_axi_rvalid,
output wire                                      m_axi_rready);

// Slave to Master signals
    assign m_axi_awaddr    = s_axi_awaddr;
    assign m_axi_awlen     = s_axi_awlen;
    assign m_axi_awsize    = s_axi_awsize;
    assign m_axi_awburst   = s_axi_awburst;
    assign m_axi_awlock    = s_axi_awlock;
    assign m_axi_awqos     = s_axi_awqos;
    assign m_axi_awregion  = s_axi_awregion;
    assign m_axi_awuser    = s_axi_awuser;
    assign m_axi_awvalid   = s_axi_awvalid;
    assign m_axi_wdata     = s_axi_wdata;
    assign m_axi_wstrb     = s_axi_wstrb;
    assign m_axi_wlast     = s_axi_wlast;
    assign m_axi_wuser     = s_axi_wuser;
    assign m_axi_wvalid    = s_axi_wvalid;
    assign m_axi_bready    = s_axi_bready;
    assign m_axi_araddr    = s_axi_araddr;
    assign m_axi_arlen     = s_axi_arlen;
    assign m_axi_arsize    = s_axi_arsize;
    assign m_axi_arburst   = s_axi_arburst;
    assign m_axi_arlock    = s_axi_arlock;
    assign m_axi_arqos     = s_axi_arqos;
    assign m_axi_arregion  = s_axi_arregion;
    assign m_axi_aruser    = s_axi_aruser;
    assign m_axi_arvalid   = s_axi_arvalid;
    assign m_axi_rready    = s_axi_rready;

// Master to Slave signals
    assign s_axi_awready   = m_axi_awready;
    assign s_axi_wready    = m_axi_wready;
    assign s_axi_bresp     = m_axi_bresp;
    assign s_axi_buser     = m_axi_buser;
    assign s_axi_bvalid    = m_axi_bvalid;
    assign s_axi_arready   = m_axi_arready;
    assign s_axi_rdata     = m_axi_rdata;
    assign s_axi_rresp     = m_axi_rresp;
    assign s_axi_rlast     = m_axi_rlast;
    assign s_axi_ruser     = m_axi_ruser;
    assign s_axi_rvalid    = m_axi_rvalid;

    assign m_axi_awid      = 6'b0;
    assign m_axi_wid       = 6'b0;
    assign m_axi_arid      = 6'b0;
    assign s_axi_bid       = s_axi_awid;
    assign s_axi_rid       = s_axi_arid;
    assign m_axi_awprot    = 3'b010;
    assign m_axi_arprot    = 3'b010;
    assign m_axi_awcache   = 4'b1011;
    assign m_axi_arcache   = 4'b1011;

endmodule
