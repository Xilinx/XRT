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

module embedded_scheduler_hw(

/*
 * clk port is connected to the 250 Mhz PCIe ClK
 */
input   clk							, 
/*
 * This port is connected to the pcie axi_resent_n port from xDMA IP
 */
input   reset_n							,


/* Axilite Bus for Read and Write Access to Register MAP, CSR ,
 * Status Resgister , IRQ etc.
 * This bus is mastered by Embedded Microblaze and CPU Host
 */
input   wire    [31:0]      host_awaddr     			,
input   wire    [2:0]       host_awprot     			,
input   wire                host_awvalid    			,
output  reg                 host_awready    	=0		,
input   wire    [31:0]      host_wdata      			,
input   wire    [3:0]       host_wstrb      			,
input   wire                host_wvalid     			,
output  reg                 host_wready     	=0		,
output  reg                 host_bvalid     	=0		,
output  wire    [1:0]       host_bresp      			,
input   wire                host_bready     			,

input   wire    [31:0]      host_araddr     			,
input   wire    [2:0]       host_arprot     			,
input   wire                host_arvalid    			,
output  reg                 host_arready    	=0		,
output  reg     [31:0]      host_rdata      	=0		,
output  wire    [1:0]       host_rresp      			,
output  reg                 host_rvalid     	=0		,
input   wire                host_rready     			,

/*
 * Control Ports and Arguments for the CUISR HLS IP
 * */
output                      ap_start_cuisr			,
input                       ap_done_cuisr 			,
output          [31:0]      Offset        			,

/*
 * Arguments which are set by the Embedded Runtime Firmware
 * These must be set based on the xclbin configuration at the 
 * begining before processing any workloads.
 * These arguments are set using the configure command packet
 * sent by the host. These arguments are also passed to the 
 * CUDMA IP
 */
output  reg     [12:0]      SlotSize            =13'h1000	,   
output  reg     [5:0]       CuOffset            =6'hC		, 
output  reg     [7:0]       NoofSlots           =8'd16		, 
output  reg     [31:0]      CuBaseAddress       =32'h0		,
output  reg     [31:0]      CqBaseAddress       =32'h150000	,

/*
 * Control ports and Arguments for the CUDMA HLS IP
 */
output  wire                ap_start_cudma			,
input   wire                ap_done_cudma			,
output  wire    [127:0]     CqDmaQueue_reg			,

/*
 * FPGA to Host Interrupt lines which aare wired to the xDMA 
 * usr_irq ports [0:3]. These interrupts get set when atleast one 
 * command in the command queue is completed by the compute units
 */
output                      irq_sr0         			,
output                      irq_sr1         			,
output                      irq_sr2         			,
output                      irq_sr3         			,

/*
 * This is the acknowledgement bits for all the 16 usr interrupts 
 * available on the xDMA IP.
 */
input	wire    [15:0]	    irq_ack         			,

/*
 * These are the possible 128 irq lines from the dynamic region
 * for possible 128 Compute units which may be present in the 
 * dynamic region. These irq's indicate completion of a task by 
 * the compute units.
 */
input   wire    [127:0]     irq_cu          			,

/*
 * This is the irq port which is connected to the Embedded MIcroblaze
 * this indicates the completion of a task by a compute unit for a
 * given command queue slot
 */
output  wire                irq_cu_completion			,

/*
 * This is the irq port is used to inform the Embedded MIcroblaze
 * firware whenever a new slot is available in the command Queue.
 */
output  wire                irq_slotavailable

);


