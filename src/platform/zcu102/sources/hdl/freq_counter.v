// *************************************************************************
//    ____  ____
//   /   /\/   /
//  /___/  \  /
//  \   \   \/    ??? Copyright 2017 Xilinx, Inc. All rights reserved.
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

`timescale 1 ns / 1 ps

module freq_counter #
(
	parameter REF_CLK_FREQ_HZ=100000000
) (
    input   clk,
    input   reset_n,
    // PCIE M_AXI_LITE BUS PORTS
    input   wire    [31:0]      axil_awaddr     ,
    input   wire    [2:0]       axil_awprot     ,
    input   wire                axil_awvalid    ,
    output  reg                 axil_awready    =0,
    input   wire    [31:0]      axil_wdata      ,
    input   wire    [3:0]       axil_wstrb      ,
    input   wire                axil_wvalid     ,
    output  reg                 axil_wready     =0,
    output  reg                 axil_bvalid     =0,
    output  wire    [1:0]       axil_bresp      ,
    input   wire                axil_bready     ,
    
    input   wire    [31:0]      axil_araddr     ,
    input   wire    [2:0]       axil_arprot     ,
    input   wire                axil_arvalid    ,
    output  reg                 axil_arready    =0,
    output  reg     [31:0]      axil_rdata      =0,
    output  wire    [1:0]       axil_rresp      ,
    output  reg                 axil_rvalid     =0,
    input   wire                axil_rready     ,
    input test_clk0,
    input test_clk1
);

reg user_rst;
wire rst0_synced;
wire rst1_synced;
wire done;
wire done0_synced;
wire done1_synced;
wire user_rst1_ack; //Verify reset made it into other clock domain
wire user_rst_ack;
reg [1:0] clear_user_rst;
reg [31:0] ref_clk_cntr;
reg [31:0] test_clk0_cntr;
wire [31:0] test_clk0_cntr_synced; //Sync back to axi clock
reg [31:0] test_clk1_cntr;
wire [31:0] test_clk1_cntr_synced; //Sync back to axi clock

xpm_cdc_array_single #(

  //Common module parameters
  .DEST_SYNC_FF   (2), // integer; range: 2-10
  .SIM_ASSERT_CHK (0), // integer; 0=disable simulation messages, 1=enable simulation messages
  .SRC_INPUT_REG  (0), // integer; 0=do not register input, 1=register input
  .WIDTH          (1)  // integer; range: 1-1024

) xpm_cdc_array_single_inst (

  .src_clk  (clk),  // optional; required when SRC_INPUT_REG = 1
  .src_in   (~reset_n | user_rst),
  .dest_clk (test_clk0),
  .dest_out (rst0_synced)

);

xpm_cdc_array_single #(

  //Common module parameters
  .DEST_SYNC_FF   (2), // integer; range: 2-10
  .SIM_ASSERT_CHK (0), // integer; 0=disable simulation messages, 1=enable simulation messages
  .SRC_INPUT_REG  (0), // integer; 0=do not register input, 1=register input
  .WIDTH          (1)  // integer; range: 1-1024

) xpm_cdc_array_single_inst5 (

  .src_clk  (clk),  // optional; required when SRC_INPUT_REG = 1
  .src_in   (~reset_n | user_rst),
  .dest_clk (test_clk1),
  .dest_out (rst1_synced)

);

xpm_cdc_array_single #(

  //Common module parameters
  .DEST_SYNC_FF   (2), // integer; range: 2-10
  .SIM_ASSERT_CHK (0), // integer; 0=disable simulation messages, 1=enable simulation messages
  .SRC_INPUT_REG  (0), // integer; 0=do not register input, 1=register input
  .WIDTH          (1)  // integer; range: 1-1024

) xpm_cdc_array_single_inst1 (

  .src_clk  (test_clk0),  // optional; required when SRC_INPUT_REG = 1
  .src_in   (rst0_synced),
  .dest_clk (clk),
  .dest_out (user_rst0_ack)

);

xpm_cdc_array_single #(

  //Common module parameters
  .DEST_SYNC_FF   (2), // integer; range: 2-10
  .SIM_ASSERT_CHK (0), // integer; 0=disable simulation messages, 1=enable simulation messages
  .SRC_INPUT_REG  (0), // integer; 0=do not register input, 1=register input
  .WIDTH          (1)  // integer; range: 1-1024

) xpm_cdc_array_single_inst6 (

  .src_clk  (test_clk1),  // optional; required when SRC_INPUT_REG = 1
  .src_in   (rst1_synced),
  .dest_clk (clk),
  .dest_out (user_rst1_ack)

);

xpm_cdc_array_single #(

  //Common module parameters
  .DEST_SYNC_FF   (2), // integer; range: 2-10
  .SIM_ASSERT_CHK (0), // integer; 0=disable simulation messages, 1=enable simulation messages
  .SRC_INPUT_REG  (0), // integer; 0=do not register input, 1=register input
  .WIDTH          (1)  // integer; range: 1-1024

) xpm_cdc_array_single_inst2 (

  .src_clk  (clk),  // optional; required when SRC_INPUT_REG = 1
  .src_in   (done),
  .dest_clk (test_clk0),
  .dest_out (done0_synced)

);

xpm_cdc_array_single #(

  //Common module parameters
  .DEST_SYNC_FF   (2), // integer; range: 2-10
  .SIM_ASSERT_CHK (0), // integer; 0=disable simulation messages, 1=enable simulation messages
  .SRC_INPUT_REG  (0), // integer; 0=do not register input, 1=register input
  .WIDTH          (1)  // integer; range: 1-1024

) xpm_cdc_array_single_inst7 (

  .src_clk  (clk),  // optional; required when SRC_INPUT_REG = 1
  .src_in   (done),
  .dest_clk (test_clk1),
  .dest_out (done1_synced)

);

xpm_cdc_array_single #(

  //Common module parameters
  .DEST_SYNC_FF   (2), // integer; range: 2-10
  .SIM_ASSERT_CHK (0), // integer; 0=disable simulation messages, 1=enable simulation messages
  .SRC_INPUT_REG  (0), // integer; 0=do not register input, 1=register input
  .WIDTH          (32)  // integer; range: 1-1024

) xpm_cdc_array_single_inst3 (

  .src_clk  (test_clk0),  // optional; required when SRC_INPUT_REG = 1
  .src_in   (test_clk0_cntr),
  .dest_clk (clk),
  .dest_out (test_clk0_cntr_synced)

);

xpm_cdc_array_single #(

  //Common module parameters
  .DEST_SYNC_FF   (2), // integer; range: 2-10
  .SIM_ASSERT_CHK (0), // integer; 0=disable simulation messages, 1=enable simulation messages
  .SRC_INPUT_REG  (0), // integer; 0=do not register input, 1=register input
  .WIDTH          (32)  // integer; range: 1-1024

) xpm_cdc_array_single_inst4 (

  .src_clk  (test_clk1),  // optional; required when SRC_INPUT_REG = 1
  .src_in   (test_clk1_cntr),
  .dest_clk (clk),
  .dest_out (test_clk1_cntr_synced)

);

assign user_rst_ack = user_rst0_ack && user_rst1_ack;

always @(posedge clk) begin
    clear_user_rst[0] <= user_rst_ack;
    clear_user_rst[1] <= clear_user_rst[0];
end



//*****************************************************************************//
// State Machine for HOST Write Request
reg [2:0]			state_write 			=	1;
localparam [2:0] 	IDLE_WRITE				=	3'b001,
					WAIT_FOR_WVALID_WRITE	=	3'b010,
					WAIT_FOR_BREADY_WRITE	=	3'b100;

always @(posedge clk)
begin
	if(!reset_n)
		state_write <= IDLE_WRITE;
	else 
	begin
		case (state_write)
		IDLE_WRITE : 
		begin
			if(axil_awvalid)
			begin
				state_write		<= WAIT_FOR_WVALID_WRITE;	
				axil_awready 	<= 1;	
			end 
		end // end of  IDLE_WRITE
		
		WAIT_FOR_WVALID_WRITE :
		begin
			axil_awready		<= 0;
			if(axil_wvalid)
			begin
				state_write		<= WAIT_FOR_BREADY_WRITE;
				axil_wready 	<= 1;
				axil_bvalid		<= 1;
			end 
		end // end of WAIT_FOR_WVALID_WRITE
		
		WAIT_FOR_BREADY_WRITE :
		begin
		    axil_wready 	    <= 0;
			if(axil_bready)
			begin
				axil_bvalid		<= 0;
				state_write		<= IDLE_WRITE;
			end 
		end 
		endcase
	end // end of else block 
end 


//****************************************************************************//
// State Machine for HOST READ Request
reg [1:0]			state_read				=	1;				
localparam	[1:0]	IDLE_READ				=	2'b01,
					WAIT_FOR_RVALID_READ	=	2'b10;

always @(posedge clk)
begin
	if(!reset_n)
		state_read <= IDLE_READ;
	else 
	begin
		case (state_read)
		IDLE_READ : 
		begin
			if(axil_arvalid)
			begin
				state_read		<= WAIT_FOR_RVALID_READ;	
				axil_arready 	<= 1;
				axil_rvalid		<= 1;
			end 
		end // end of IDLE_READ
		
		WAIT_FOR_RVALID_READ :
		begin
			axil_arready		<= 0;
			if(axil_rready)
			begin
				state_read		<= IDLE_READ;
				axil_rvalid 	<= 0;
			end 
		end // end of WAIT_FOR_RVALID_READ
		endcase
	end // end of else block 
end // end of always block 


//****************************************************************************//
always @(posedge clk)
begin
    if(axil_arvalid)
    begin
        case (axil_araddr[3:0])
        4'h0:begin
                    if(axil_wvalid && (axil_awaddr[3:0] == 4'h0))
                        axil_rdata <= (done<<1) | (axil_wdata&32'hFFFFFFFD);
                    else
                        axil_rdata <= {30'b0, done, user_rst};
                end   
        
        4'h4 : axil_rdata <= ref_clk_cntr;
        4'h8 : axil_rdata <= test_clk0_cntr_synced;
        4'hC : axil_rdata <= test_clk1_cntr_synced;
        
        default: axil_rdata <= 32'hDEADBEEF;                
        endcase
    end
end

//***************************************************************************//
// AXI LITE WRITE DATA

always @(posedge clk)
begin
    if(axil_wvalid && axil_wready && axil_awaddr[3:0] == 4'h0) begin
        user_rst  <= axil_wdata[0];
    end
    else begin
        if(clear_user_rst[1]) user_rst <= 1'b0; //Self clear bit after 2 cycles
    end
end //end of always block 

assign axil_bresp = 0;
assign axil_rresp = 0; 

assign done = ref_clk_cntr == REF_CLK_FREQ_HZ;

//Ref clock counter logic
always @(posedge clk) begin
	if(!reset_n | user_rst) begin
		ref_clk_cntr <= 32'b0;
	end
	else begin
		if(~done) ref_clk_cntr <= ref_clk_cntr+1;
	end
end

//Test clock counter logic
always @(posedge test_clk0) begin
	if(rst0_synced) begin
		test_clk0_cntr <= 32'b0;
	end
	else begin
		if(~done0_synced) test_clk0_cntr <= test_clk0_cntr+1;
	end
end

//Test clock counter logic
always @(posedge test_clk1) begin
	if(rst1_synced) begin
		test_clk1_cntr <= 32'b0;
	end
	else begin
		if(~done1_synced) test_clk1_cntr <= test_clk1_cntr+1;
	end
end

endmodule
