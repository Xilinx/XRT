// *************************************************************************
// //    ____  ____
// //   /   /\/   /
// //  /___/  \  /
// //  \   \   \/    ?? Copyright 2016-2017 Xilinx, Inc. All rights reserved.
// //   \   \        This file contains confidential and proprietary
// //   /   /        information of Xilinx, Inc. and is protected under U.S.
// //  /___/   /\    and international copyright and other intellectual
// //  \   \  /  \   property laws.
// //   \___\/\___\
// //
// //
// //
// *************************************************************************
// //
// // Disclaimer:
// //
// //       This disclaimer is not a license and does not grant any rights to
// //       the materials distributed herewith. Except as otherwise provided
// in
// //       a valid license issued to you by Xilinx, and to the maximum extent
// //       permitted by applicable law: (1) THESE MATERIALS ARE MADE
// AVAILABLE
// //       "AS IS" AND WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL
// //       WARRANTIES AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY,
// //       INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
// //       NON-INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// //       (2) Xilinx shall not be liable (whether in contract or tort,
// //       including negligence, or under any other theory of liability) for
// //       any loss or damage of any kind or nature related to, arising under
// //       or in connection with these materials, including for any direct,
// or
// //       any indirect, special, incidental, or consequential loss or damage
// //       (including loss of data, profits, goodwill, or any type of loss or
// //       damage suffered as a result of any action brought by a third
// party)
// //       even if such damage or loss was reasonably foreseeable or Xilinx
// //       had been advised of the possibility of the same.
// //
// // Critical Applications:
// //
// //       Xilinx products are not designed or intended to be fail-safe, or
// //       for use in any application requiring fail-safe performance, such
// as
// //       life-support or safety devices or systems, Class III medical
// //       devices, nuclear facilities, applications related to the
// deployment
// //       of airbags, or any other applications that could lead to death,
// //       personal injury, or severe property or environmental damage
// //       (individually and collectively, "Critical Applications"). Customer
// //       assumes the sole risk and liability of any use of Xilinx products
// //       in Critical Applications, subject only to applicable laws and
// //       regulations governing limitations on product liability.
// //
// // THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS
// // FILE AT ALL TIMES.
// //
// //
// *************************************************************************

`timescale 1ns / 1ps


module CuDmaController_rtl(
    input   wire                clk             ,
    input   wire                reset_n         ,
    
/*
 *  This bit is set in the top module and is corresponding to CuDmaQueueBusy
 *  Signal, this is set to indicate that a axilite write is in progress and no
 *  processing can be done.
 */
    input   wire                busy            ,
 
    input   wire                CuDmaEnable     ,
    input   wire    [31:0]      CuDmaQueue0     ,
    input   wire    [31:0]      CuDmaQueue1     ,
    input   wire    [31:0]      CuDmaQueue2     ,
    input   wire    [31:0]      CuDmaQueue3     ,
 
/* This bit is set in this module when  a snapshot of the CuDmaQueue is taken
 * for processing. When this bit is set the CuDmaQueue and CuDmaCount
 * registers in the top module are reset to 0
 */

    output  wire                clear           ,
    input   wire    [5:0]       CuDmaCount0     ,
    input   wire    [5:0]       CuDmaCount1     ,
    input   wire    [5:0]       CuDmaCount2     ,
    input   wire    [5:0]       CuDmaCount3     ,

    
    output  reg                 ap_start        =0,
    input   wire                ap_done,

/*
 * This is the concatenation of 4 32 bit CuDmaQueue resgiters which is
 * required by the CuDMA HLS IP.
 */

    output  reg     [127:0]     CqDmaQueue_reg=0, 
    
    
    output  wire    [3:0]       state0
    );
    
    reg         [3:0]   state = 1;
    localparam  [3:0]   IDLE                =4'b0001,
                        WAIT_FOR_COUNT      =4'b0010,
                        AP_START            =4'b0100,
                        AP_DONE             =4'b1000;
    
    
    always @(posedge clk)
    begin
        if(!reset_n)
            state <= IDLE;
        else
        begin
            case (state)
                IDLE : begin
                    if(CuDmaEnable)
                        state <= WAIT_FOR_COUNT;
                end 
                
                WAIT_FOR_COUNT : begin
                    if(!busy)
                    begin
                        if(CuDmaCount0 > 0 | CuDmaCount1 > 0 | CuDmaCount2 > 0 | CuDmaCount3 >0)
                        begin
                            CqDmaQueue_reg <= {CuDmaQueue3,CuDmaQueue2,CuDmaQueue1,CuDmaQueue0};    
                            state          <= AP_START;
                            ap_start       <= 1;
                        end
                    end 
                    else if (!CuDmaEnable)
                    begin
                        state <= IDLE;
                    end 
                end  
                
                AP_START : begin
                    ap_start    <= 0;
                    state       <= AP_DONE;
                end 
                
                AP_DONE : begin
                    if(ap_done)     
                        state <= IDLE;           
                end 
                             
                default : state <= IDLE;
            endcase
        end 
    end // end of always block
    
assign clear = (state[1] & !busy) ? 1 : 0;        
assign state0 = state;



  
endmodule