/***************** ADDRESS MAP *******************

 *
 * 4 Status Registers with 32 bits each, each bit indicates completion
 * status of each slot in the command Queue. The command queue can have
 * a maximum of 128 Slots.When the host receives an interrupt via irq_sr*
 * then the host performs a read to one of these status registers based on 
 * the number of available CQ slots and then maps the completions bits with 
 * the associated slots ID's.These are clear on read registers which get 
 * cleared whenever a axilite read is performed in this address space by
 * either the host or the Embedded Microblaze.
 *

0x0     -> StatusRegister [31:0]        -> W/COR
0x4     -> StatusRegister [32:63]       -> W/COR
0x8     -> StatusRegister [64:95]       -> W/COR
0xC     -> StatusRegister [96:127]      -> W/COR

 *
 * These CSR registers are associated with the CUDMA HLS IP. The CuDmaEnable
 * signal must be set to enabled the CUDMA functionality in hardware. There
 * are 4 CuDmaQueue registers which are set my the Embedded MIcroblaze
 * firmware which indicate to the CuDMA controller logic which CQ Slots to be
 * processed 
0x18    -> CuDmaEnable                  -> R/W
0x1C    -> CuDmaQueue [31:0]            -> W Only
0x20    -> CuDmaQueue [32:63]           -> W only
0x24    -> CuDmaQueue [64:95]           -> W only
0x28    -> CuDmaQueue [96:127]          -> w only

 * 
 * These register are configured using the config CQ slot packaet based on the
 * information which is available in the xclbin. These register are provided
 * as input arguments to the CUDMA HLS IP.
 *

0x2C    -> SlotSize                     -> R/W
0x30    -> CuOffset                     -> R/W
0x34    -> NoofSlots                    -> R/W
0x38    -> CuBaseAddress                -> R/W
0x3C    -> CqBaseAddress                -> R/W

 *
 * These CSR registers are associated with the CUISR HLS IP. The CuIsrHandlerEnable
 * signal must be set to enable the CuISR functionality in hardware.The
 * cu_inc_status* register show the the competed tasks by each compute unit.
 * These registers are read by the Embedded runtime when irq_cu_completion
 * interrupt is assert to the ERT. The bits set in this register are then
 * mapped to the corresponding Command Queue Slot, book keeping is done by the
 * ERT firmware
 
0x40    -> CuIsrHandlerEnable           -> R/W
0x44    -> cu_inc_status0               -> COR
0x48    -> cu_inc_status1               -> COR
0x4C    -> cu_inc_status2               -> COR
0x50    -> cu_inc_status3               -> COR


 *
 * These CSR registers are associated with the Command Queue Slot Interrupts
 * to ERT. When a command is written to the command Queue, a corresponding bit
 * is set in the CqSlotQueue register by the host. There are possible 128 CQ
 * slots and hene there are 4 32 bit registers. A write to any of these
 * registers results in assertion of irq_slotavailable interrupt line, which
 * then triggers ERT to read the CqSlotQueue register to check which slot was
 * populated. These registers are COR and will be cleared once ERT performs
 * a read operation.
 *

0x54    -> CqSlotQueueEnable            -> R/W
0x58    -> CqSlotQueue [31:0]           -> W/COR
0x5C    -> CqSlotQueue [32:63]          -> W/COR
0x60    -> CqSlotQueue [64:95]          -> W/COR
0x64    -> CqSlotQueue [96:127]         -> W/COR


 *
 * This CSR register indicates the total no of compute units which are
 * available for use. This number is dependent on the xclbin which is
 * configured on the board. This is configured as part of the config packet
 * which sent by the host.
 *
0x68    -> no_of_computeunits           -> R/W

/*
 * This CSR register enabled the Interrupts from  FPGA to HOST for indicating
 * the completion of command slots.
 *
0x100   -> GIE                          -> R/W

*
 * These are DEBUG registers which hold the current state of all the state
 * machine which are running in the system
 *

 *
 *Axilite write and Read state machince status
 *

0x300   -> DEBUG_StatusRegister_State_Write             -> R Only
0x304   -> DEBUG_StatusRegister_State_Read              -> R Only

 *
 * State machine of the interrupt handler logic for FPGA to HOst interrupts
 *
0x308   -> DEBUG_StatusRegister_IrqHandler_State0       -> R Only
0x30C   -> DEBUG_StatusRegister_IrqHandler_State1       -> R Only
0x310   -> DEBUG_StatusRegister_IrqHandler_State2       -> R Only
0x314   -> DEBUG_StatusRegister_IrqHandler_State3       -> R Only

 *
 * CU Dma controller status
 *
0x318   -> DEBUG_CuDmaController_State0                 -> R Only
0x31C   -> DEBUG_CuDmaController_StateDmaLoop           -> R Only
0x320   -> DEBUG_CuDmaController_Statecqcopy            -> R Only
0x324   -> DEBUG_CuDmaController_Statecucopy            -> R Only

 *
 * CU ISR Status
 *

0x328   -> DEBUG_CuISR_State0                           -> R Only
0x32C   -> DEBUG_CuISR_State_isr_read                   -> R Only   
0x330   -> DEBUG_CuISR_State_isr_write                  -> R Only   
 
 

***************************************************/

