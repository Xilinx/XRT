//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 05/30/2017 08:58:15 PM
// Design Name: 
// Module Name: testbench
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////
`timescale 1ps/1ps

`define WRITE_AXILTE_XFER(vip_agent,trans,addr,data) \
    ``trans.set_write_cmd(     \
      ``addr,                 \
       XIL_AXI_BURST_TYPE_INCR,     \
       0,                           \
       0,                           \
       XIL_AXI_SIZE_4BYTE           \
     );                             \
    ``trans.set_data_beat(0,``data);  \
    ``vip_agent.mst_wr_driver.send(``trans);

`define READ_AXILTE_XFER(vip_agent,trans,addr) \
    ``trans.set_read_cmd(          \
       ``addr,                     \
        XIL_AXI_BURST_TYPE_INCR,  \
        0,                        \
        0,                        \
        XIL_AXI_SIZE_4BYTE        \
     );                           \
     ``trans.set_driver_return_item_policy(XIL_AXI_PAYLOAD_RETURN); \
     ``vip_agent.mst_rd_driver.send(``trans);


module testbench();

import axi_vip_pkg::*;

import pfm_top_axi_vip_ctrl_mgntpf_0_pkg::*;
import pfm_top_axi_vip_ctrl_userpf_0_pkg::*;
import pfm_top_axi_vip_data_0_pkg::*;

import pfm_dynamic_axi_vip_data_0_pkg::*;
import pfm_dynamic_axi_vip_ctrl_mgntpf_0_pkg::*;
import pfm_dynamic_axi_vip_ctrl_userpf_0_pkg::*;
//

//Base region VIP 
pfm_top_axi_vip_ctrl_mgntpf_0_passthrough_t             axi_mgntpf;
pfm_top_axi_vip_ctrl_userpf_0_passthrough_t             axi_userpf;
pfm_top_axi_vip_data_0_passthrough_t                    axi_data;

//EXPANDED region VIP
pfm_dynamic_axi_vip_data_0_passthrough_t           axi_exp_data;
pfm_dynamic_axi_vip_ctrl_mgntpf_0_passthrough_t axi_exp_ctrl_mgntpf;
pfm_dynamic_axi_vip_ctrl_userpf_0_passthrough_t axi_exp_ctrl_userpf;

//Offset Address
parameter int BRAM_OFFSET = 32'h0003_1000;


axi_transaction wr_transaction;
axi_transaction rd_transaction;
axi_transaction bram_tout;
reg [31:0] bram_data;
int data_q[$];


axi_transaction iic_rd_out;

axi_transaction tout;
axi_transaction wr_data_transaction;
axi_transaction wr_reactive;
axi_transaction userpf_wr_reactive;
axi_transaction mgntpf_wr_reactive;


axi_transaction wr_ctrl_mgnt;
axi_transaction wr_ctrl_mgnt_reactive;

axi_transaction wr_ctrl_usr;
axi_transaction rd_ctrl_usr;
axi_transaction rom_tout;
axi_transaction wr_ctrl_usr_reactive;

axi_transaction spi_wr_trans;
axi_transaction spi_rd_trans;
axi_transaction spi_out;

wire scl_io,sda_io;
wire scl_int;

wire io0_io,io1_io,io2_io,io3_io,ss_io;
reg spi_w_done;
reg [63:0] clk_counter = 0;

reg emc_clk;
reg iic_write_done,iic_read_done;


event base_mgntpf_done,base_userpf_done,base_data_done;
event exp_mgntpf_done,exp_userpf_done,exp_data_done;

  initial begin
  $timeformat(-9, 2, " ns", 20);

  // //Change severity of PC
  DSA.pfm_top_i.dynamic_region.axi_vip_ctrl_userpf.inst.clr_xilinx_slave_ready_check();
  DSA.pfm_top_i.dynamic_region.axi_vip_ctrl_mgntpf.inst.clr_xilinx_slave_ready_check();
  `ifndef XILINX_SIMULATOR
    DSA.pfm_top_i.dynamic_region.axi_vip_data.inst.set_fatal_to_warnings();
    DSA.pfm_top_i.dynamic_region.axi_vip_ctrl_userpf.inst.set_fatal_to_warnings();
    DSA.pfm_top_i.dynamic_region.axi_vip_ctrl_mgntpf.inst.set_fatal_to_warnings();
  `endif
  end


  initial begin : PCIE_CLK
    //Force clocks
    forever begin
      force DSA.pfm_top_i.static_region.dma_pcie.axi_aclk = 1'b0;
      #2000ps;
      force DSA.pfm_top_i.static_region.dma_pcie.axi_aclk = 1'b1;
      #2000ps;
    end
  end

  initial begin : RST_GEN
    //Force PCIE Link up to to generate reset regslice blocks 
    force DSA.pfm_top_i.static_region_user_lnk_up = 1'b1;
    force DSA.pfm_top_i.static_region.dma_pcie.axi_aresetn = 1'b0;
    repeat (20) @(posedge DSA.pfm_top_i.static_region.dma_pcie.axi_aclk);
    force DSA.pfm_top_i.static_region.dma_pcie.axi_aresetn = 1'b1;
  end


  //Generate reset for VIP : expanded_region/axi_vip_expanded_data 
  //To aviod protocol errors on reset
  //initial begin : RST_GEN_XPR
  //  force DSA.pfm_top_i.expanded_region.axi_vip_expanded_data.aresetn = 1'b0;
  //  repeat (20) @(posedge DSA.pfm_top_i.static_region.dma_pcie.axi_aclk);
  //  force DSA.pfm_top_i.expanded_region.axi_vip_expanded_data.aresetn = 1'b1;
  //end


 always @(posedge DSA.pfm_top_i.static_region.dma_pcie.axi_aclk) begin
    clk_counter <= clk_counter +1;
 end


 initial begin
    axi_mgntpf = new("Mngt",DSA.pfm_top_i.static_region.axi_vip_ctrl_mgntpf.inst.IF);
    axi_mgntpf.set_agent_tag("Passthrough MGNT VIP");
    axi_mgntpf.set_verbosity(0);
    DSA.pfm_top_i.static_region.axi_vip_ctrl_mgntpf.inst.set_master_mode();
    axi_mgntpf.start_monitor();
    axi_mgntpf.start_master();
    wr_transaction = axi_mgntpf.mst_wr_driver.create_transaction("IC_WRITE");
    rd_transaction = axi_mgntpf.mst_rd_driver.create_transaction("IC_READ");
 

    //Dynamic region:MGNTPF VIP(Slave Mode)
    axi_exp_ctrl_mgntpf = new("exp_slv_mgntpf",DSA.pfm_top_i.dynamic_region.axi_vip_ctrl_mgntpf.inst.IF);
    axi_exp_ctrl_mgntpf.set_agent_tag("Passthrough VIP");
    axi_exp_ctrl_mgntpf.set_verbosity(XIL_AXI_VERBOSITY_NONE);
    DSA.pfm_top_i.dynamic_region.axi_vip_ctrl_mgntpf.inst.set_slave_mode();
    axi_exp_ctrl_mgntpf.start_monitor();
    axi_exp_ctrl_mgntpf.start_slave();

    //Base clocking block generates Clock for other modules..Genrated from Clk_wiz 
    //wait for Locked to be 1 for clk_wiz block 
    wait(DSA.pfm_top_i.static_region.base_clocking.clkwiz_sysclks.locked == 1);
    repeat(2)@(posedge DSA.pfm_top_i.static_region.dma_pcie.axi_aclk);
   

    //Program GPIO to generate resets for expanded region -- GPIO-DATA (gate_pr)
   `WRITE_AXILTE_XFER(axi_mgntpf,wr_transaction,32'h0003_0000,32'h00000003)
    axi_mgntpf.mst_wr_driver.wait_driver_idle();
    //Read GPIO output (GPIO2 -- Loopback connection)
   `READ_AXILTE_XFER(axi_mgntpf,rd_transaction,32'h0003_0008)
    axi_mgntpf.mst_rd_driver.wait_rsp(bram_tout);
    $display("%0t :: GPIO2 ADDR: 0x%x - DATA:0x%x",$time,bram_tout.get_addr(),bram_tout.get_data_beat(0));
    if(bram_tout.get_data_beat(0) != 32'h00000003) begin
      $display("ERROR: GPIO program failure\n");
    end


   
   //WRITE/READ to AXI BRAM (scratchpad_ram_ctrl) 
   for (int i = 0; i <= 4;i++) begin
     bram_data = 32'h000020+(i*4);
    `WRITE_AXILTE_XFER(axi_mgntpf,wr_transaction,BRAM_OFFSET+(i*4),bram_data)
     data_q.push_back(bram_data);
     //$display("RAM WRITE ADDR: 0x%x - DATA:0x%x", wr_transaction.get_addr(),bram_data);
   end
   axi_mgntpf.mst_wr_driver.wait_driver_idle();
   $display("%0t :: RAM WRITE Completed",$time);
   for (int i = 0; i <= 4;i++) begin
   `READ_AXILTE_XFER(axi_mgntpf,rd_transaction,BRAM_OFFSET+(i*4))
    axi_mgntpf.mst_rd_driver.wait_rsp(bram_tout);
    $display("%0t :: RAM  READ ADDR: 0x%x - DATA:0x%x QUEUE 0x%x",$time,bram_tout.get_addr(),bram_tout.get_data_beat(0),data_q[i]);
    if(data_q[i] != bram_tout.get_data_beat(0)) begin 
      $display("ERROR: scratchpad_ram_ctrl read back error GOT: 0x%x EXPECTED: 0x%0x", bram_tout.get_data_beat(0),data_q[i]);
    end
   end


    
   ////////////////////////////////////////////////////
   //I2C  

    //ISR 
    `WRITE_AXILTE_XFER(axi_mgntpf,wr_transaction,32'h0004_1020,32'h000000D0)
    //TX_FIFO -- SLV ADDR
    `WRITE_AXILTE_XFER(axi_mgntpf,wr_transaction,32'h0004_1108,32'h00000068)
    //TX_FIFO -- CMD
    `WRITE_AXILTE_XFER(axi_mgntpf,wr_transaction,32'h0004_1108,32'h000000FF)
    //RX_FIFO_PIRQ
    `WRITE_AXILTE_XFER(axi_mgntpf,wr_transaction,32'h0004_1120,32'h00000000)
    //CR -(Control register) ()
    `WRITE_AXILTE_XFER(axi_mgntpf,wr_transaction,32'h0004_1100,32'h0000000D)
    axi_mgntpf.mst_wr_driver.wait_driver_idle();

    repeat(10)@(posedge DSA.pfm_top_i.static_region.dma_pcie.axi_aclk);

    //READ ISR register to TX_FIFO empty interrupt(Indiactes IIC write completion)
    do 
      begin
      `READ_AXILTE_XFER(axi_mgntpf,rd_transaction,32'h0004_1020)
      ///////////////////////////////////////////////////////////////////////////
      //Block until the RRESP has finished
      axi_mgntpf.mst_rd_driver.wait_rsp(tout);
      iic_write_done = (tout.get_data_beat(0) == 32'h000000D4) ? 1 : 0; 
      //$display("%0t :: iic_write_done %0x ISR : %0x",$time,iic_write_done,tout.get_data_beat(0));
    end
    while(!iic_write_done);
    $display("%0t iic_write_done ..... ",$time);
   
    repeat (10) @(posedge DSA.pfm_top_i.static_region.dma_pcie.axi_aclk);
    //Set iic slave to send data
    iic_slave.set_slv_mode(1);
    //Set CR 
    `WRITE_AXILTE_XFER(axi_mgntpf,wr_transaction,32'h0004_1020,32'h00000004)
    `WRITE_AXILTE_XFER(axi_mgntpf,wr_transaction,32'h0004_1100,32'h00000025)
    //TX_FIFO -- SLV ADDR
    `WRITE_AXILTE_XFER(axi_mgntpf,wr_transaction,32'h0004_1108,32'h00000069)
    `WRITE_AXILTE_XFER(axi_mgntpf,wr_transaction,32'h0004_1020,32'h00000004)
     axi_mgntpf.mst_wr_driver.wait_driver_idle();

    do 
      begin
      `READ_AXILTE_XFER(axi_mgntpf,rd_transaction,32'h0004_1020)
      ///////////////////////////////////////////////////////////////////////////
      //Block until the RRESP has finished
      axi_mgntpf.mst_rd_driver.wait_rsp(tout);
      iic_read_done = (tout.get_data_beat(0) == 32'h000000D8) ? 1 : 0; 
      //$display("%0t :: iic_read_done %0x ISR : %0x",$time,iic_read_done,tout.get_data_beat(0));
      end
    while(!iic_read_done);

    $display("%0t :: iic_read_done",$time);
    axi_mgntpf.wait_mst_drivers_idle();

    ///////////////////////////////////////////////////////////////
    //READ RX_FIFO content
    `READ_AXILTE_XFER(axi_mgntpf,rd_transaction,32'h0004_110C)
    axi_mgntpf.mst_rd_driver.wait_rsp(iic_rd_out);
    $display("%0t :: RX_FIFO : %0x %0x",$time,iic_rd_out.get_addr(),iic_rd_out.get_data_beat(0));
    if(iic_rd_out.get_data_beat(0) != 32'h00000055) begin 
      $display("ERROR: Received unexpected Data on I2C  : Expected: 32'h00000055  Got: %0x ",iic_rd_out.get_data_beat(0));
    end



   //////////////////////////////////////////////////
   //SYS_MON_WIZ

   //Configure sys_mon after EOS,EOC are up  
   //wait(DSA.pfm_top_i.static_region.sys_mgmt_wiz.eos_out == 1);
   //$display("SYS_MON EOS_OUT UP .....");
   wait(DSA.pfm_top_i.static_region.sys_mgmt_wiz.eoc_out == 1);
   $display("SYS_MON: EOC_OUT UP .....");

   //READ Temparature & VCCINT(Dummy) for sys_mon
   `READ_AXILTE_XFER(axi_mgntpf,rd_transaction,32'h000A_0400)
   `READ_AXILTE_XFER(axi_mgntpf,rd_transaction,32'h000A_0404)

   //////////////////////////////////////////////////
   //HW_ICAP

   //READ/WRITE acess to HW ICAP
   `READ_AXILTE_XFER(axi_mgntpf,rd_transaction,32'h0002_0114)
    //$display("Addr: 0x%x - 0x%x", rd_transaction.get_addr(),rd_transaction.get_data_beat(0));

   `WRITE_AXILTE_XFER(axi_mgntpf,wr_transaction,32'h0002_0100,32'h00FF)
    axi_mgntpf.mst_wr_driver.wait_driver_idle();
   `READ_AXILTE_XFER(axi_mgntpf,rd_transaction,32'h0002_0110)
   `READ_AXILTE_XFER(axi_mgntpf,rd_transaction,32'h0002_0114)
    axi_mgntpf.wait_mst_drivers_idle();
    axi_mgntpf.mst_rd_driver.wait_rsp(bram_tout);



   fork
    begin
      for(int i = 0; i < 4 ; i++ ) begin
        `WRITE_AXILTE_XFER(axi_mgntpf,wr_transaction,32'h100_0000+(i*4),32'h000040)
      end
    end
    begin
     for(int i = 0; i < 4 ; i++ ) begin
      axi_exp_ctrl_mgntpf.slv_wr_driver.get_wr_reactive(mgntpf_wr_reactive);
      axi_exp_ctrl_mgntpf.slv_wr_driver.send(mgntpf_wr_reactive);
     end
    end
   join
  

   $display("axi_mgntpf program done");
   ->base_mgntpf_done;
   ->exp_mgntpf_done;
end



initial begin
    axi_userpf = new("userpf",DSA.pfm_top_i.static_region.axi_vip_ctrl_userpf.inst.IF);
    axi_userpf.set_agent_tag("Passthrough VIP");
    axi_userpf.set_verbosity(0);
    DSA.pfm_top_i.static_region.axi_vip_ctrl_userpf.inst.set_master_mode();
    axi_userpf.start_monitor();
    axi_userpf.start_master();
    wr_ctrl_usr = axi_userpf.mst_wr_driver.create_transaction("ROM_WRITE");
    rd_ctrl_usr = axi_userpf.mst_rd_driver.create_transaction("ROM_READ");

    //Dynamic region:USERPF VIP(Slave Mode)
    axi_exp_ctrl_userpf = new("exp_slv_userpf",DSA.pfm_top_i.dynamic_region.axi_vip_ctrl_userpf.inst.IF);
    axi_exp_ctrl_userpf.set_agent_tag("Passthrough VIP");
    axi_exp_ctrl_userpf.set_verbosity(XIL_AXI_VERBOSITY_NONE);
    DSA.pfm_top_i.dynamic_region.axi_vip_ctrl_userpf.inst.set_slave_mode();
    axi_exp_ctrl_userpf.start_monitor();
    axi_exp_ctrl_userpf.start_slave();
    


    //READ access to feature_rom_ctrl 
    wr_ctrl_usr.set_driver_return_item_policy(XIL_AXI_PAYLOAD_RETURN);
    //WRITE to feature_rom_ctrl issues DECERR from MMU block
   `WRITE_AXILTE_XFER(axi_userpf,wr_ctrl_usr,32'h000B_0000,32'h000040)
    
    axi_userpf.mst_wr_driver.wait_rsp(rom_tout);
    $display("ROM Wrote: 0x%x - 0x%x - %0s", wr_ctrl_usr.get_addr(),rom_tout.get_data_beat(0),rom_tout.get_bresp());
    axi_userpf.mst_wr_driver.wait_driver_idle();

    //READ gives 0's (Since no Memory) 
   `READ_AXILTE_XFER(axi_userpf,rd_ctrl_usr,32'h000B_0000)
    axi_userpf.mst_rd_driver.wait_rsp(rom_tout);
    //$display("ROM :: %0x",rom_tout.get_data_beat(0));
 


   fork
    begin
      for(int i = 0; i < 4 ; i++ ) begin
        `WRITE_AXILTE_XFER(axi_userpf,wr_ctrl_usr,32'h1FF_F000+(i*4),32'h000040)
      end
    end
    begin
     for(int i = 0; i < 4 ; i++ ) begin
      axi_exp_ctrl_userpf.slv_wr_driver.get_wr_reactive(userpf_wr_reactive);
      axi_exp_ctrl_userpf.slv_wr_driver.send(userpf_wr_reactive);
     end
    end
   join
   
    $display("axi_userpf program done");
    ->base_userpf_done;
    ->exp_userpf_done;
end


initial begin

    //Base region:DATA VIP(Master Mode) 
    axi_data = new("base_mst_data",DSA.pfm_top_i.static_region.axi_vip_data.inst.IF);
    axi_data.set_agent_tag("Passthrough VIP");
    axi_data.set_verbosity(0);
    DSA.pfm_top_i.static_region.axi_vip_data.inst.set_master_mode();
    axi_data.start_monitor();
    axi_data.start_master();

   //Dynamic region:DATA VIP(Slave Mode)
   axi_exp_data = new("exp_slv_data",DSA.pfm_top_i.dynamic_region.axi_vip_data.inst.IF);
   axi_exp_data.set_agent_tag("Passthrough VIP");
   axi_exp_data.set_verbosity(XIL_AXI_VERBOSITY_NONE);
   DSA.pfm_top_i.dynamic_region.axi_vip_data.inst.set_slave_mode();
   axi_exp_data.start_monitor();
   axi_exp_data.start_slave();
 
   fork
    begin
      wr_data_transaction = axi_data.mst_wr_driver.create_transaction("WR_DATA");
      for(int i = 0; i < 4 ; i++ ) begin
        wr_data_transaction.set_write_cmd(
           64'h0_0000_0000+(i*64),
           XIL_AXI_BURST_TYPE_INCR,
           0,
           0,
           XIL_AXI_SIZE_8BYTE
         );
         wr_data_transaction.set_data_beat(0,512'h05000040);
         axi_data.mst_wr_driver.send(wr_data_transaction);
         //$display("Wrote: 0x%x - 0x%x", wr_data_transaction.get_addr(),512'h05000040);
      end
    end
    begin
     for(int i = 0; i < 4 ; i++ ) begin
     axi_exp_data.slv_wr_driver.get_wr_reactive(wr_reactive);
     //fill_reactive(wr_reactive);
     axi_exp_data.slv_wr_driver.send(wr_reactive);
    end
    end
   join
 
  //$display("Data program done");
  ->base_data_done;
  ->exp_data_done;
end






initial begin
 fork
  begin
   @(base_mgntpf_done);
   axi_mgntpf.wait_mst_drivers_idle();
  end
  begin
   @(base_data_done);
   axi_data.wait_mst_drivers_idle();
  end
  begin
    @(base_userpf_done);
    axi_userpf.wait_mst_drivers_idle();
  end
  begin
   @(exp_mgntpf_done);
   axi_exp_ctrl_mgntpf.wait_mst_drivers_idle();
  end
  begin
   @(exp_data_done);
   axi_exp_data.wait_mst_drivers_idle();
  end
  begin
    @(exp_userpf_done);
    axi_exp_ctrl_userpf.wait_mst_drivers_idle();
  end
 join

 $display("Total clks : %0d",clk_counter);
 $display("INFO:ALL DRIVERS IDLE");
 $finish;
end

initial begin
    repeat (500000) @(posedge DSA.pfm_top_i.static_region.dma_pcie.axi_aclk);
    $display ("ERROR: Test Time out");
    $finish;
end


final begin
 $display ("########################End of simulation###########################");
end


iic_slave iic_slave (
.sda_io(sda_io),
.scl_io(scl_io)
);


pfm_top_wrapper DSA (
 .c0_ddr4_act_n(  ),
 .c0_ddr4_adr(  ),
 .c0_ddr4_ba(  ),
 .c0_ddr4_bg(  ),
 .c0_ddr4_ck_c(  ),
 .c0_ddr4_ck_t(  ),
 .c0_ddr4_cke(  ),
 .c0_ddr4_cs_n(  ),
 .c0_ddr4_dq(  ),
 .c0_ddr4_dqs_c(  ),
 .c0_ddr4_dqs_t(  ),
 .c0_ddr4_odt(  ),
 .c0_ddr4_par(  ),
 .c0_ddr4_reset_n(  ),
 .c0_sys_clk_n(  ),
 .c0_sys_clk_p(  ),
 .c1_ddr4_act_n(  ),
 .c1_ddr4_adr(  ),
 .c1_ddr4_ba(  ),
 .c1_ddr4_bg(  ),
 .c1_ddr4_ck_c(  ),
 .c1_ddr4_ck_t(  ),
 .c1_ddr4_cke(  ),
 .c1_ddr4_cs_n(  ),
 .c1_ddr4_dq(  ),
 .c1_ddr4_dqs_c(  ),
 .c1_ddr4_dqs_t(  ),
 .c1_ddr4_odt(  ),
 .c1_ddr4_par(  ),
 .c1_ddr4_reset_n(  ),
 .c1_sys_clk_n(  ),
 .c1_sys_clk_p(  ),
 .c2_ddr4_act_n(  ),
 .c2_ddr4_adr(  ),
 .c2_ddr4_ba(  ),
 .c2_ddr4_bg(  ),
 .c2_ddr4_ck_c(  ),
 .c2_ddr4_ck_t(  ),
 .c2_ddr4_cke(  ),
 .c2_ddr4_cs_n(  ),
 .c2_ddr4_dq(  ),
 .c2_ddr4_dqs_c(  ),
 .c2_ddr4_dqs_t(  ),
 .c2_ddr4_odt(  ),
 .c2_ddr4_par(  ),
 .c2_ddr4_reset_n(  ),
 .c2_sys_clk_n(  ),
 .c2_sys_clk_p(  ),
 .c3_ddr4_act_n(  ),
 .c3_ddr4_adr(  ),
 .c3_ddr4_ba(  ),
 .c3_ddr4_bg(  ),
 .c3_ddr4_ck_c(  ),
 .c3_ddr4_ck_t(  ),
 .c3_ddr4_cke(  ),
 .c3_ddr4_cs_n(  ),
 .c3_ddr4_dq(  ),
 .c3_ddr4_dqs_c(  ),
 .c3_ddr4_dqs_t(  ),
 .c3_ddr4_odt(  ),
 .c3_ddr4_par(  ),
 .c3_ddr4_reset_n(  ),
 .c3_sys_clk_n(  ),
 .c3_sys_clk_p(  ),
 .iic_reset_n(  ),
 .iic_scl_io(scl_io),
 .iic_sda_io(sda_io),
 .init_calib_complete(  ),
 .led_0(  ),
 .pcie_7x_mgt_rxn(  ),
 .pcie_7x_mgt_rxp(  ),
 .pcie_7x_mgt_txn(  ),
 .pcie_7x_mgt_txp(  ),
 .perst_n(  ),
 .ref_clk_clk_n(  ),
 .ref_clk_clk_p(  )
);

endmodule
