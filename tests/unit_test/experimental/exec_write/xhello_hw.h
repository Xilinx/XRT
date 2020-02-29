// ==============================================================
// File generated on Wed Nov 28 04:48:38 MST 2018
// Vivado(TM) HLS - High-Level Synthesis from C, C++ and SystemC v2018.3 (64-bit)
// SW Build 2399019 on Tue Nov 27 19:07:14 MST 2018
// IP Build 2398463 on Tue Nov 27 21:07:40 MST 2018
// Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
// ==============================================================
// control
// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read/COR)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read)
//        bit 7  - auto_restart (Read/Write)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0  - Channel 0 (ap_done)
//        bit 1  - Channel 1 (ap_ready)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/TOW)
//        bit 0  - Channel 0 (ap_done)
//        bit 1  - Channel 1 (ap_ready)
//        others - reserved
// 0x10 : Data signal of group_id_x
//        bit 31~0 - group_id_x[31:0] (Read/Write)
// 0x14 : reserved
// 0x18 : Data signal of group_id_y
//        bit 31~0 - group_id_y[31:0] (Read/Write)
// 0x1c : reserved
// 0x20 : Data signal of group_id_z
//        bit 31~0 - group_id_z[31:0] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of global_offset_x
//        bit 31~0 - global_offset_x[31:0] (Read/Write)
// 0x2c : reserved
// 0x30 : Data signal of global_offset_y
//        bit 31~0 - global_offset_y[31:0] (Read/Write)
// 0x34 : reserved
// 0x38 : Data signal of global_offset_z
//        bit 31~0 - global_offset_z[31:0] (Read/Write)
// 0x3c : reserved
// 0x40 : Data signal of access1
//        bit 31~0 - access1[31:0] (Read/Write)
// 0x44 : Data signal of access1
//        bit 31~0 - access1[63:32] (Read/Write)
// 0x48 : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XHELLO_HELLO_CONTROL_ADDR_AP_CTRL              0x00
#define XHELLO_HELLO_CONTROL_ADDR_GIE                  0x04
#define XHELLO_HELLO_CONTROL_ADDR_IER                  0x08
#define XHELLO_HELLO_CONTROL_ADDR_ISR                  0x0c
#define XHELLO_HELLO_CONTROL_ADDR_GROUP_ID_X_DATA      0x10
#define XHELLO_HELLO_CONTROL_BITS_GROUP_ID_X_DATA      32
#define XHELLO_HELLO_CONTROL_ADDR_GROUP_ID_Y_DATA      0x18
#define XHELLO_HELLO_CONTROL_BITS_GROUP_ID_Y_DATA      32
#define XHELLO_HELLO_CONTROL_ADDR_GROUP_ID_Z_DATA      0x20
#define XHELLO_HELLO_CONTROL_BITS_GROUP_ID_Z_DATA      32
#define XHELLO_HELLO_CONTROL_ADDR_GLOBAL_OFFSET_X_DATA 0x28
#define XHELLO_HELLO_CONTROL_BITS_GLOBAL_OFFSET_X_DATA 32
#define XHELLO_HELLO_CONTROL_ADDR_GLOBAL_OFFSET_Y_DATA 0x30
#define XHELLO_HELLO_CONTROL_BITS_GLOBAL_OFFSET_Y_DATA 32
#define XHELLO_HELLO_CONTROL_ADDR_GLOBAL_OFFSET_Z_DATA 0x38
#define XHELLO_HELLO_CONTROL_BITS_GLOBAL_OFFSET_Z_DATA 32
#define XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA         0x40
#define XHELLO_HELLO_CONTROL_BITS_ACCESS1_DATA         64