// AXI LITE Register Declarations 
reg     [31:0]  StatusRegister0     =0  ;
reg     [31:0]  StatusRegister1     =0  ;
reg     [31:0]  StatusRegister2     =0  ;
reg     [31:0]  StatusRegister3     =0  ;
reg             CuDmaEnable         =0  ;
reg     [31:0]  CuDmaQueue0         =0  ;
reg     [31:0]  CuDmaQueue1         =0  ;
reg     [31:0]  CuDmaQueue2         =0  ;
reg     [31:0]  CuDmaQueue3         =0  ;
reg             CuIsrHandlerEnable  =0  ;
wire    [31:0]  cu_inc_status0          ;
wire    [31:0]  cu_inc_status1          ;
wire    [31:0]  cu_inc_status2          ;
wire    [31:0]  cu_inc_status3          ;
reg             CqSlotQueueEnable   =0  ;
reg     [31:0]  CqSlotQueue0        =0  ;
reg     [31:0]  CqSlotQueue1        =0  ;
reg     [31:0]  CqSlotQueue2        =0  ;
reg     [31:0]  CqSlotQueue3        =0  ;
(* dont_touch = "true" *)reg     [5:0]   CqSlotQueueCount0   =0  ;
(* dont_touch = "true" *)reg     [5:0]   CqSlotQueueCount1   =0  ;
(* dont_touch = "true" *)reg     [5:0]   CqSlotQueueCount2   =0  ;
(* dont_touch = "true" *)reg     [5:0]   CqSlotQueueCount3   =0  ;
reg             GIE                 =0  ;
reg     [7:0]   no_of_computeunits  =1  ;

// DEBUG SIGNALS 
wire    [2:0]   DEBUG_StatusRegister_State_Write;
wire    [1:0]   DEBUG_StatusRegister_State_Read;
wire    [2:0]   DEBUG_StatusRegister_IrqHandler_State0;
wire    [2:0]   DEBUG_StatusRegister_IrqHandler_State1;
wire    [2:0]   DEBUG_StatusRegister_IrqHandler_State2;
wire    [2:0]   DEBUG_StatusRegister_IrqHandler_State3;
wire    [3:0]   DEBUG_CuDmaController_State0;
wire    [8:0]   DEBUG_CuISR_State0; 



 /**
 * These Registers are used by the Cu DMA Controller logic
 * CuDmaQueueBusy signal indicates that an active axilite write 
 * by the ERT is under progress for the CuDmaQueue register  and 
 * the CuDMA controller should wait before taking a CuDmaQueue snapshot.
 * CuDmaCount is incremented everytime ERT sets a bit in the CuDmaQueue
 * when a snapshot of the CuDmaQueue is taken by the CuDmacontroller for
 * processing , this count value as well as the CuDmQueue contents are
 * reset to 0 by setting the Clear_CuQueue to high.
 **/


reg             			 CuDmaQueueBusy      =0  ;
wire            			 clear_CuQueue           ;
(* dont_touch = "true" *)reg     [5:0]   CuDmaCount0         =0  ;
(* dont_touch = "true" *)reg     [5:0]   CuDmaCount1         =0  ;
(* dont_touch = "true" *)reg     [5:0]   CuDmaCount2         =0  ;
(* dont_touch = "true" *)reg     [5:0]   CuDmaCount3         =0  ;


//*****************************************************************************//

/*
 *  This is CDC path between in the irq's from the compute unit clock domain
 *  to the PCIe Axi clock domain , which is the clock frequency at which the
 *  ERT RTL controller logic works.
 *  This is a standard Xilinx Primitive for clock crossing.
 */
 
wire [127:0] irq_cu_cdc;

