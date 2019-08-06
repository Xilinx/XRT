// *************************************************************************
//    ____  ____
//   /   /\/   /
//  /___/  \  /
//  \   \   \/    ?? Copyright 2016-2017 Xilinx, Inc. All rights reserved.
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



module irq_handler(
    input wire          clk,
    input wire          reset_n,
    //input wire  [5:0]   CompletionCount,
    input wire  [31:0]  StatusRegister,
    //input wire  [7:0]   InterruptFrequency,
    //input wire  [7:0]   InterruptDelay,
    
    output reg          irq=0,
    input wire          irq_ack,
    input wire          GIE,
    //input wire  [9:0]   addr_offset,
    
    //input wire          rvalid,
    //input wire          rready,
    //input wire  [9:0]   araddr,
    output wire [2:0]   state
    );
    
//************** Interrupt Handler State Machine ****************************//
    reg         [2:0]   state_interrupt=1;
//(* dont_touch = "true" *)    reg         [7:0]   delay_count=0;
    localparam  [2:0]   IDLE_INTERRUPT      =3'b001,
                        WAITFORACK          =3'b010,
                        WAITFORREAD         =3'b100;
                        /*DEASSERT_INTERRUPT  =5'b01000,
                        DELAY               =5'b10000;*/
                        
    
    always @(posedge clk)
    begin
        if(!reset_n)
        begin
            state_interrupt <= IDLE_INTERRUPT;
        end 
        else
        begin
            case (state_interrupt)
                IDLE_INTERRUPT : begin
                  if(GIE)
                  begin
                    //if(CompletionCount >= InterruptFrequency)
                    if(StatusRegister > 0)
                    begin
                        state_interrupt <= WAITFORACK;
                        irq             <= 1;
                    end // end of if case
                  end
                end //end of IDLE_INTERRUPT
                               
                WAITFORACK : begin
                    if(irq_ack)
                        state_interrupt <= WAITFORREAD;
                end // end of WAITFORACK
                
                WAITFORREAD : begin
                    //if(rready && rvalid && (araddr[9:0] == addr_offset))
                    if(StatusRegister == 0)
		    begin
                        //state_interrupt <= DEASSERT_INTERRUPT;
                        state_interrupt <= IDLE_INTERRUPT;
                        irq             <= 0;
                    end // end of if case
                end // end of WAITFORREAD
                
               /* DEASSERT_INTERRUPT : begin
                    if(InterruptDelay > 0)
                        state_interrupt <= DELAY;
                    else
                        state_interrupt    <= IDLE_INTERRUPT;
                end // end of DEASSERT_INTERRUPT
                
                DELAY : begin
                    if(delay_count == InterruptDelay)
                    begin
                        delay_count <= 0;
                        state_interrupt <= IDLE_INTERRUPT;
                    end 
                    else
                    begin
                        delay_count <= delay_count + 1;
                    end // end of else
                end // DELAY*/
            endcase
        end 
    end // end of always block     
    
    assign state = state_interrupt;
    
     
endmodule