xpm_cdc_array_single #(

  //Common module parameters
  .DEST_SYNC_FF   (2), // integer; range: 2-10
  .SIM_ASSERT_CHK (0), // integer; 0=disable simulation messages, 1=enable simulation messages
  .SRC_INPUT_REG  (0), // integer; 0=do not register input, 1=register input
  .WIDTH          (128)  // integer; range: 1-1024

) xpm_cdc_array_single_inst (

  .src_clk  (src_clk),  // optional; required when SRC_INPUT_REG = 1
  .src_in   (irq_cu),
  .dest_clk (clk),
  .dest_out (irq_cu_cdc)

);


//*****************************************************************************//
/*
 * This state machine controls the write handshake signals of the axilite bus 
 * which is used to read and write to the register space within this module
 */
reg        [2:0] state_write 		=1;
localparam [2:0] IDLE_WRITE		=3'b001,
		 WAIT_FOR_WVALID_WRITE	=3'b010,
		 WAIT_FOR_BREADY_WRITE	=3'b100;

always @(posedge clk)
begin
	if(!reset_n)
		state_write <= IDLE_WRITE;
	else 
	begin
		case (state_write)
		IDLE_WRITE : 
		begin
		   if(host_awvalid)
		   begin
		      state_write	<= WAIT_FOR_WVALID_WRITE;	
 		      host_awready 	<= 1;	
		   end 
		end // end of  IDLE_WRITE
		
		WAIT_FOR_WVALID_WRITE :
		begin
		   host_awready		<= 0;
		   if(host_wvalid)
		   begin
		      state_write	<= WAIT_FOR_BREADY_WRITE;
		      host_wready 	<= 1;
		      host_bvalid	<= 1;
		   end 
		end // end of WAIT_FOR_WVALID_WRITE
		
		WAIT_FOR_BREADY_WRITE :
		begin
		   host_wready 	    <= 0;
		   if(host_bready)
		      begin
		         host_bvalid		<= 0;
			 state_write		<= IDLE_WRITE;
		      end 
		end 
		endcase
	end // end of else block 
end 


//****************************************************************************//
/*
 * This state machine controls the read handshake signals of the axilite bus 
 * which is used to read and write to the register space witin this module.
 */


reg 		[1:0]	state_read		=1;				
localparam	[1:0]	IDLE_READ		=2'b01,
			WAIT_FOR_RVALID_READ	=2'b10;

always @(posedge clk)
begin
	if(!reset_n)
		state_read <= IDLE_READ;
	else 
	begin
	   case (state_read)
	      IDLE_READ : 
	         begin
		    if(host_arvalid)
		    begin
		       state_read	<= WAIT_FOR_RVALID_READ;	
		       host_arready 	<= 1;
		       host_rvalid	<= 1;
		    end 
		 end // end of IDLE_READ
		
	       WAIT_FOR_RVALID_READ :
	          begin
		     host_arready       <= 0;
		     if(host_rready)
		     begin
		        state_read	<= IDLE_READ;
			host_rvalid 	<= 0;
		     end 
		  end // end of WAIT_FOR_RVALID_READ
	    endcase
	end // end of else block 
end // end of always block 


//****************************************************************************//
/*
 * This always block defines the logic to map the correct read data to the rdata
 * port of the axilite bus based on the address request
 */


always @(posedge clk)
begin
    if(host_arvalid)
    begin
        case (host_araddr[9:0])
/*
 * Since the Status Register is a clear on read register,for the worst case
 * senario when a write and read on this register happens at the same time,to
 * avoid any loss of data the first if condition is implemented where the
 * write_data is ored with the current Status register value and assigned to
 * the host_rdata
 */ 	
        10'h0:begin 
            if(host_wvalid && (host_awaddr[9:0] == 10'h0))
                host_rdata <= StatusRegister0 | host_wdata;
            else
                host_rdata <= StatusRegister0;
        end 
        
        10'h4:begin
            if(host_wvalid && (host_awaddr[9:0] == 10'h4))
                host_rdata <= StatusRegister1 | host_wdata;
            else
                host_rdata <= StatusRegister1;  
        end 
        
        10'h8:begin
            if(host_wvalid && (host_awaddr[9:0] == 10'h8))
                host_rdata <= StatusRegister2 | host_wdata;
            else
                host_rdata <= StatusRegister2;
        end 
        
        10'hC:begin
            if(host_wvalid && (host_awaddr[9:0] == 10'hC))
                host_rdata <= StatusRegister3 | host_wdata;
            else
                host_rdata <= StatusRegister3;
        end
        
        
        10'h18 : host_rdata <= CuDmaEnable;
        10'h1C : host_rdata <= CuDmaQueue0;
        10'h20 : host_rdata <= CuDmaQueue1;
        10'h24 : host_rdata <= CuDmaQueue2;
        10'h28 : host_rdata <= CuDmaQueue3;
        
        10'h2C : host_rdata <= SlotSize;
        10'h30 : host_rdata <= CuOffset;
        10'h34 : host_rdata <= NoofSlots;      
        10'h38 : host_rdata <= CuBaseAddress;
        10'h3C : host_rdata <= CqBaseAddress;
        
        10'h40 : host_rdata <= CuIsrHandlerEnable;
        10'h44 : host_rdata <= cu_inc_status0;
        10'h48 : host_rdata <= cu_inc_status1;
        10'h4C : host_rdata <= cu_inc_status2;
        10'h50 : host_rdata <= cu_inc_status3;


/*
 * CqSlotQueue registers are also COR register and hence the it also has the
 * same control logic as the Status Registers mentioned above
 */
        
        10'h54 : host_rdata <= CqSlotQueueEnable;
        10'h58 : begin
            if(host_wvalid && (host_awaddr[9:0] == 10'h58))
                host_rdata <= CqSlotQueue0 | host_wdata;
            else 
                host_rdata <= CqSlotQueue0;
        end 
        10'h5C : begin
            if(host_wvalid && (host_awaddr[9:0] == 10'h5C))
                host_rdata <= CqSlotQueue1 | host_wdata;
            else 
                host_rdata <= CqSlotQueue1;
        end
        10'h60 : begin
            if(host_wvalid && (host_awaddr[9:0] == 10'h60))
                host_rdata <= CqSlotQueue2 | host_wdata;
            else 
                host_rdata <= CqSlotQueue2;
        end
        10'h64 : begin
            if(host_wvalid && (host_awaddr[9:0] == 10'h64))
                host_rdata <= CqSlotQueue3 | host_wdata;
            else 
                host_rdata <= CqSlotQueue3;
        end
        
        10'h68 : host_rdata <= no_of_computeunits;
        
        10'h100: host_rdata <= GIE;
        
        
        10'h300: host_rdata <= DEBUG_StatusRegister_State_Write;
        10'h304: host_rdata <= DEBUG_StatusRegister_State_Read;
        10'h308: host_rdata <= DEBUG_StatusRegister_IrqHandler_State0;
        10'h30C: host_rdata <= DEBUG_StatusRegister_IrqHandler_State1;
        10'h310: host_rdata <= DEBUG_StatusRegister_IrqHandler_State2;
        10'h314: host_rdata <= DEBUG_StatusRegister_IrqHandler_State3;
        10'h318: host_rdata <= DEBUG_CuDmaController_State0;
        10'h328: host_rdata <= DEBUG_CuISR_State0;
/*
 * DEADBEEF is sent back in case a read is performed to unknown address space
 * within the ERT controller address space
 */        
        default: host_rdata <= 32'hDEADBEEF;                
        endcase
    end
end 

//*****************************************************************************//
/*
 * This always block has the control logic storing and clearing the values of
 * the Status Register. When ever axilite read is performed to this address
 * location in the register map, the contents of the Status Register are
 * cleared to 0. When a axilite write is performed the data is ored with the
 * existing data, there allowing the ERT firmware to directly write into this
 * registor without having to read modify write.
 */

always @(posedge clk)
begin
    if(host_rvalid && host_rready && (host_araddr[9:0] == 10'h0))
        StatusRegister0 <= 0; 
    else if(host_wvalid && host_wready && (host_awaddr[9:0] == 10'h0)) // Write request
        StatusRegister0 <= StatusRegister0 | host_wdata;
        
    if(host_rvalid && host_rready && (host_araddr[9:0] == 10'h4))
            StatusRegister1 <= 0; 
        else if(host_wvalid && host_wready && (host_awaddr[9:0] == 10'h4)) // Write request
            StatusRegister1 <= StatusRegister1 | host_wdata;

    if(host_rvalid && host_rready && (host_araddr[9:0] == 10'h8))
        StatusRegister2 <= 0; 
    else if(host_wvalid && host_wready && (host_awaddr[9:0] == 10'h8)) // Write request
        StatusRegister2 <= StatusRegister2 | host_wdata;

    if(host_rvalid && host_rready && (host_araddr[9:0] == 10'hC))
        StatusRegister3 <= 0; 
    else if(host_wvalid && host_wready && (host_awaddr[9:0] == 10'hC)) // Write request
        StatusRegister3 <= StatusRegister3 | host_wdata;
end // end of always block

//***************************************************************************//
/*
 * This always block is used to assigned axilite write data to the appropriate 
 * CSR registers. Few of the others are assigned in seperate always blocks.
 */

always @(posedge clk)
begin
    if(host_wvalid && host_wready)
    begin
        case (host_awaddr[9:0])
           10'h18: CuDmaEnable         <= host_wdata[0];
           10'h2C: SlotSize            <= host_wdata[12:0];
           10'h30: CuOffset            <= host_wdata[5:0];
           10'h34: NoofSlots           <= host_wdata[7:0];
           10'h38: CuBaseAddress       <= host_wdata;
           10'h3C: CqBaseAddress       <= host_wdata;
           10'h40: CuIsrHandlerEnable  <= host_wdata[0];
           10'h54: CqSlotQueueEnable   <= host_wdata[0];
           10'h68: no_of_computeunits  <= host_wdata[7:0];
           10'h100: GIE                <= host_wdata[0];
        endcase
    end
end //end of always block 

/*
 * The the bresp ands rresp sginals for the axilite bus are tied to 0
 */
assign host_bresp = 0;
assign host_rresp = 0; 

//***************************************************************************//

/*
 * This is the combinational logic which sets the CuDmaQueueBusy bit to 1,
 * whenever a axilite write is in progress on address locations 1C,20,24,28.
 * This address locations correspond to CSR registers CuDmaQueue*, these hold
 * the command slot values which need to be processed by the CU DMA HLS IP.
 */


always @(*)
begin
    if(host_wvalid && host_wready && (host_awaddr[9:0] == 10'h1C))
        CuDmaQueueBusy = 1;
    else if (host_wvalid && host_wready && (host_awaddr[9:0] == 10'h20))
        CuDmaQueueBusy = 1;
    else if (host_wvalid && host_wready && (host_awaddr[9:0] == 10'h24))
        CuDmaQueueBusy = 1;
    else if (host_wvalid && host_wready && (host_awaddr[9:0] == 10'h28))
        CuDmaQueueBusy = 1;
    else
        CuDmaQueueBusy = 0;
end 

/*
 * When there is a axilite write request to address locations 1C,20,24,28, the
 * data is written in to one of the CuDmaQueue* Registers. Alongside that the
 * CuDmaCount is also incremented by 1, this count value is used by the CuDme
 * controller logic to enable and begin operations for the CU DMA HLS IP.
 * When the clear_CuQueue is set by the CuDMA controller logic the count and
 * queue register values are reset to 0.
 */

always @(posedge clk)
begin
    if(clear_CuQueue)
    begin
        CuDmaQueue0 <= 0; 
        CuDmaCount0 <= 0;
    end 
    else if(host_wvalid && host_wready && (host_awaddr[9:0] == 10'h1C)) // Write request
    begin
        CuDmaQueue0 <= CuDmaQueue0 | host_wdata;
        CuDmaCount0 <= CuDmaCount0 + 1;
    end 
        
    if(clear_CuQueue)
    begin
        CuDmaQueue1 <= 0; 
        CuDmaCount1 <= 0;
    end 
    else if(host_wvalid && host_wready && (host_awaddr[9:0] == 10'h20)) // Write request
    begin
        CuDmaQueue1 <= CuDmaQueue1 | host_wdata;
        CuDmaCount1 <= CuDmaCount1 + 1;
    end 

    if(clear_CuQueue)
    begin
        CuDmaQueue2 <= 0; 
        CuDmaCount2 <= 0;
    end 
    else if(host_wvalid && host_wready && (host_awaddr[9:0] == 10'h24)) // Write request
    begin
        CuDmaQueue2 <= CuDmaQueue2 | host_wdata;
        CuDmaCount2 <= CuDmaCount2 + 1;
    end 

    if(clear_CuQueue)
    begin
        CuDmaQueue3 <= 0; 
        CuDmaCount3 <= 0;
    end 
    else if(host_wvalid && host_wready && (host_awaddr[9:0] == 10'h28)) // Write request
    begin
        CuDmaQueue3 <= CuDmaQueue3 | host_wdata;
        CuDmaCount3 <= CuDmaCount3 + 1;
    end
end // end of always block

//****************************************************************************//
/*
 * This always block is used to write axilite date to the CqSlotQueue, and
 * update the CuSlotCount by 1 everytime there is a write request to address
 * space 58,5c,60,64. When there is read request both the count and the queue
 * values are cleared to 0. The CqSlotQueue register is essentially used for
 * indicating a new slot in the commadn queue. This is populated by the HOst,
 * when ever atleast 1 bit is set in the CqSlotQueue resgiter. The count
 * values is incremented as a result of this write , leads to an interrupt
 * being issued to the ERT which will trigger an ERT read and hence clear this
 * space and also figure out which command slot was configured by the host and
 * perform the tasks associated with the particular command Slot.
 */


always @(posedge clk) 
begin
    if(host_rvalid && host_rready && (host_araddr[9:0] == 10'h58))
    begin
        CqSlotQueue0        <= 0; 
        CqSlotQueueCount0   <= 0;
    end 
    else if(host_wvalid && host_wready && (host_awaddr[9:0] == 10'h58)) // Write request
    begin
        CqSlotQueue0        <= CqSlotQueue0 | host_wdata;
        CqSlotQueueCount0   <= CqSlotQueueCount0 + 1;
    end 
        
    if(host_rvalid && host_rready && (host_araddr[9:0] == 10'h5C))
    begin
        CqSlotQueue1        <= 0; 
        CqSlotQueueCount1   <= 0;
    end 
    else if(host_wvalid && host_wready && (host_awaddr[9:0] == 10'h5C)) // Write request
    begin
        CqSlotQueue1        <= CqSlotQueue1 | host_wdata;
        CqSlotQueueCount1   <= CqSlotQueueCount1 + 1;
    end 

    if(host_rvalid && host_rready && (host_araddr[9:0] == 10'h60))
    begin
        CqSlotQueue2        <= 0; 
        CqSlotQueueCount2   <= 0;
    end 
    else if(host_wvalid && host_wready && (host_awaddr[9:0] == 10'h60)) // Write request
    begin
        CqSlotQueue2        <= CqSlotQueue2 | host_wdata;
        CqSlotQueueCount2   <= CqSlotQueueCount2 + 1;
    end 

    if(host_rvalid && host_rready && (host_araddr[9:0] == 10'h64))
    begin
        CqSlotQueue3        <= 0; 
        CqSlotQueueCount3   <= 0;
    end 
    else if(host_wvalid && host_wready && (host_awaddr[9:0] == 10'h64)) // Write request
    begin
        CqSlotQueue3        <= CqSlotQueue3 | host_wdata;
        CqSlotQueueCount3   <= CqSlotQueueCount3 + 1;
    end
end // end of always block

//****************************************************************************//
/*
 * This functionality is described above
 */

reg slotavailable0=0;
reg slotavailable1=0;
reg slotavailable2=0;
reg slotavailable3=0;

always @(posedge clk)
begin
    if(CqSlotQueueCount0 > 0)
        slotavailable0 <= 1;
    else 
        slotavailable0 <= 0;
end 

always @(posedge clk)
begin
    if(CqSlotQueueCount1 > 0)
        slotavailable1 <= 1;
    else 
        slotavailable1 <= 0;
end 

always @(posedge clk)
begin
    if(CqSlotQueueCount2 > 0)
        slotavailable2 <= 1;
    else 
        slotavailable2 <= 0;
end

always @(posedge clk)
begin
    if(CqSlotQueueCount3 > 0)
        slotavailable3 <= 1;
    else 
        slotavailable3 <= 0;
end  

assign irq_slotavailable = (CqSlotQueueEnable) ? (slotavailable0 | slotavailable1 | slotavailable2 | slotavailable3) : 0;


//****************************************************************************//

//IRQ Handler for StatusRegister0
irq_handler sr0 (
    .clk                (clk                ),
    .reset_n            (reset_n            ),
    .StatusRegister     (StatusRegister0    ),
    .irq                (irq_sr0            ),
    .irq_ack            (irq_ack[0]         ),
    .GIE                (GIE                ),
    .state              (DEBUG_StatusRegister_IrqHandler_State0)
);

//IRQ Handler for StatusRegister1
irq_handler sr1 (
    .clk                (clk                ),
    .reset_n            (reset_n            ),
    .StatusRegister     (StatusRegister1    ),
    .irq                (irq_sr1            ),
    .irq_ack            (irq_ack[1]         ),
    .GIE                (GIE                ),
    .state              (DEBUG_StatusRegister_IrqHandler_State1)
);

//IRQ Handler for StatusRegister2
irq_handler sr2 (
    .clk                (clk                ),
    .reset_n            (reset_n            ),
    .StatusRegister     (StatusRegister2    ),
    .irq                (irq_sr2            ),
    .irq_ack            (irq_ack[2]         ),
    .GIE                (GIE                ),
    .state              (DEBUG_StatusRegister_IrqHandler_State2)              
);

//IRQ Handler for StatusRegister3
irq_handler sr3 (
    .clk                (clk                ),
    .reset_n            (reset_n            ),
    .StatusRegister     (StatusRegister3    ),
    .irq                (irq_sr3            ),
    .irq_ack            (irq_ack[3]         ),
    .GIE                (GIE                ),
    .state              (DEBUG_StatusRegister_IrqHandler_State3)
);

//DMA MOdule Instantiation
CuDmaController_rtl CuDmaController_rtl(  
    .clk                ( clk               ),
    .reset_n            ( reset_n           ),   
    .busy               ( CuDmaQueueBusy    ),
    .CuDmaEnable        ( CuDmaEnable       ),
    .CuDmaQueue0        ( CuDmaQueue0       ),
    .CuDmaQueue1        ( CuDmaQueue1       ),
    .CuDmaQueue2        ( CuDmaQueue2       ),
    .CuDmaQueue3        ( CuDmaQueue3       ),
    .ap_start           ( ap_start_cudma    ),
    .ap_done            ( ap_done_cudma     ),
    .CqDmaQueue_reg     ( CqDmaQueue_reg    ),    
    .clear              ( clear_CuQueue     ),
    .CuDmaCount0        ( CuDmaCount0       ),
    .CuDmaCount1        ( CuDmaCount1       ),
    .CuDmaCount2        ( CuDmaCount2       ),
    .CuDmaCount3        ( CuDmaCount3       ),
    .state0             ( DEBUG_CuDmaController_State0)
);

//DMA MOdule Instantiation
CuISR CuISR(  
    .clk                ( clk               ),
    .reset_n            ( reset_n           ),
   
    .CuIsrEnable        ( CuIsrHandlerEnable),
    .cu_inc_status0     ( cu_inc_status0    ),
    .cu_inc_status1     ( cu_inc_status1    ),
    .cu_inc_status2     ( cu_inc_status2    ),
    .cu_inc_status3     ( cu_inc_status3    ),
    
    .CU_INC_IN          ( irq_cu_cdc        ),
    .irq_cu_completion  ( irq_cu_completion ),
    
    .CuOffset           ( CuOffset          ),
    .no_of_computeunits ( no_of_computeunits),  
    .CuBaseAddress      ( CuBaseAddress     ),
    
    .host_rready        ( host_rready       ),
    .host_rvalid        ( host_rvalid       ),
    .host_araddr        ( host_araddr       ),
    
    .ap_start           ( ap_start_cuisr    ),
    .ap_done            ( ap_done_cuisr     ),    
    .Offset             ( Offset            ),
    
    //DEBUG SIGNALS
    .state0             ( DEBUG_CuISR_State0)
);


assign DEBUG_StatusRegister_State_Write = state_write;
assign DEBUG_StatusRegister_State_Read  = state_read;




endmodule
