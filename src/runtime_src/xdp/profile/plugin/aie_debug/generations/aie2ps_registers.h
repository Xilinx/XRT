// #######################################################################
// Copyright (c) 2024 AMD, Inc.  All rights reserved.
//
// This   document  contains  proprietary information  which   is
// protected by  copyright. All rights  are reserved. No  part of
// this  document may be photocopied, reproduced or translated to
// another  program  language  without  prior written  consent of
// XILINX Inc., San Jose, CA. 95124
//
// Xilinx, Inc.
// XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
// COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
// ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR
// STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION
// IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE
// FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
// XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
// THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO
// ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE
// FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE.
//
// #######################################################################

#ifndef AIE2PS_REGISTERS_H_
#define AIE2PS_REGISTERS_H_

namespace aie2ps
{
	
// Register definitions for AIE2PS
// ###################################

// Register definitions for CM
// ####################################
// Accumulator Control
const unsigned int cm_accumulator_control = 0x00036060;
// Combo event control
const unsigned int cm_combo_event_control = 0x00034404;
// Combo event inputs
const unsigned int cm_combo_event_inputs = 0x00034400;
// Core BMHH0 Part1
const unsigned int cm_core_bmhh0_part1 = 0x000300C0;
// Core BMHH0 Part2
const unsigned int cm_core_bmhh0_part2 = 0x000300D0;
// Core BMHH0 Part3
const unsigned int cm_core_bmhh0_part3 = 0x000300E0;
// Core BMHH0 Part4
const unsigned int cm_core_bmhh0_part4 = 0x000300F0;
// Core BMHH1 Part1
const unsigned int cm_core_bmhh1_part1 = 0x000301C0;
// Core BMHH1 Part2
const unsigned int cm_core_bmhh1_part2 = 0x000301D0;
// Core BMHH1 Part3
const unsigned int cm_core_bmhh1_part3 = 0x000301E0;
// Core BMHH1 Part4
const unsigned int cm_core_bmhh1_part4 = 0x000301F0;
// Core BMHH2 Part1
const unsigned int cm_core_bmhh2_part1 = 0x000302C0;
// Core BMHH2 Part2
const unsigned int cm_core_bmhh2_part2 = 0x000302D0;
// Core BMHH2 Part3
const unsigned int cm_core_bmhh2_part3 = 0x000302E0;
// Core BMHH2 Part4
const unsigned int cm_core_bmhh2_part4 = 0x000302F0;
// Core BMHH3 Part1
const unsigned int cm_core_bmhh3_part1 = 0x000303C0;
// Core BMHH3 Part2
const unsigned int cm_core_bmhh3_part2 = 0x000303D0;
// Core BMHH3 Part3
const unsigned int cm_core_bmhh3_part3 = 0x000303E0;
// Core BMHH3 Part4
const unsigned int cm_core_bmhh3_part4 = 0x000303F0;
// Core BMHH4 Part1
const unsigned int cm_core_bmhh4_part1 = 0x000304C0;
// Core BMHH4 Part2
const unsigned int cm_core_bmhh4_part2 = 0x000304D0;
// Core BMHH4 Part3
const unsigned int cm_core_bmhh4_part3 = 0x000304E0;
// Core BMHH4 Part4
const unsigned int cm_core_bmhh4_part4 = 0x000304F0;
// Core BMHH5 Part1
const unsigned int cm_core_bmhh5_part1 = 0x000305C0;
// Core BMHH5 Part2
const unsigned int cm_core_bmhh5_part2 = 0x000305D0;
// Core BMHH5 Part3
const unsigned int cm_core_bmhh5_part3 = 0x000305E0;
// Core BMHH5 Part4
const unsigned int cm_core_bmhh5_part4 = 0x000305F0;
// Core BMHH6 Part1
const unsigned int cm_core_bmhh6_part1 = 0x000306C0;
// Core BMHH6 Part2
const unsigned int cm_core_bmhh6_part2 = 0x000306D0;
// Core BMHH6 Part3
const unsigned int cm_core_bmhh6_part3 = 0x000306E0;
// Core BMHH6 Part4
const unsigned int cm_core_bmhh6_part4 = 0x000306F0;
// Core BMHH7 Part1
const unsigned int cm_core_bmhh7_part1 = 0x000307C0;
// Core BMHH7 Part2
const unsigned int cm_core_bmhh7_part2 = 0x000307D0;
// Core BMHH7 Part3
const unsigned int cm_core_bmhh7_part3 = 0x000307E0;
// Core BMHH7 Part4
const unsigned int cm_core_bmhh7_part4 = 0x000307F0;
// Core BMHL0 Part1
const unsigned int cm_core_bmhl0_part1 = 0x00030080;
// Core BMHL0 Part2
const unsigned int cm_core_bmhl0_part2 = 0x00030090;
// Core BMHL0 Part3
const unsigned int cm_core_bmhl0_part3 = 0x000300A0;
// Core BMHL0 Part4
const unsigned int cm_core_bmhl0_part4 = 0x000300B0;
// Core BMHL1 Part1
const unsigned int cm_core_bmhl1_part1 = 0x00030180;
// Core BMHL1 Part2
const unsigned int cm_core_bmhl1_part2 = 0x00030190;
// Core BMHL1 Part3
const unsigned int cm_core_bmhl1_part3 = 0x000301A0;
// Core BMHL1 Part4
const unsigned int cm_core_bmhl1_part4 = 0x000301B0;
// Core BMHL2 Part1
const unsigned int cm_core_bmhl2_part1 = 0x00030280;
// Core BMHL2 Part2
const unsigned int cm_core_bmhl2_part2 = 0x00030290;
// Core BMHL2 Part3
const unsigned int cm_core_bmhl2_part3 = 0x000302A0;
// Core BMHL2 Part4
const unsigned int cm_core_bmhl2_part4 = 0x000302B0;
// Core BMHL3 Part1
const unsigned int cm_core_bmhl3_part1 = 0x00030380;
// Core BMHL3 Part2
const unsigned int cm_core_bmhl3_part2 = 0x00030390;
// Core BMHL3 Part3
const unsigned int cm_core_bmhl3_part3 = 0x000303A0;
// Core BMHL3 Part4
const unsigned int cm_core_bmhl3_part4 = 0x000303B0;
// Core BMHL4 Part1
const unsigned int cm_core_bmhl4_part1 = 0x00030480;
// Core BMHL4 Part2
const unsigned int cm_core_bmhl4_part2 = 0x00030490;
// Core BMHL4 Part3
const unsigned int cm_core_bmhl4_part3 = 0x000304A0;
// Core BMHL4 Part4
const unsigned int cm_core_bmhl4_part4 = 0x000304B0;
// Core BMHL5 Part1
const unsigned int cm_core_bmhl5_part1 = 0x00030580;
// Core BMHL5 Part2
const unsigned int cm_core_bmhl5_part2 = 0x00030590;
// Core BMHL5 Part3
const unsigned int cm_core_bmhl5_part3 = 0x000305A0;
// Core BMHL5 Part4
const unsigned int cm_core_bmhl5_part4 = 0x000305B0;
// Core BMHL6 Part1
const unsigned int cm_core_bmhl6_part1 = 0x00030680;
// Core BMHL6 Part2
const unsigned int cm_core_bmhl6_part2 = 0x00030690;
// Core BMHL6 Part3
const unsigned int cm_core_bmhl6_part3 = 0x000306A0;
// Core BMHL6 Part4
const unsigned int cm_core_bmhl6_part4 = 0x000306B0;
// Core BMHL7 Part1
const unsigned int cm_core_bmhl7_part1 = 0x00030780;
// Core BMHL7 Part2
const unsigned int cm_core_bmhl7_part2 = 0x00030790;
// Core BMHL7 Part3
const unsigned int cm_core_bmhl7_part3 = 0x000307A0;
// Core BMHL7 Part4
const unsigned int cm_core_bmhl7_part4 = 0x000307B0;
// Core BMLH0 Part1
const unsigned int cm_core_bmlh0_part1 = 0x00030040;
// Core BMLH0 Part2
const unsigned int cm_core_bmlh0_part2 = 0x00030050;
// Core BMLH0 Part3
const unsigned int cm_core_bmlh0_part3 = 0x00030060;
// Core BMLH0 Part4
const unsigned int cm_core_bmlh0_part4 = 0x00030070;
// Core BMLH1 Part1
const unsigned int cm_core_bmlh1_part1 = 0x00030140;
// Core BMLH1 Part2
const unsigned int cm_core_bmlh1_part2 = 0x00030150;
// Core BMLH1 Part3
const unsigned int cm_core_bmlh1_part3 = 0x00030160;
// Core BMLH1 Part4
const unsigned int cm_core_bmlh1_part4 = 0x00030170;
// Core BMLH2 Part1
const unsigned int cm_core_bmlh2_part1 = 0x00030240;
// Core BMLH2 Part2
const unsigned int cm_core_bmlh2_part2 = 0x00030250;
// Core BMLH2 Part3
const unsigned int cm_core_bmlh2_part3 = 0x00030260;
// Core BMLH2 Part4
const unsigned int cm_core_bmlh2_part4 = 0x00030270;
// Core BMLH3 Part1
const unsigned int cm_core_bmlh3_part1 = 0x00030340;
// Core BMLH3 Part2
const unsigned int cm_core_bmlh3_part2 = 0x00030350;
// Core BMLH3 Part3
const unsigned int cm_core_bmlh3_part3 = 0x00030360;
// Core BMLH3 Part4
const unsigned int cm_core_bmlh3_part4 = 0x00030370;
// Core BMLH4 Part1
const unsigned int cm_core_bmlh4_part1 = 0x00030440;
// Core BMLH4 Part2
const unsigned int cm_core_bmlh4_part2 = 0x00030450;
// Core BMLH4 Part3
const unsigned int cm_core_bmlh4_part3 = 0x00030460;
// Core BMLH4 Part4
const unsigned int cm_core_bmlh4_part4 = 0x00030470;
// Core BMLH5 Part1
const unsigned int cm_core_bmlh5_part1 = 0x00030540;
// Core BMLH5 Part2
const unsigned int cm_core_bmlh5_part2 = 0x00030550;
// Core BMLH5 Part3
const unsigned int cm_core_bmlh5_part3 = 0x00030560;
// Core BMLH5 Part4
const unsigned int cm_core_bmlh5_part4 = 0x00030570;
// Core BMLH6 Part1
const unsigned int cm_core_bmlh6_part1 = 0x00030640;
// Core BMLH6 Part2
const unsigned int cm_core_bmlh6_part2 = 0x00030650;
// Core BMLH6 Part3
const unsigned int cm_core_bmlh6_part3 = 0x00030660;
// Core BMLH6 Part4
const unsigned int cm_core_bmlh6_part4 = 0x00030670;
// Core BMLH7 Part1
const unsigned int cm_core_bmlh7_part1 = 0x00030740;
// Core BMLH7 Part2
const unsigned int cm_core_bmlh7_part2 = 0x00030750;
// Core BMLH7 Part3
const unsigned int cm_core_bmlh7_part3 = 0x00030760;
// Core BMLH7 Part4
const unsigned int cm_core_bmlh7_part4 = 0x00030770;
// Core BMLL0 Part1
const unsigned int cm_core_bmll0_part1 = 0x00030000;
// Core BMLL0 Part2
const unsigned int cm_core_bmll0_part2 = 0x00030010;
// Core BMLL0 Part3
const unsigned int cm_core_bmll0_part3 = 0x00030020;
// Core BMLL0 Part4
const unsigned int cm_core_bmll0_part4 = 0x00030030;
// Core BMLL1 Part1
const unsigned int cm_core_bmll1_part1 = 0x00030100;
// Core BMLL1 Part2
const unsigned int cm_core_bmll1_part2 = 0x00030110;
// Core BMLL1 Part3
const unsigned int cm_core_bmll1_part3 = 0x00030120;
// Core BMLL1 Part4
const unsigned int cm_core_bmll1_part4 = 0x00030130;
// Core BMLL2 Part1
const unsigned int cm_core_bmll2_part1 = 0x00030200;
// Core BMLL2 Part2
const unsigned int cm_core_bmll2_part2 = 0x00030210;
// Core BMLL2 Part3
const unsigned int cm_core_bmll2_part3 = 0x00030220;
// Core BMLL2 Part4
const unsigned int cm_core_bmll2_part4 = 0x00030230;
// Core BMLL3 Part1
const unsigned int cm_core_bmll3_part1 = 0x00030300;
// Core BMLL3 Part2
const unsigned int cm_core_bmll3_part2 = 0x00030310;
// Core BMLL3 Part3
const unsigned int cm_core_bmll3_part3 = 0x00030320;
// Core BMLL3 Part4
const unsigned int cm_core_bmll3_part4 = 0x00030330;
// Core BMLL4 Part1
const unsigned int cm_core_bmll4_part1 = 0x00030400;
// Core BMLL4 Part2
const unsigned int cm_core_bmll4_part2 = 0x00030410;
// Core BMLL4 Part3
const unsigned int cm_core_bmll4_part3 = 0x00030420;
// Core BMLL4 Part4
const unsigned int cm_core_bmll4_part4 = 0x00030430;
// Core BMLL5 Part1
const unsigned int cm_core_bmll5_part1 = 0x00030500;
// Core BMLL5 Part2
const unsigned int cm_core_bmll5_part2 = 0x00030510;
// Core BMLL5 Part3
const unsigned int cm_core_bmll5_part3 = 0x00030520;
// Core BMLL5 Part4
const unsigned int cm_core_bmll5_part4 = 0x00030530;
// Core BMLL6 Part1
const unsigned int cm_core_bmll6_part1 = 0x00030600;
// Core BMLL6 Part2
const unsigned int cm_core_bmll6_part2 = 0x00030610;
// Core BMLL6 Part3
const unsigned int cm_core_bmll6_part3 = 0x00030620;
// Core BMLL6 Part4
const unsigned int cm_core_bmll6_part4 = 0x00030630;
// Core BMLL7 Part1
const unsigned int cm_core_bmll7_part1 = 0x00030700;
// Core BMLL7 Part2
const unsigned int cm_core_bmll7_part2 = 0x00030710;
// Core BMLL7 Part3
const unsigned int cm_core_bmll7_part3 = 0x00030720;
// Core BMLL7 Part4
const unsigned int cm_core_bmll7_part4 = 0x00030730;
// Core Control
const unsigned int cm_core_control = 0x00038000;
// Core CR1
const unsigned int cm_core_cr1 = 0x00032D80;
// Core CR2
const unsigned int cm_core_cr2 = 0x00032D90;
// Core CR3
const unsigned int cm_core_cr3 = 0x00032DA0;
// Core DC0
const unsigned int cm_core_dc0 = 0x00032B80;
// Core DC1
const unsigned int cm_core_dc1 = 0x00032B90;
// Core DC2
const unsigned int cm_core_dc2 = 0x00032BA0;
// Core DC3
const unsigned int cm_core_dc3 = 0x00032BB0;
// Core DC4
const unsigned int cm_core_dc4 = 0x00032BC0;
// Core DC5
const unsigned int cm_core_dc5 = 0x00032BD0;
// Core DC6
const unsigned int cm_core_dc6 = 0x00032BE0;
// Core DC7
const unsigned int cm_core_dc7 = 0x00032BF0;
// Core DJ0
const unsigned int cm_core_dj0 = 0x00032B00;
// Core DJ1
const unsigned int cm_core_dj1 = 0x00032B10;
// Core DJ2
const unsigned int cm_core_dj2 = 0x00032B20;
// Core DJ3
const unsigned int cm_core_dj3 = 0x00032B30;
// Core DJ4
const unsigned int cm_core_dj4 = 0x00032B40;
// Core DJ5
const unsigned int cm_core_dj5 = 0x00032B50;
// Core DJ6
const unsigned int cm_core_dj6 = 0x00032B60;
// Core DJ7
const unsigned int cm_core_dj7 = 0x00032B70;
// Core DN0
const unsigned int cm_core_dn0 = 0x00032A80;
// Core DN1
const unsigned int cm_core_dn1 = 0x00032A90;
// Core DN2
const unsigned int cm_core_dn2 = 0x00032AA0;
// Core DN3
const unsigned int cm_core_dn3 = 0x00032AB0;
// Core DN4
const unsigned int cm_core_dn4 = 0x00032AC0;
// Core DN5
const unsigned int cm_core_dn5 = 0x00032AD0;
// Core DN6
const unsigned int cm_core_dn6 = 0x00032AE0;
// Core DN7
const unsigned int cm_core_dn7 = 0x00032AF0;
// Core EG0
const unsigned int cm_core_eg0 = 0x00032600;
// Core EG1
const unsigned int cm_core_eg1 = 0x00032610;
// Core EG10
const unsigned int cm_core_eg10 = 0x000326A0;
// Core EG11
const unsigned int cm_core_eg11 = 0x000326B0;
// Core EG2
const unsigned int cm_core_eg2 = 0x00032620;
// Core EG3
const unsigned int cm_core_eg3 = 0x00032630;
// Core EG4
const unsigned int cm_core_eg4 = 0x00032640;
// Core EG5
const unsigned int cm_core_eg5 = 0x00032650;
// Core EG6
const unsigned int cm_core_eg6 = 0x00032660;
// Core EG7
const unsigned int cm_core_eg7 = 0x00032670;
// Core EG8
const unsigned int cm_core_eg8 = 0x00032680;
// Core EG9
const unsigned int cm_core_eg9 = 0x00032690;
// Core F0
const unsigned int cm_core_f0 = 0x00032700;
// Core F1
const unsigned int cm_core_f1 = 0x00032710;
// Core F10
const unsigned int cm_core_f10 = 0x000327A0;
// Core F11
const unsigned int cm_core_f11 = 0x000327B0;
// Core F2
const unsigned int cm_core_f2 = 0x00032720;
// Core F3
const unsigned int cm_core_f3 = 0x00032730;
// Core F4
const unsigned int cm_core_f4 = 0x00032740;
// Core F5
const unsigned int cm_core_f5 = 0x00032750;
// Core F6
const unsigned int cm_core_f6 = 0x00032760;
// Core F7
const unsigned int cm_core_f7 = 0x00032770;
// Core F8
const unsigned int cm_core_f8 = 0x00032780;
// Core F9
const unsigned int cm_core_f9 = 0x00032790;
// Core FC
const unsigned int cm_core_fc = 0x00032D10;
// Core FIFOXTRA Part1
const unsigned int cm_core_fifoxtra_part1 = 0x00032580;
// Core FIFOXTRA Part2
const unsigned int cm_core_fifoxtra_part2 = 0x00032590;
// Core FIFOXTRA Part3
const unsigned int cm_core_fifoxtra_part3 = 0x000325A0;
// Core FIFOXTRA Part4
const unsigned int cm_core_fifoxtra_part4 = 0x000325B0;
// Core LC
const unsigned int cm_core_lc = 0x00032D60;
// Core LCI
const unsigned int cm_core_lci = 0x00032D70;
// Core LDFIFOH0 Part1
const unsigned int cm_core_ldfifoh0_part1 = 0x00032440;
// Core LDFIFOH0 Part2
const unsigned int cm_core_ldfifoh0_part2 = 0x00032450;
// Core LDFIFOH0 Part3
const unsigned int cm_core_ldfifoh0_part3 = 0x00032460;
// Core LDFIFOH0 Part4
const unsigned int cm_core_ldfifoh0_part4 = 0x00032470;
// Core LDFIFOH1 Part1
const unsigned int cm_core_ldfifoh1_part1 = 0x000324C0;
// Core LDFIFOH1 Part2
const unsigned int cm_core_ldfifoh1_part2 = 0x000324D0;
// Core LDFIFOH1 Part3
const unsigned int cm_core_ldfifoh1_part3 = 0x000324E0;
// Core LDFIFOH1 Part4
const unsigned int cm_core_ldfifoh1_part4 = 0x000324F0;
// Core LDFIFOL0 Part1
const unsigned int cm_core_ldfifol0_part1 = 0x00032400;
// Core LDFIFOL0 Part2
const unsigned int cm_core_ldfifol0_part2 = 0x00032410;
// Core LDFIFOL0 Part3
const unsigned int cm_core_ldfifol0_part3 = 0x00032420;
// Core LDFIFOL0 Part4
const unsigned int cm_core_ldfifol0_part4 = 0x00032430;
// Core LDFIFOL1 Part1
const unsigned int cm_core_ldfifol1_part1 = 0x00032480;
// Core LDFIFOL1 Part2
const unsigned int cm_core_ldfifol1_part2 = 0x00032490;
// Core LDFIFOL1 Part3
const unsigned int cm_core_ldfifol1_part3 = 0x000324A0;
// Core LDFIFOL1 Part4
const unsigned int cm_core_ldfifol1_part4 = 0x000324B0;
// Core LE
const unsigned int cm_core_le = 0x00032D50;
// Core LR
const unsigned int cm_core_lr = 0x00032D30;
// Core LS
const unsigned int cm_core_ls = 0x00032D40;
// Core M0
const unsigned int cm_core_m0 = 0x00032A00;
// Core M1
const unsigned int cm_core_m1 = 0x00032A10;
// Core M2
const unsigned int cm_core_m2 = 0x00032A20;
// Core M3
const unsigned int cm_core_m3 = 0x00032A30;
// Core M4
const unsigned int cm_core_m4 = 0x00032A40;
// Core M5
const unsigned int cm_core_m5 = 0x00032A50;
// Core M6
const unsigned int cm_core_m6 = 0x00032A60;
// Core M7
const unsigned int cm_core_m7 = 0x00032A70;
// Core P0
const unsigned int cm_core_p0 = 0x00032C00;
// Core P1
const unsigned int cm_core_p1 = 0x00032C10;
// Core P2
const unsigned int cm_core_p2 = 0x00032C20;
// Core P3
const unsigned int cm_core_p3 = 0x00032C30;
// Core P4
const unsigned int cm_core_p4 = 0x00032C40;
// Core P5
const unsigned int cm_core_p5 = 0x00032C50;
// Core P6
const unsigned int cm_core_p6 = 0x00032C60;
// Core P7
const unsigned int cm_core_p7 = 0x00032C70;
// Core PC
const unsigned int cm_core_pc = 0x00032D00;
const unsigned int cm_program_counter = 0x00032D00;
// Core Processor Bus
const unsigned int cm_core_processor_bus = 0x00038038;
// Core R0
const unsigned int cm_core_r0 = 0x00032800;
// Core R1
const unsigned int cm_core_r1 = 0x00032810;
// Core R10
const unsigned int cm_core_r10 = 0x000328A0;
// Core R11
const unsigned int cm_core_r11 = 0x000328B0;
// Core R12
const unsigned int cm_core_r12 = 0x000328C0;
// Core R13
const unsigned int cm_core_r13 = 0x000328D0;
// Core R14
const unsigned int cm_core_r14 = 0x000328E0;
// Core R15
const unsigned int cm_core_r15 = 0x000328F0;
// Core R16
const unsigned int cm_core_r16 = 0x00032900;
// Core R17
const unsigned int cm_core_r17 = 0x00032910;
// Core R18
const unsigned int cm_core_r18 = 0x00032920;
// Core R19
const unsigned int cm_core_r19 = 0x00032930;
// Core R2
const unsigned int cm_core_r2 = 0x00032820;
// Core R20
const unsigned int cm_core_r20 = 0x00032940;
// Core R21
const unsigned int cm_core_r21 = 0x00032950;
// Core R22
const unsigned int cm_core_r22 = 0x00032960;
// Core R23
const unsigned int cm_core_r23 = 0x00032970;
// Core R24
const unsigned int cm_core_r24 = 0x00032980;
// Core R25
const unsigned int cm_core_r25 = 0x00032990;
// Core R26
const unsigned int cm_core_r26 = 0x000329A0;
// Core R27
const unsigned int cm_core_r27 = 0x000329B0;
// Core R28
const unsigned int cm_core_r28 = 0x000329C0;
// Core R29
const unsigned int cm_core_r29 = 0x000329D0;
// Core R3
const unsigned int cm_core_r3 = 0x00032830;
// Core R30
const unsigned int cm_core_r30 = 0x000329E0;
// Core R31
const unsigned int cm_core_r31 = 0x000329F0;
// Core R4
const unsigned int cm_core_r4 = 0x00032840;
// Core R5
const unsigned int cm_core_r5 = 0x00032850;
// Core R6
const unsigned int cm_core_r6 = 0x00032860;
// Core R7
const unsigned int cm_core_r7 = 0x00032870;
// Core R8
const unsigned int cm_core_r8 = 0x00032880;
// Core R9
const unsigned int cm_core_r9 = 0x00032890;
// Core Reset Defeature
const unsigned int cm_core_reset_defeature = 0x00060030;
// Core S0
const unsigned int cm_core_s0 = 0x00032C80;
// Core S1
const unsigned int cm_core_s1 = 0x00032C90;
// Core S2
const unsigned int cm_core_s2 = 0x00032CA0;
// Core S3
const unsigned int cm_core_s3 = 0x00032CB0;
// Core SP
const unsigned int cm_core_sp = 0x00032D20;
// Core SR1
const unsigned int cm_core_sr1 = 0x00032DC0;
// Core SR2
const unsigned int cm_core_sr2 = 0x00032DD0;
// Core Status
const unsigned int cm_core_status = 0x00038004;
// Core STFIFOH Part1
const unsigned int cm_core_stfifoh_part1 = 0x00032540;
// Core STFIFOH Part2
const unsigned int cm_core_stfifoh_part2 = 0x00032550;
// Core STFIFOH Part3
const unsigned int cm_core_stfifoh_part3 = 0x00032560;
// Core STFIFOH Part4
const unsigned int cm_core_stfifoh_part4 = 0x00032570;
// Core STFIFOL Part1
const unsigned int cm_core_stfifol_part1 = 0x00032500;
// Core STFIFOL Part2
const unsigned int cm_core_stfifol_part2 = 0x00032510;
// Core STFIFOL Part3
const unsigned int cm_core_stfifol_part3 = 0x00032520;
// Core STFIFOL Part4
const unsigned int cm_core_stfifol_part4 = 0x00032530;
// Core X0 Part1
const unsigned int cm_core_x0_part1 = 0x00031800;
// Core X0 Part2
const unsigned int cm_core_x0_part2 = 0x00031810;
// Core X0 Part3
const unsigned int cm_core_x0_part3 = 0x00031820;
// Core X0 Part4
const unsigned int cm_core_x0_part4 = 0x00031830;
// Core X10 Part1
const unsigned int cm_core_x10_part1 = 0x00031A80;
// Core X10 Part2
const unsigned int cm_core_x10_part2 = 0x00031A90;
// Core X10 Part3
const unsigned int cm_core_x10_part3 = 0x00031AA0;
// Core X10 Part4
const unsigned int cm_core_x10_part4 = 0x00031AB0;
// Core X11 Part1
const unsigned int cm_core_x11_part1 = 0x00031AC0;
// Core X11 Part2
const unsigned int cm_core_x11_part2 = 0x00031AD0;
// Core X11 Part3
const unsigned int cm_core_x11_part3 = 0x00031AE0;
// Core X11 Part4
const unsigned int cm_core_x11_part4 = 0x00031AF0;
// Core X1 Part1
const unsigned int cm_core_x1_part1 = 0x00031840;
// Core X1 Part2
const unsigned int cm_core_x1_part2 = 0x00031850;
// Core X1 Part3
const unsigned int cm_core_x1_part3 = 0x00031860;
// Core X1 Part4
const unsigned int cm_core_x1_part4 = 0x00031870;
// Core X2 Part1
const unsigned int cm_core_x2_part1 = 0x00031880;
// Core X2 Part2
const unsigned int cm_core_x2_part2 = 0x00031890;
// Core X2 Part3
const unsigned int cm_core_x2_part3 = 0x000318A0;
// Core X2 Part4
const unsigned int cm_core_x2_part4 = 0x000318B0;
// Core X3 Part1
const unsigned int cm_core_x3_part1 = 0x000318C0;
// Core X3 Part2
const unsigned int cm_core_x3_part2 = 0x000318D0;
// Core X3 Part3
const unsigned int cm_core_x3_part3 = 0x000318E0;
// Core X3 Part4
const unsigned int cm_core_x3_part4 = 0x000318F0;
// Core X4 Part1
const unsigned int cm_core_x4_part1 = 0x00031900;
// Core X4 Part2
const unsigned int cm_core_x4_part2 = 0x00031910;
// Core X4 Part3
const unsigned int cm_core_x4_part3 = 0x00031920;
// Core X4 Part4
const unsigned int cm_core_x4_part4 = 0x00031930;
// Core X5 Part1
const unsigned int cm_core_x5_part1 = 0x00031940;
// Core X5 Part2
const unsigned int cm_core_x5_part2 = 0x00031950;
// Core X5 Part3
const unsigned int cm_core_x5_part3 = 0x00031960;
// Core X5 Part4
const unsigned int cm_core_x5_part4 = 0x00031970;
// Core X6 Part1
const unsigned int cm_core_x6_part1 = 0x00031980;
// Core X6 Part2
const unsigned int cm_core_x6_part2 = 0x00031990;
// Core X6 Part3
const unsigned int cm_core_x6_part3 = 0x000319A0;
// Core X6 Part4
const unsigned int cm_core_x6_part4 = 0x000319B0;
// Core X7 Part1
const unsigned int cm_core_x7_part1 = 0x000319C0;
// Core X7 Part2
const unsigned int cm_core_x7_part2 = 0x000319D0;
// Core X7 Part3
const unsigned int cm_core_x7_part3 = 0x000319E0;
// Core X7 Part4
const unsigned int cm_core_x7_part4 = 0x000319F0;
// Core X8 Part1
const unsigned int cm_core_x8_part1 = 0x00031A00;
// Core X8 Part2
const unsigned int cm_core_x8_part2 = 0x00031A10;
// Core X8 Part3
const unsigned int cm_core_x8_part3 = 0x00031A20;
// Core X8 Part4
const unsigned int cm_core_x8_part4 = 0x00031A30;
// Core X9 Part1
const unsigned int cm_core_x9_part1 = 0x00031A40;
// Core X9 Part2
const unsigned int cm_core_x9_part2 = 0x00031A50;
// Core X9 Part3
const unsigned int cm_core_x9_part3 = 0x00031A60;
// Core X9 Part4
const unsigned int cm_core_x9_part4 = 0x00031A70;
// CSSD Trigger
const unsigned int cm_cssd_trigger = 0x00036040;
// Debug Control0
const unsigned int cm_debug_control0 = 0x00038010;
// Debug Control1
const unsigned int cm_debug_control1 = 0x00038014;
// Debug Control2
const unsigned int cm_debug_control2 = 0x00038018;
// Debug Status
const unsigned int cm_debug_status = 0x0003801C;
// ECC Control
const unsigned int cm_ecc_control = 0x00038100;
// ECC Failing Address
const unsigned int cm_ecc_failing_address = 0x00038120;
// ECC Scrubbing Event
const unsigned int cm_ecc_scrubbing_event = 0x00038110;
// Edge Detection event control
const unsigned int cm_edge_detection_event_control = 0x00034408;
// Enable Events
const unsigned int cm_enable_events = 0x00038008;
// Error Halt Control
const unsigned int cm_error_halt_control = 0x00038030;
// Error Halt Event
const unsigned int cm_error_halt_event = 0x00038034;
// Event Broadcast0
const unsigned int cm_event_broadcast0 = 0x00034010;
// Event Broadcast1
const unsigned int cm_event_broadcast1 = 0x00034014;
// Event Broadcast10
const unsigned int cm_event_broadcast10 = 0x00034038;
// Event Broadcast11
const unsigned int cm_event_broadcast11 = 0x0003403C;
// Event Broadcast12
const unsigned int cm_event_broadcast12 = 0x00034040;
// Event Broadcast13
const unsigned int cm_event_broadcast13 = 0x00034044;
// Event Broadcast14
const unsigned int cm_event_broadcast14 = 0x00034048;
// Event Broadcast15
const unsigned int cm_event_broadcast15 = 0x0003404C;
// Event Broadcast2
const unsigned int cm_event_broadcast2 = 0x00034018;
// Event Broadcast3
const unsigned int cm_event_broadcast3 = 0x0003401C;
// Event Broadcast4
const unsigned int cm_event_broadcast4 = 0x00034020;
// Event Broadcast5
const unsigned int cm_event_broadcast5 = 0x00034024;
// Event Broadcast6
const unsigned int cm_event_broadcast6 = 0x00034028;
// Event Broadcast7
const unsigned int cm_event_broadcast7 = 0x0003402C;
// Event Broadcast8
const unsigned int cm_event_broadcast8 = 0x00034030;
// Event Broadcast9
const unsigned int cm_event_broadcast9 = 0x00034034;
// Event Broadcast Block East Clr
const unsigned int cm_event_broadcast_block_east_clr = 0x00034084;
// Event Broadcast Block East Set
const unsigned int cm_event_broadcast_block_east_set = 0x00034080;
// Event Broadcast Block East Value
const unsigned int cm_event_broadcast_block_east_value = 0x00034088;
// Event Broadcast Block North Clr
const unsigned int cm_event_broadcast_block_north_clr = 0x00034074;
// Event Broadcast Block North Set
const unsigned int cm_event_broadcast_block_north_set = 0x00034070;
// Event Broadcast Block North Value
const unsigned int cm_event_broadcast_block_north_value = 0x00034078;
// Event Broadcast Block South Clr
const unsigned int cm_event_broadcast_block_south_clr = 0x00034054;
// Event Broadcast Block South Set
const unsigned int cm_event_broadcast_block_south_set = 0x00034050;
// Event Broadcast Block South Value
const unsigned int cm_event_broadcast_block_south_value = 0x00034058;
// Event Broadcast Block West Clr
const unsigned int cm_event_broadcast_block_west_clr = 0x00034064;
// Event Broadcast Block West Set
const unsigned int cm_event_broadcast_block_west_set = 0x00034060;
// Event Broadcast Block West Value
const unsigned int cm_event_broadcast_block_west_value = 0x00034068;
// Event Generate
const unsigned int cm_event_generate = 0x00034008;
// Event Group 0 Enable
const unsigned int cm_event_group_0_enable = 0x00034500;
// Event Group Broadcast Enable
const unsigned int cm_event_group_broadcast_enable = 0x0003451C;
// Event Group Core Program Flow Enable
const unsigned int cm_event_group_core_program_flow_enable = 0x0003450C;
// Event Group Core Stall Enable
const unsigned int cm_event_group_core_stall_enable = 0x00034508;
// Event Group Errors0 Enable
const unsigned int cm_event_group_errors0_enable = 0x00034510;
// Event Group Errors1 Enable
const unsigned int cm_event_group_errors1_enable = 0x00034514;
// Event Group PC Enable
const unsigned int cm_event_group_pc_enable = 0x00034504;
// Event Group Stream Switch Enable
const unsigned int cm_event_group_stream_switch_enable = 0x00034518;
// Event Group User Event Enable
const unsigned int cm_event_group_user_event_enable = 0x00034520;
// Event Status0
const unsigned int cm_event_status0 = 0x00034200;
// Event Status1
const unsigned int cm_event_status1 = 0x00034204;
// Event Status2
const unsigned int cm_event_status2 = 0x00034208;
// Event Status3
const unsigned int cm_event_status3 = 0x0003420C;
// Memory Control
const unsigned int cm_memory_control = 0x00036070;
// Module Clock Control
const unsigned int cm_module_clock_control = 0x00060000;
// Module Reset Control
const unsigned int cm_module_reset_control = 0x00060010;
// PC Event0
const unsigned int cm_pc_event0 = 0x00038020;
// PC Event1
const unsigned int cm_pc_event1 = 0x00038024;
// PC Event2
const unsigned int cm_pc_event2 = 0x00038028;
// PC Event3
const unsigned int cm_pc_event3 = 0x0003802C;
// Performance Control0
const unsigned int cm_performance_control0 = 0x00037500;
// Performance Control1
const unsigned int cm_performance_control1 = 0x00037504;
// Performance Control2
const unsigned int cm_performance_control2 = 0x00037508;
// Performance Counter0
const unsigned int cm_performance_counter0 = 0x00037520;
// Performance Counter0 Event Value
const unsigned int cm_performance_counter0_event_value = 0x00037580;
// Performance Counter1
const unsigned int cm_performance_counter1 = 0x00037524;
// Performance Counter1 Event Value
const unsigned int cm_performance_counter1_event_value = 0x00037584;
// Performance Counter2
const unsigned int cm_performance_counter2 = 0x00037528;
// Performance Counter2 Event Value
const unsigned int cm_performance_counter2_event_value = 0x00037588;
// Performance Counter3
const unsigned int cm_performance_counter3 = 0x0003752C;
// Performance Counter3 Event Value
const unsigned int cm_performance_counter3_event_value = 0x0003758C;
// Reset Event
const unsigned int cm_reset_event = 0x0003800C;
// Spare Reg
const unsigned int cm_spare_reg = 0x00060104;
// Spare Reg Privileged
const unsigned int cm_spare_reg_privileged = 0x00060100;
// Stream Switch Adaptive Clock Gate Abort Period
const unsigned int cm_stream_switch_adaptive_clock_gate_abort_period = 0x0003FF38;
// Stream Switch Adaptive Clock Gate Status
const unsigned int cm_stream_switch_adaptive_clock_gate_status = 0x0003FF34;
// Stream Switch Deterministic Merge arb0 ctrl
const unsigned int cm_stream_switch_deterministic_merge_arb0_ctrl = 0x0003F808;
// Stream Switch Deterministic Merge arb0 slave0 1
const unsigned int cm_stream_switch_deterministic_merge_arb0_slave0_1 = 0x0003F800;
// Stream Switch Deterministic Merge arb0 slave2 3
const unsigned int cm_stream_switch_deterministic_merge_arb0_slave2_3 = 0x0003F804;
// Stream Switch Deterministic Merge arb1 ctrl
const unsigned int cm_stream_switch_deterministic_merge_arb1_ctrl = 0x0003F818;
// Stream Switch Deterministic Merge arb1 slave0 1
const unsigned int cm_stream_switch_deterministic_merge_arb1_slave0_1 = 0x0003F810;
// Stream Switch Deterministic Merge arb1 slave2 3
const unsigned int cm_stream_switch_deterministic_merge_arb1_slave2_3 = 0x0003F814;
// Stream Switch Event Port Selection 0
const unsigned int cm_stream_switch_event_port_selection_0 = 0x0003FF00;
// Stream Switch Event Port Selection 1
const unsigned int cm_stream_switch_event_port_selection_1 = 0x0003FF04;
// Stream Switch Master Config AIE Core0
const unsigned int cm_stream_switch_master_config_aie_core0 = 0x0003F000;
// Stream Switch Master Config DMA0
const unsigned int cm_stream_switch_master_config_dma0 = 0x0003F004;
// Stream Switch Master Config DMA1
const unsigned int cm_stream_switch_master_config_dma1 = 0x0003F008;
// Stream Switch Master Config East0
const unsigned int cm_stream_switch_master_config_east0 = 0x0003F04C;
// Stream Switch Master Config East1
const unsigned int cm_stream_switch_master_config_east1 = 0x0003F050;
// Stream Switch Master Config East2
const unsigned int cm_stream_switch_master_config_east2 = 0x0003F054;
// Stream Switch Master Config East3
const unsigned int cm_stream_switch_master_config_east3 = 0x0003F058;
// Stream Switch Master Config FIFO0
const unsigned int cm_stream_switch_master_config_fifo0 = 0x0003F010;
// Stream Switch Master Config North0
const unsigned int cm_stream_switch_master_config_north0 = 0x0003F034;
// Stream Switch Master Config North1
const unsigned int cm_stream_switch_master_config_north1 = 0x0003F038;
// Stream Switch Master Config North2
const unsigned int cm_stream_switch_master_config_north2 = 0x0003F03C;
// Stream Switch Master Config North3
const unsigned int cm_stream_switch_master_config_north3 = 0x0003F040;
// Stream Switch Master Config North4
const unsigned int cm_stream_switch_master_config_north4 = 0x0003F044;
// Stream Switch Master Config North5
const unsigned int cm_stream_switch_master_config_north5 = 0x0003F048;
// Stream Switch Master Config South0
const unsigned int cm_stream_switch_master_config_south0 = 0x0003F014;
// Stream Switch Master Config South1
const unsigned int cm_stream_switch_master_config_south1 = 0x0003F018;
// Stream Switch Master Config South2
const unsigned int cm_stream_switch_master_config_south2 = 0x0003F01C;
// Stream Switch Master Config South3
const unsigned int cm_stream_switch_master_config_south3 = 0x0003F020;
// Stream Switch Master Config Tile Ctrl
const unsigned int cm_stream_switch_master_config_tile_ctrl = 0x0003F00C;
// Stream Switch Master Config West0
const unsigned int cm_stream_switch_master_config_west0 = 0x0003F024;
// Stream Switch Master Config West1
const unsigned int cm_stream_switch_master_config_west1 = 0x0003F028;
// Stream Switch Master Config West2
const unsigned int cm_stream_switch_master_config_west2 = 0x0003F02C;
// Stream Switch Master Config West3
const unsigned int cm_stream_switch_master_config_west3 = 0x0003F030;
// Stream Switch Parity Injection
const unsigned int cm_stream_switch_parity_injection = 0x0003FF20;
// Stream Switch Parity Status
const unsigned int cm_stream_switch_parity_status = 0x0003FF10;
// Stream Switch Slave AIE Core0 Slot0
const unsigned int cm_stream_switch_slave_aie_core0_slot0 = 0x0003F200;
// Stream Switch Slave AIE Core0 Slot1
const unsigned int cm_stream_switch_slave_aie_core0_slot1 = 0x0003F204;
// Stream Switch Slave AIE Core0 Slot2
const unsigned int cm_stream_switch_slave_aie_core0_slot2 = 0x0003F208;
// Stream Switch Slave AIE Core0 Slot3
const unsigned int cm_stream_switch_slave_aie_core0_slot3 = 0x0003F20C;
// Stream Switch Slave AIE Trace Slot0
const unsigned int cm_stream_switch_slave_aie_trace_slot0 = 0x0003F370;
// Stream Switch Slave AIE Trace Slot1
const unsigned int cm_stream_switch_slave_aie_trace_slot1 = 0x0003F374;
// Stream Switch Slave AIE Trace Slot2
const unsigned int cm_stream_switch_slave_aie_trace_slot2 = 0x0003F378;
// Stream Switch Slave AIE Trace Slot3
const unsigned int cm_stream_switch_slave_aie_trace_slot3 = 0x0003F37C;
// Stream Switch Slave Config AIE Core0
const unsigned int cm_stream_switch_slave_config_aie_core0 = 0x0003F100;
// Stream Switch Slave Config AIE Trace
const unsigned int cm_stream_switch_slave_config_aie_trace = 0x0003F15C;
// Stream Switch Slave Config DMA 0
const unsigned int cm_stream_switch_slave_config_dma_0 = 0x0003F104;
// Stream Switch Slave Config DMA 1
const unsigned int cm_stream_switch_slave_config_dma_1 = 0x0003F108;
// Stream Switch Slave Config East 0
const unsigned int cm_stream_switch_slave_config_east_0 = 0x0003F14C;
// Stream Switch Slave Config East 1
const unsigned int cm_stream_switch_slave_config_east_1 = 0x0003F150;
// Stream Switch Slave Config East 2
const unsigned int cm_stream_switch_slave_config_east_2 = 0x0003F154;
// Stream Switch Slave Config East 3
const unsigned int cm_stream_switch_slave_config_east_3 = 0x0003F158;
// Stream Switch Slave Config FIFO 0
const unsigned int cm_stream_switch_slave_config_fifo_0 = 0x0003F110;
// Stream Switch Slave Config Mem Trace
const unsigned int cm_stream_switch_slave_config_mem_trace = 0x0003F160;
// Stream Switch Slave Config North 0
const unsigned int cm_stream_switch_slave_config_north_0 = 0x0003F13C;
// Stream Switch Slave Config North 1
const unsigned int cm_stream_switch_slave_config_north_1 = 0x0003F140;
// Stream Switch Slave Config North 2
const unsigned int cm_stream_switch_slave_config_north_2 = 0x0003F144;
// Stream Switch Slave Config North 3
const unsigned int cm_stream_switch_slave_config_north_3 = 0x0003F148;
// Stream Switch Slave Config South 0
const unsigned int cm_stream_switch_slave_config_south_0 = 0x0003F114;
// Stream Switch Slave Config South 1
const unsigned int cm_stream_switch_slave_config_south_1 = 0x0003F118;
// Stream Switch Slave Config South 2
const unsigned int cm_stream_switch_slave_config_south_2 = 0x0003F11C;
// Stream Switch Slave Config South 3
const unsigned int cm_stream_switch_slave_config_south_3 = 0x0003F120;
// Stream Switch Slave Config South 4
const unsigned int cm_stream_switch_slave_config_south_4 = 0x0003F124;
// Stream Switch Slave Config South 5
const unsigned int cm_stream_switch_slave_config_south_5 = 0x0003F128;
// Stream Switch Slave Config Tile Ctrl
const unsigned int cm_stream_switch_slave_config_tile_ctrl = 0x0003F10C;
// Stream Switch Slave Config West 0
const unsigned int cm_stream_switch_slave_config_west_0 = 0x0003F12C;
// Stream Switch Slave Config West 1
const unsigned int cm_stream_switch_slave_config_west_1 = 0x0003F130;
// Stream Switch Slave Config West 2
const unsigned int cm_stream_switch_slave_config_west_2 = 0x0003F134;
// Stream Switch Slave Config West 3
const unsigned int cm_stream_switch_slave_config_west_3 = 0x0003F138;
// Stream Switch Slave DMA 0 Slot0
const unsigned int cm_stream_switch_slave_dma_0_slot0 = 0x0003F210;
// Stream Switch Slave DMA 0 Slot1
const unsigned int cm_stream_switch_slave_dma_0_slot1 = 0x0003F214;
// Stream Switch Slave DMA 0 Slot2
const unsigned int cm_stream_switch_slave_dma_0_slot2 = 0x0003F218;
// Stream Switch Slave DMA 0 Slot3
const unsigned int cm_stream_switch_slave_dma_0_slot3 = 0x0003F21C;
// Stream Switch Slave DMA 1 Slot0
const unsigned int cm_stream_switch_slave_dma_1_slot0 = 0x0003F220;
// Stream Switch Slave DMA 1 Slot1
const unsigned int cm_stream_switch_slave_dma_1_slot1 = 0x0003F224;
// Stream Switch Slave DMA 1 Slot2
const unsigned int cm_stream_switch_slave_dma_1_slot2 = 0x0003F228;
// Stream Switch Slave DMA 1 Slot3
const unsigned int cm_stream_switch_slave_dma_1_slot3 = 0x0003F22C;
// Stream Switch Slave East 0 Slot0
const unsigned int cm_stream_switch_slave_east_0_slot0 = 0x0003F330;
// Stream Switch Slave East 0 Slot1
const unsigned int cm_stream_switch_slave_east_0_slot1 = 0x0003F334;
// Stream Switch Slave East 0 Slot2
const unsigned int cm_stream_switch_slave_east_0_slot2 = 0x0003F338;
// Stream Switch Slave East 0 Slot3
const unsigned int cm_stream_switch_slave_east_0_slot3 = 0x0003F33C;
// Stream Switch Slave East 1 Slot0
const unsigned int cm_stream_switch_slave_east_1_slot0 = 0x0003F340;
// Stream Switch Slave East 1 Slot1
const unsigned int cm_stream_switch_slave_east_1_slot1 = 0x0003F344;
// Stream Switch Slave East 1 Slot2
const unsigned int cm_stream_switch_slave_east_1_slot2 = 0x0003F348;
// Stream Switch Slave East 1 Slot3
const unsigned int cm_stream_switch_slave_east_1_slot3 = 0x0003F34C;
// Stream Switch Slave East 2 Slot0
const unsigned int cm_stream_switch_slave_east_2_slot0 = 0x0003F350;
// Stream Switch Slave East 2 Slot1
const unsigned int cm_stream_switch_slave_east_2_slot1 = 0x0003F354;
// Stream Switch Slave East 2 Slot2
const unsigned int cm_stream_switch_slave_east_2_slot2 = 0x0003F358;
// Stream Switch Slave East 2 Slot3
const unsigned int cm_stream_switch_slave_east_2_slot3 = 0x0003F35C;
// Stream Switch Slave East 3 Slot0
const unsigned int cm_stream_switch_slave_east_3_slot0 = 0x0003F360;
// Stream Switch Slave East 3 Slot1
const unsigned int cm_stream_switch_slave_east_3_slot1 = 0x0003F364;
// Stream Switch Slave East 3 Slot2
const unsigned int cm_stream_switch_slave_east_3_slot2 = 0x0003F368;
// Stream Switch Slave East 3 Slot3
const unsigned int cm_stream_switch_slave_east_3_slot3 = 0x0003F36C;
// Stream Switch Slave FIFO 0 Slot0
const unsigned int cm_stream_switch_slave_fifo_0_slot0 = 0x0003F240;
// Stream Switch Slave FIFO 0 Slot1
const unsigned int cm_stream_switch_slave_fifo_0_slot1 = 0x0003F244;
// Stream Switch Slave FIFO 0 Slot2
const unsigned int cm_stream_switch_slave_fifo_0_slot2 = 0x0003F248;
// Stream Switch Slave FIFO 0 Slot3
const unsigned int cm_stream_switch_slave_fifo_0_slot3 = 0x0003F24C;
// Stream Switch Slave Mem Trace Slot0
const unsigned int cm_stream_switch_slave_mem_trace_slot0 = 0x0003F380;
// Stream Switch Slave Mem Trace Slot1
const unsigned int cm_stream_switch_slave_mem_trace_slot1 = 0x0003F384;
// Stream Switch Slave Mem Trace Slot2
const unsigned int cm_stream_switch_slave_mem_trace_slot2 = 0x0003F388;
// Stream Switch Slave Mem Trace Slot3
const unsigned int cm_stream_switch_slave_mem_trace_slot3 = 0x0003F38C;
// Stream Switch Slave North 0 Slot0
const unsigned int cm_stream_switch_slave_north_0_slot0 = 0x0003F2F0;
// Stream Switch Slave North 0 Slot1
const unsigned int cm_stream_switch_slave_north_0_slot1 = 0x0003F2F4;
// Stream Switch Slave North 0 Slot2
const unsigned int cm_stream_switch_slave_north_0_slot2 = 0x0003F2F8;
// Stream Switch Slave North 0 Slot3
const unsigned int cm_stream_switch_slave_north_0_slot3 = 0x0003F2FC;
// Stream Switch Slave North 1 Slot0
const unsigned int cm_stream_switch_slave_north_1_slot0 = 0x0003F300;
// Stream Switch Slave North 1 Slot1
const unsigned int cm_stream_switch_slave_north_1_slot1 = 0x0003F304;
// Stream Switch Slave North 1 Slot2
const unsigned int cm_stream_switch_slave_north_1_slot2 = 0x0003F308;
// Stream Switch Slave North 1 Slot3
const unsigned int cm_stream_switch_slave_north_1_slot3 = 0x0003F30C;
// Stream Switch Slave North 2 Slot0
const unsigned int cm_stream_switch_slave_north_2_slot0 = 0x0003F310;
// Stream Switch Slave North 2 Slot1
const unsigned int cm_stream_switch_slave_north_2_slot1 = 0x0003F314;
// Stream Switch Slave North 2 Slot2
const unsigned int cm_stream_switch_slave_north_2_slot2 = 0x0003F318;
// Stream Switch Slave North 2 Slot3
const unsigned int cm_stream_switch_slave_north_2_slot3 = 0x0003F31C;
// Stream Switch Slave North 3 Slot0
const unsigned int cm_stream_switch_slave_north_3_slot0 = 0x0003F320;
// Stream Switch Slave North 3 Slot1
const unsigned int cm_stream_switch_slave_north_3_slot1 = 0x0003F324;
// Stream Switch Slave North 3 Slot2
const unsigned int cm_stream_switch_slave_north_3_slot2 = 0x0003F328;
// Stream Switch Slave North 3 Slot3
const unsigned int cm_stream_switch_slave_north_3_slot3 = 0x0003F32C;
// Stream Switch Slave South 0 Slot0
const unsigned int cm_stream_switch_slave_south_0_slot0 = 0x0003F250;
// Stream Switch Slave South 0 Slot1
const unsigned int cm_stream_switch_slave_south_0_slot1 = 0x0003F254;
// Stream Switch Slave South 0 Slot2
const unsigned int cm_stream_switch_slave_south_0_slot2 = 0x0003F258;
// Stream Switch Slave South 0 Slot3
const unsigned int cm_stream_switch_slave_south_0_slot3 = 0x0003F25C;
// Stream Switch Slave South 1 Slot0
const unsigned int cm_stream_switch_slave_south_1_slot0 = 0x0003F260;
// Stream Switch Slave South 1 Slot1
const unsigned int cm_stream_switch_slave_south_1_slot1 = 0x0003F264;
// Stream Switch Slave South 1 Slot2
const unsigned int cm_stream_switch_slave_south_1_slot2 = 0x0003F268;
// Stream Switch Slave South 1 Slot3
const unsigned int cm_stream_switch_slave_south_1_slot3 = 0x0003F26C;
// Stream Switch Slave South 2 Slot0
const unsigned int cm_stream_switch_slave_south_2_slot0 = 0x0003F270;
// Stream Switch Slave South 2 Slot1
const unsigned int cm_stream_switch_slave_south_2_slot1 = 0x0003F274;
// Stream Switch Slave South 2 Slot2
const unsigned int cm_stream_switch_slave_south_2_slot2 = 0x0003F278;
// Stream Switch Slave South 2 Slot3
const unsigned int cm_stream_switch_slave_south_2_slot3 = 0x0003F27C;
// Stream Switch Slave South 3 Slot0
const unsigned int cm_stream_switch_slave_south_3_slot0 = 0x0003F280;
// Stream Switch Slave South 3 Slot1
const unsigned int cm_stream_switch_slave_south_3_slot1 = 0x0003F284;
// Stream Switch Slave South 3 Slot2
const unsigned int cm_stream_switch_slave_south_3_slot2 = 0x0003F288;
// Stream Switch Slave South 3 Slot3
const unsigned int cm_stream_switch_slave_south_3_slot3 = 0x0003F28C;
// Stream Switch Slave South 4 Slot0
const unsigned int cm_stream_switch_slave_south_4_slot0 = 0x0003F290;
// Stream Switch Slave South 4 Slot1
const unsigned int cm_stream_switch_slave_south_4_slot1 = 0x0003F294;
// Stream Switch Slave South 4 Slot2
const unsigned int cm_stream_switch_slave_south_4_slot2 = 0x0003F298;
// Stream Switch Slave South 4 Slot3
const unsigned int cm_stream_switch_slave_south_4_slot3 = 0x0003F29C;
// Stream Switch Slave South 5 Slot0
const unsigned int cm_stream_switch_slave_south_5_slot0 = 0x0003F2A0;
// Stream Switch Slave South 5 Slot1
const unsigned int cm_stream_switch_slave_south_5_slot1 = 0x0003F2A4;
// Stream Switch Slave South 5 Slot2
const unsigned int cm_stream_switch_slave_south_5_slot2 = 0x0003F2A8;
// Stream Switch Slave South 5 Slot3
const unsigned int cm_stream_switch_slave_south_5_slot3 = 0x0003F2AC;
// Stream Switch Slave Tile Ctrl Slot0
const unsigned int cm_stream_switch_slave_tile_ctrl_slot0 = 0x0003F230;
// Stream Switch Slave Tile Ctrl Slot1
const unsigned int cm_stream_switch_slave_tile_ctrl_slot1 = 0x0003F234;
// Stream Switch Slave Tile Ctrl Slot2
const unsigned int cm_stream_switch_slave_tile_ctrl_slot2 = 0x0003F238;
// Stream Switch Slave Tile Ctrl Slot3
const unsigned int cm_stream_switch_slave_tile_ctrl_slot3 = 0x0003F23C;
// Stream Switch Slave West 0 Slot0
const unsigned int cm_stream_switch_slave_west_0_slot0 = 0x0003F2B0;
// Stream Switch Slave West 0 Slot1
const unsigned int cm_stream_switch_slave_west_0_slot1 = 0x0003F2B4;
// Stream Switch Slave West 0 Slot2
const unsigned int cm_stream_switch_slave_west_0_slot2 = 0x0003F2B8;
// Stream Switch Slave West 0 Slot3
const unsigned int cm_stream_switch_slave_west_0_slot3 = 0x0003F2BC;
// Stream Switch Slave West 1 Slot0
const unsigned int cm_stream_switch_slave_west_1_slot0 = 0x0003F2C0;
// Stream Switch Slave West 1 Slot1
const unsigned int cm_stream_switch_slave_west_1_slot1 = 0x0003F2C4;
// Stream Switch Slave West 1 Slot2
const unsigned int cm_stream_switch_slave_west_1_slot2 = 0x0003F2C8;
// Stream Switch Slave West 1 Slot3
const unsigned int cm_stream_switch_slave_west_1_slot3 = 0x0003F2CC;
// Stream Switch Slave West 2 Slot0
const unsigned int cm_stream_switch_slave_west_2_slot0 = 0x0003F2D0;
// Stream Switch Slave West 2 Slot1
const unsigned int cm_stream_switch_slave_west_2_slot1 = 0x0003F2D4;
// Stream Switch Slave West 2 Slot2
const unsigned int cm_stream_switch_slave_west_2_slot2 = 0x0003F2D8;
// Stream Switch Slave West 2 Slot3
const unsigned int cm_stream_switch_slave_west_2_slot3 = 0x0003F2DC;
// Stream Switch Slave West 3 Slot0
const unsigned int cm_stream_switch_slave_west_3_slot0 = 0x0003F2E0;
// Stream Switch Slave West 3 Slot1
const unsigned int cm_stream_switch_slave_west_3_slot1 = 0x0003F2E4;
// Stream Switch Slave West 3 Slot2
const unsigned int cm_stream_switch_slave_west_3_slot2 = 0x0003F2E8;
// Stream Switch Slave West 3 Slot3
const unsigned int cm_stream_switch_slave_west_3_slot3 = 0x0003F2EC;
// Tile Control
const unsigned int cm_tile_control = 0x00060020;
// Tile Control Packet Handler Status
const unsigned int cm_tile_control_packet_handler_status = 0x0003FF30;
// Timer Control
const unsigned int cm_timer_control = 0x00034000;
// Timer High
const unsigned int cm_timer_high = 0x000340FC;
// Timer Low
const unsigned int cm_timer_low = 0x000340F8;
// Timer Trig Event High Value
const unsigned int cm_timer_trig_event_high_value = 0x000340F4;
// Timer Trig Event Low Value
const unsigned int cm_timer_trig_event_low_value = 0x000340F0;
// Trace Control0
const unsigned int cm_trace_control0 = 0x000340D0;
// Trace Control1
const unsigned int cm_trace_control1 = 0x000340D4;
// Trace Event0
const unsigned int cm_trace_event0 = 0x000340E0;
// Trace Event1
const unsigned int cm_trace_event1 = 0x000340E4;
// Trace Status
const unsigned int cm_trace_status = 0x000340D8;

// Register definitions for MEM
// ######################################
// Checkbit Error Generation
const unsigned int mem_checkbit_error_generation = 0x00092000;
// Combo event control
const unsigned int mem_combo_event_control = 0x00094404;
// Combo event inputs
const unsigned int mem_combo_event_inputs = 0x00094400;
// CSSD Trigger
const unsigned int mem_cssd_trigger = 0x00096040;
// DMA BD0 0
const unsigned int mem_dma_bd0_0 = 0x000A0000;
// DMA BD0 1
const unsigned int mem_dma_bd0_1 = 0x000A0004;
// DMA BD0 2
const unsigned int mem_dma_bd0_2 = 0x000A0008;
// DMA BD0 3
const unsigned int mem_dma_bd0_3 = 0x000A000C;
// DMA BD0 4
const unsigned int mem_dma_bd0_4 = 0x000A0010;
// DMA BD0 5
const unsigned int mem_dma_bd0_5 = 0x000A0014;
// DMA BD0 6
const unsigned int mem_dma_bd0_6 = 0x000A0018;
// DMA BD0 7
const unsigned int mem_dma_bd0_7 = 0x000A001C;
// DMA BD10 0
const unsigned int mem_dma_bd10_0 = 0x000A0140;
// DMA BD10 1
const unsigned int mem_dma_bd10_1 = 0x000A0144;
// DMA BD10 2
const unsigned int mem_dma_bd10_2 = 0x000A0148;
// DMA BD10 3
const unsigned int mem_dma_bd10_3 = 0x000A014C;
// DMA BD10 4
const unsigned int mem_dma_bd10_4 = 0x000A0150;
// DMA BD10 5
const unsigned int mem_dma_bd10_5 = 0x000A0154;
// DMA BD10 6
const unsigned int mem_dma_bd10_6 = 0x000A0158;
// DMA BD10 7
const unsigned int mem_dma_bd10_7 = 0x000A015C;
// DMA BD11 0
const unsigned int mem_dma_bd11_0 = 0x000A0160;
// DMA BD11 1
const unsigned int mem_dma_bd11_1 = 0x000A0164;
// DMA BD11 2
const unsigned int mem_dma_bd11_2 = 0x000A0168;
// DMA BD11 3
const unsigned int mem_dma_bd11_3 = 0x000A016C;
// DMA BD11 4
const unsigned int mem_dma_bd11_4 = 0x000A0170;
// DMA BD11 5
const unsigned int mem_dma_bd11_5 = 0x000A0174;
// DMA BD11 6
const unsigned int mem_dma_bd11_6 = 0x000A0178;
// DMA BD11 7
const unsigned int mem_dma_bd11_7 = 0x000A017C;
// DMA BD12 0
const unsigned int mem_dma_bd12_0 = 0x000A0180;
// DMA BD12 1
const unsigned int mem_dma_bd12_1 = 0x000A0184;
// DMA BD12 2
const unsigned int mem_dma_bd12_2 = 0x000A0188;
// DMA BD12 3
const unsigned int mem_dma_bd12_3 = 0x000A018C;
// DMA BD12 4
const unsigned int mem_dma_bd12_4 = 0x000A0190;
// DMA BD12 5
const unsigned int mem_dma_bd12_5 = 0x000A0194;
// DMA BD12 6
const unsigned int mem_dma_bd12_6 = 0x000A0198;
// DMA BD12 7
const unsigned int mem_dma_bd12_7 = 0x000A019C;
// DMA BD13 0
const unsigned int mem_dma_bd13_0 = 0x000A01A0;
// DMA BD13 1
const unsigned int mem_dma_bd13_1 = 0x000A01A4;
// DMA BD13 2
const unsigned int mem_dma_bd13_2 = 0x000A01A8;
// DMA BD13 3
const unsigned int mem_dma_bd13_3 = 0x000A01AC;
// DMA BD13 4
const unsigned int mem_dma_bd13_4 = 0x000A01B0;
// DMA BD13 5
const unsigned int mem_dma_bd13_5 = 0x000A01B4;
// DMA BD13 6
const unsigned int mem_dma_bd13_6 = 0x000A01B8;
// DMA BD13 7
const unsigned int mem_dma_bd13_7 = 0x000A01BC;
// DMA BD14 0
const unsigned int mem_dma_bd14_0 = 0x000A01C0;
// DMA BD14 1
const unsigned int mem_dma_bd14_1 = 0x000A01C4;
// DMA BD14 2
const unsigned int mem_dma_bd14_2 = 0x000A01C8;
// DMA BD14 3
const unsigned int mem_dma_bd14_3 = 0x000A01CC;
// DMA BD14 4
const unsigned int mem_dma_bd14_4 = 0x000A01D0;
// DMA BD14 5
const unsigned int mem_dma_bd14_5 = 0x000A01D4;
// DMA BD14 6
const unsigned int mem_dma_bd14_6 = 0x000A01D8;
// DMA BD14 7
const unsigned int mem_dma_bd14_7 = 0x000A01DC;
// DMA BD15 0
const unsigned int mem_dma_bd15_0 = 0x000A01E0;
// DMA BD15 1
const unsigned int mem_dma_bd15_1 = 0x000A01E4;
// DMA BD15 2
const unsigned int mem_dma_bd15_2 = 0x000A01E8;
// DMA BD15 3
const unsigned int mem_dma_bd15_3 = 0x000A01EC;
// DMA BD15 4
const unsigned int mem_dma_bd15_4 = 0x000A01F0;
// DMA BD15 5
const unsigned int mem_dma_bd15_5 = 0x000A01F4;
// DMA BD15 6
const unsigned int mem_dma_bd15_6 = 0x000A01F8;
// DMA BD15 7
const unsigned int mem_dma_bd15_7 = 0x000A01FC;
// DMA BD16 0
const unsigned int mem_dma_bd16_0 = 0x000A0200;
// DMA BD16 1
const unsigned int mem_dma_bd16_1 = 0x000A0204;
// DMA BD16 2
const unsigned int mem_dma_bd16_2 = 0x000A0208;
// DMA BD16 3
const unsigned int mem_dma_bd16_3 = 0x000A020C;
// DMA BD16 4
const unsigned int mem_dma_bd16_4 = 0x000A0210;
// DMA BD16 5
const unsigned int mem_dma_bd16_5 = 0x000A0214;
// DMA BD16 6
const unsigned int mem_dma_bd16_6 = 0x000A0218;
// DMA BD16 7
const unsigned int mem_dma_bd16_7 = 0x000A021C;
// DMA BD17 0
const unsigned int mem_dma_bd17_0 = 0x000A0220;
// DMA BD17 1
const unsigned int mem_dma_bd17_1 = 0x000A0224;
// DMA BD17 2
const unsigned int mem_dma_bd17_2 = 0x000A0228;
// DMA BD17 3
const unsigned int mem_dma_bd17_3 = 0x000A022C;
// DMA BD17 4
const unsigned int mem_dma_bd17_4 = 0x000A0230;
// DMA BD17 5
const unsigned int mem_dma_bd17_5 = 0x000A0234;
// DMA BD17 6
const unsigned int mem_dma_bd17_6 = 0x000A0238;
// DMA BD17 7
const unsigned int mem_dma_bd17_7 = 0x000A023C;
// DMA BD18 0
const unsigned int mem_dma_bd18_0 = 0x000A0240;
// DMA BD18 1
const unsigned int mem_dma_bd18_1 = 0x000A0244;
// DMA BD18 2
const unsigned int mem_dma_bd18_2 = 0x000A0248;
// DMA BD18 3
const unsigned int mem_dma_bd18_3 = 0x000A024C;
// DMA BD18 4
const unsigned int mem_dma_bd18_4 = 0x000A0250;
// DMA BD18 5
const unsigned int mem_dma_bd18_5 = 0x000A0254;
// DMA BD18 6
const unsigned int mem_dma_bd18_6 = 0x000A0258;
// DMA BD18 7
const unsigned int mem_dma_bd18_7 = 0x000A025C;
// DMA BD19 0
const unsigned int mem_dma_bd19_0 = 0x000A0260;
// DMA BD19 1
const unsigned int mem_dma_bd19_1 = 0x000A0264;
// DMA BD19 2
const unsigned int mem_dma_bd19_2 = 0x000A0268;
// DMA BD19 3
const unsigned int mem_dma_bd19_3 = 0x000A026C;
// DMA BD19 4
const unsigned int mem_dma_bd19_4 = 0x000A0270;
// DMA BD19 5
const unsigned int mem_dma_bd19_5 = 0x000A0274;
// DMA BD19 6
const unsigned int mem_dma_bd19_6 = 0x000A0278;
// DMA BD19 7
const unsigned int mem_dma_bd19_7 = 0x000A027C;
// DMA BD1 0
const unsigned int mem_dma_bd1_0 = 0x000A0020;
// DMA BD1 1
const unsigned int mem_dma_bd1_1 = 0x000A0024;
// DMA BD1 2
const unsigned int mem_dma_bd1_2 = 0x000A0028;
// DMA BD1 3
const unsigned int mem_dma_bd1_3 = 0x000A002C;
// DMA BD1 4
const unsigned int mem_dma_bd1_4 = 0x000A0030;
// DMA BD1 5
const unsigned int mem_dma_bd1_5 = 0x000A0034;
// DMA BD1 6
const unsigned int mem_dma_bd1_6 = 0x000A0038;
// DMA BD1 7
const unsigned int mem_dma_bd1_7 = 0x000A003C;
// DMA BD20 0
const unsigned int mem_dma_bd20_0 = 0x000A0280;
// DMA BD20 1
const unsigned int mem_dma_bd20_1 = 0x000A0284;
// DMA BD20 2
const unsigned int mem_dma_bd20_2 = 0x000A0288;
// DMA BD20 3
const unsigned int mem_dma_bd20_3 = 0x000A028C;
// DMA BD20 4
const unsigned int mem_dma_bd20_4 = 0x000A0290;
// DMA BD20 5
const unsigned int mem_dma_bd20_5 = 0x000A0294;
// DMA BD20 6
const unsigned int mem_dma_bd20_6 = 0x000A0298;
// DMA BD20 7
const unsigned int mem_dma_bd20_7 = 0x000A029C;
// DMA BD21 0
const unsigned int mem_dma_bd21_0 = 0x000A02A0;
// DMA BD21 1
const unsigned int mem_dma_bd21_1 = 0x000A02A4;
// DMA BD21 2
const unsigned int mem_dma_bd21_2 = 0x000A02A8;
// DMA BD21 3
const unsigned int mem_dma_bd21_3 = 0x000A02AC;
// DMA BD21 4
const unsigned int mem_dma_bd21_4 = 0x000A02B0;
// DMA BD21 5
const unsigned int mem_dma_bd21_5 = 0x000A02B4;
// DMA BD21 6
const unsigned int mem_dma_bd21_6 = 0x000A02B8;
// DMA BD21 7
const unsigned int mem_dma_bd21_7 = 0x000A02BC;
// DMA BD22 0
const unsigned int mem_dma_bd22_0 = 0x000A02C0;
// DMA BD22 1
const unsigned int mem_dma_bd22_1 = 0x000A02C4;
// DMA BD22 2
const unsigned int mem_dma_bd22_2 = 0x000A02C8;
// DMA BD22 3
const unsigned int mem_dma_bd22_3 = 0x000A02CC;
// DMA BD22 4
const unsigned int mem_dma_bd22_4 = 0x000A02D0;
// DMA BD22 5
const unsigned int mem_dma_bd22_5 = 0x000A02D4;
// DMA BD22 6
const unsigned int mem_dma_bd22_6 = 0x000A02D8;
// DMA BD22 7
const unsigned int mem_dma_bd22_7 = 0x000A02DC;
// DMA BD23 0
const unsigned int mem_dma_bd23_0 = 0x000A02E0;
// DMA BD23 1
const unsigned int mem_dma_bd23_1 = 0x000A02E4;
// DMA BD23 2
const unsigned int mem_dma_bd23_2 = 0x000A02E8;
// DMA BD23 3
const unsigned int mem_dma_bd23_3 = 0x000A02EC;
// DMA BD23 4
const unsigned int mem_dma_bd23_4 = 0x000A02F0;
// DMA BD23 5
const unsigned int mem_dma_bd23_5 = 0x000A02F4;
// DMA BD23 6
const unsigned int mem_dma_bd23_6 = 0x000A02F8;
// DMA BD23 7
const unsigned int mem_dma_bd23_7 = 0x000A02FC;
// DMA BD24 0
const unsigned int mem_dma_bd24_0 = 0x000A0300;
// DMA BD24 1
const unsigned int mem_dma_bd24_1 = 0x000A0304;
// DMA BD24 2
const unsigned int mem_dma_bd24_2 = 0x000A0308;
// DMA BD24 3
const unsigned int mem_dma_bd24_3 = 0x000A030C;
// DMA BD24 4
const unsigned int mem_dma_bd24_4 = 0x000A0310;
// DMA BD24 5
const unsigned int mem_dma_bd24_5 = 0x000A0314;
// DMA BD24 6
const unsigned int mem_dma_bd24_6 = 0x000A0318;
// DMA BD24 7
const unsigned int mem_dma_bd24_7 = 0x000A031C;
// DMA BD25 0
const unsigned int mem_dma_bd25_0 = 0x000A0320;
// DMA BD25 1
const unsigned int mem_dma_bd25_1 = 0x000A0324;
// DMA BD25 2
const unsigned int mem_dma_bd25_2 = 0x000A0328;
// DMA BD25 3
const unsigned int mem_dma_bd25_3 = 0x000A032C;
// DMA BD25 4
const unsigned int mem_dma_bd25_4 = 0x000A0330;
// DMA BD25 5
const unsigned int mem_dma_bd25_5 = 0x000A0334;
// DMA BD25 6
const unsigned int mem_dma_bd25_6 = 0x000A0338;
// DMA BD25 7
const unsigned int mem_dma_bd25_7 = 0x000A033C;
// DMA BD26 0
const unsigned int mem_dma_bd26_0 = 0x000A0340;
// DMA BD26 1
const unsigned int mem_dma_bd26_1 = 0x000A0344;
// DMA BD26 2
const unsigned int mem_dma_bd26_2 = 0x000A0348;
// DMA BD26 3
const unsigned int mem_dma_bd26_3 = 0x000A034C;
// DMA BD26 4
const unsigned int mem_dma_bd26_4 = 0x000A0350;
// DMA BD26 5
const unsigned int mem_dma_bd26_5 = 0x000A0354;
// DMA BD26 6
const unsigned int mem_dma_bd26_6 = 0x000A0358;
// DMA BD26 7
const unsigned int mem_dma_bd26_7 = 0x000A035C;
// DMA BD27 0
const unsigned int mem_dma_bd27_0 = 0x000A0360;
// DMA BD27 1
const unsigned int mem_dma_bd27_1 = 0x000A0364;
// DMA BD27 2
const unsigned int mem_dma_bd27_2 = 0x000A0368;
// DMA BD27 3
const unsigned int mem_dma_bd27_3 = 0x000A036C;
// DMA BD27 4
const unsigned int mem_dma_bd27_4 = 0x000A0370;
// DMA BD27 5
const unsigned int mem_dma_bd27_5 = 0x000A0374;
// DMA BD27 6
const unsigned int mem_dma_bd27_6 = 0x000A0378;
// DMA BD27 7
const unsigned int mem_dma_bd27_7 = 0x000A037C;
// DMA BD28 0
const unsigned int mem_dma_bd28_0 = 0x000A0380;
// DMA BD28 1
const unsigned int mem_dma_bd28_1 = 0x000A0384;
// DMA BD28 2
const unsigned int mem_dma_bd28_2 = 0x000A0388;
// DMA BD28 3
const unsigned int mem_dma_bd28_3 = 0x000A038C;
// DMA BD28 4
const unsigned int mem_dma_bd28_4 = 0x000A0390;
// DMA BD28 5
const unsigned int mem_dma_bd28_5 = 0x000A0394;
// DMA BD28 6
const unsigned int mem_dma_bd28_6 = 0x000A0398;
// DMA BD28 7
const unsigned int mem_dma_bd28_7 = 0x000A039C;
// DMA BD29 0
const unsigned int mem_dma_bd29_0 = 0x000A03A0;
// DMA BD29 1
const unsigned int mem_dma_bd29_1 = 0x000A03A4;
// DMA BD29 2
const unsigned int mem_dma_bd29_2 = 0x000A03A8;
// DMA BD29 3
const unsigned int mem_dma_bd29_3 = 0x000A03AC;
// DMA BD29 4
const unsigned int mem_dma_bd29_4 = 0x000A03B0;
// DMA BD29 5
const unsigned int mem_dma_bd29_5 = 0x000A03B4;
// DMA BD29 6
const unsigned int mem_dma_bd29_6 = 0x000A03B8;
// DMA BD29 7
const unsigned int mem_dma_bd29_7 = 0x000A03BC;
// DMA BD2 0
const unsigned int mem_dma_bd2_0 = 0x000A0040;
// DMA BD2 1
const unsigned int mem_dma_bd2_1 = 0x000A0044;
// DMA BD2 2
const unsigned int mem_dma_bd2_2 = 0x000A0048;
// DMA BD2 3
const unsigned int mem_dma_bd2_3 = 0x000A004C;
// DMA BD2 4
const unsigned int mem_dma_bd2_4 = 0x000A0050;
// DMA BD2 5
const unsigned int mem_dma_bd2_5 = 0x000A0054;
// DMA BD2 6
const unsigned int mem_dma_bd2_6 = 0x000A0058;
// DMA BD2 7
const unsigned int mem_dma_bd2_7 = 0x000A005C;
// DMA BD30 0
const unsigned int mem_dma_bd30_0 = 0x000A03C0;
// DMA BD30 1
const unsigned int mem_dma_bd30_1 = 0x000A03C4;
// DMA BD30 2
const unsigned int mem_dma_bd30_2 = 0x000A03C8;
// DMA BD30 3
const unsigned int mem_dma_bd30_3 = 0x000A03CC;
// DMA BD30 4
const unsigned int mem_dma_bd30_4 = 0x000A03D0;
// DMA BD30 5
const unsigned int mem_dma_bd30_5 = 0x000A03D4;
// DMA BD30 6
const unsigned int mem_dma_bd30_6 = 0x000A03D8;
// DMA BD30 7
const unsigned int mem_dma_bd30_7 = 0x000A03DC;
// DMA BD31 0
const unsigned int mem_dma_bd31_0 = 0x000A03E0;
// DMA BD31 1
const unsigned int mem_dma_bd31_1 = 0x000A03E4;
// DMA BD31 2
const unsigned int mem_dma_bd31_2 = 0x000A03E8;
// DMA BD31 3
const unsigned int mem_dma_bd31_3 = 0x000A03EC;
// DMA BD31 4
const unsigned int mem_dma_bd31_4 = 0x000A03F0;
// DMA BD31 5
const unsigned int mem_dma_bd31_5 = 0x000A03F4;
// DMA BD31 6
const unsigned int mem_dma_bd31_6 = 0x000A03F8;
// DMA BD31 7
const unsigned int mem_dma_bd31_7 = 0x000A03FC;
// DMA BD32 0
const unsigned int mem_dma_bd32_0 = 0x000A0400;
// DMA BD32 1
const unsigned int mem_dma_bd32_1 = 0x000A0404;
// DMA BD32 2
const unsigned int mem_dma_bd32_2 = 0x000A0408;
// DMA BD32 3
const unsigned int mem_dma_bd32_3 = 0x000A040C;
// DMA BD32 4
const unsigned int mem_dma_bd32_4 = 0x000A0410;
// DMA BD32 5
const unsigned int mem_dma_bd32_5 = 0x000A0414;
// DMA BD32 6
const unsigned int mem_dma_bd32_6 = 0x000A0418;
// DMA BD32 7
const unsigned int mem_dma_bd32_7 = 0x000A041C;
// DMA BD33 0
const unsigned int mem_dma_bd33_0 = 0x000A0420;
// DMA BD33 1
const unsigned int mem_dma_bd33_1 = 0x000A0424;
// DMA BD33 2
const unsigned int mem_dma_bd33_2 = 0x000A0428;
// DMA BD33 3
const unsigned int mem_dma_bd33_3 = 0x000A042C;
// DMA BD33 4
const unsigned int mem_dma_bd33_4 = 0x000A0430;
// DMA BD33 5
const unsigned int mem_dma_bd33_5 = 0x000A0434;
// DMA BD33 6
const unsigned int mem_dma_bd33_6 = 0x000A0438;
// DMA BD33 7
const unsigned int mem_dma_bd33_7 = 0x000A043C;
// DMA BD34 0
const unsigned int mem_dma_bd34_0 = 0x000A0440;
// DMA BD34 1
const unsigned int mem_dma_bd34_1 = 0x000A0444;
// DMA BD34 2
const unsigned int mem_dma_bd34_2 = 0x000A0448;
// DMA BD34 3
const unsigned int mem_dma_bd34_3 = 0x000A044C;
// DMA BD34 4
const unsigned int mem_dma_bd34_4 = 0x000A0450;
// DMA BD34 5
const unsigned int mem_dma_bd34_5 = 0x000A0454;
// DMA BD34 6
const unsigned int mem_dma_bd34_6 = 0x000A0458;
// DMA BD34 7
const unsigned int mem_dma_bd34_7 = 0x000A045C;
// DMA BD35 0
const unsigned int mem_dma_bd35_0 = 0x000A0460;
// DMA BD35 1
const unsigned int mem_dma_bd35_1 = 0x000A0464;
// DMA BD35 2
const unsigned int mem_dma_bd35_2 = 0x000A0468;
// DMA BD35 3
const unsigned int mem_dma_bd35_3 = 0x000A046C;
// DMA BD35 4
const unsigned int mem_dma_bd35_4 = 0x000A0470;
// DMA BD35 5
const unsigned int mem_dma_bd35_5 = 0x000A0474;
// DMA BD35 6
const unsigned int mem_dma_bd35_6 = 0x000A0478;
// DMA BD35 7
const unsigned int mem_dma_bd35_7 = 0x000A047C;
// DMA BD36 0
const unsigned int mem_dma_bd36_0 = 0x000A0480;
// DMA BD36 1
const unsigned int mem_dma_bd36_1 = 0x000A0484;
// DMA BD36 2
const unsigned int mem_dma_bd36_2 = 0x000A0488;
// DMA BD36 3
const unsigned int mem_dma_bd36_3 = 0x000A048C;
// DMA BD36 4
const unsigned int mem_dma_bd36_4 = 0x000A0490;
// DMA BD36 5
const unsigned int mem_dma_bd36_5 = 0x000A0494;
// DMA BD36 6
const unsigned int mem_dma_bd36_6 = 0x000A0498;
// DMA BD36 7
const unsigned int mem_dma_bd36_7 = 0x000A049C;
// DMA BD37 0
const unsigned int mem_dma_bd37_0 = 0x000A04A0;
// DMA BD37 1
const unsigned int mem_dma_bd37_1 = 0x000A04A4;
// DMA BD37 2
const unsigned int mem_dma_bd37_2 = 0x000A04A8;
// DMA BD37 3
const unsigned int mem_dma_bd37_3 = 0x000A04AC;
// DMA BD37 4
const unsigned int mem_dma_bd37_4 = 0x000A04B0;
// DMA BD37 5
const unsigned int mem_dma_bd37_5 = 0x000A04B4;
// DMA BD37 6
const unsigned int mem_dma_bd37_6 = 0x000A04B8;
// DMA BD37 7
const unsigned int mem_dma_bd37_7 = 0x000A04BC;
// DMA BD38 0
const unsigned int mem_dma_bd38_0 = 0x000A04C0;
// DMA BD38 1
const unsigned int mem_dma_bd38_1 = 0x000A04C4;
// DMA BD38 2
const unsigned int mem_dma_bd38_2 = 0x000A04C8;
// DMA BD38 3
const unsigned int mem_dma_bd38_3 = 0x000A04CC;
// DMA BD38 4
const unsigned int mem_dma_bd38_4 = 0x000A04D0;
// DMA BD38 5
const unsigned int mem_dma_bd38_5 = 0x000A04D4;
// DMA BD38 6
const unsigned int mem_dma_bd38_6 = 0x000A04D8;
// DMA BD38 7
const unsigned int mem_dma_bd38_7 = 0x000A04DC;
// DMA BD39 0
const unsigned int mem_dma_bd39_0 = 0x000A04E0;
// DMA BD39 1
const unsigned int mem_dma_bd39_1 = 0x000A04E4;
// DMA BD39 2
const unsigned int mem_dma_bd39_2 = 0x000A04E8;
// DMA BD39 3
const unsigned int mem_dma_bd39_3 = 0x000A04EC;
// DMA BD39 4
const unsigned int mem_dma_bd39_4 = 0x000A04F0;
// DMA BD39 5
const unsigned int mem_dma_bd39_5 = 0x000A04F4;
// DMA BD39 6
const unsigned int mem_dma_bd39_6 = 0x000A04F8;
// DMA BD39 7
const unsigned int mem_dma_bd39_7 = 0x000A04FC;
// DMA BD3 0
const unsigned int mem_dma_bd3_0 = 0x000A0060;
// DMA BD3 1
const unsigned int mem_dma_bd3_1 = 0x000A0064;
// DMA BD3 2
const unsigned int mem_dma_bd3_2 = 0x000A0068;
// DMA BD3 3
const unsigned int mem_dma_bd3_3 = 0x000A006C;
// DMA BD3 4
const unsigned int mem_dma_bd3_4 = 0x000A0070;
// DMA BD3 5
const unsigned int mem_dma_bd3_5 = 0x000A0074;
// DMA BD3 6
const unsigned int mem_dma_bd3_6 = 0x000A0078;
// DMA BD3 7
const unsigned int mem_dma_bd3_7 = 0x000A007C;
// DMA BD40 0
const unsigned int mem_dma_bd40_0 = 0x000A0500;
// DMA BD40 1
const unsigned int mem_dma_bd40_1 = 0x000A0504;
// DMA BD40 2
const unsigned int mem_dma_bd40_2 = 0x000A0508;
// DMA BD40 3
const unsigned int mem_dma_bd40_3 = 0x000A050C;
// DMA BD40 4
const unsigned int mem_dma_bd40_4 = 0x000A0510;
// DMA BD40 5
const unsigned int mem_dma_bd40_5 = 0x000A0514;
// DMA BD40 6
const unsigned int mem_dma_bd40_6 = 0x000A0518;
// DMA BD40 7
const unsigned int mem_dma_bd40_7 = 0x000A051C;
// DMA BD41 0
const unsigned int mem_dma_bd41_0 = 0x000A0520;
// DMA BD41 1
const unsigned int mem_dma_bd41_1 = 0x000A0524;
// DMA BD41 2
const unsigned int mem_dma_bd41_2 = 0x000A0528;
// DMA BD41 3
const unsigned int mem_dma_bd41_3 = 0x000A052C;
// DMA BD41 4
const unsigned int mem_dma_bd41_4 = 0x000A0530;
// DMA BD41 5
const unsigned int mem_dma_bd41_5 = 0x000A0534;
// DMA BD41 6
const unsigned int mem_dma_bd41_6 = 0x000A0538;
// DMA BD41 7
const unsigned int mem_dma_bd41_7 = 0x000A053C;
// DMA BD42 0
const unsigned int mem_dma_bd42_0 = 0x000A0540;
// DMA BD42 1
const unsigned int mem_dma_bd42_1 = 0x000A0544;
// DMA BD42 2
const unsigned int mem_dma_bd42_2 = 0x000A0548;
// DMA BD42 3
const unsigned int mem_dma_bd42_3 = 0x000A054C;
// DMA BD42 4
const unsigned int mem_dma_bd42_4 = 0x000A0550;
// DMA BD42 5
const unsigned int mem_dma_bd42_5 = 0x000A0554;
// DMA BD42 6
const unsigned int mem_dma_bd42_6 = 0x000A0558;
// DMA BD42 7
const unsigned int mem_dma_bd42_7 = 0x000A055C;
// DMA BD43 0
const unsigned int mem_dma_bd43_0 = 0x000A0560;
// DMA BD43 1
const unsigned int mem_dma_bd43_1 = 0x000A0564;
// DMA BD43 2
const unsigned int mem_dma_bd43_2 = 0x000A0568;
// DMA BD43 3
const unsigned int mem_dma_bd43_3 = 0x000A056C;
// DMA BD43 4
const unsigned int mem_dma_bd43_4 = 0x000A0570;
// DMA BD43 5
const unsigned int mem_dma_bd43_5 = 0x000A0574;
// DMA BD43 6
const unsigned int mem_dma_bd43_6 = 0x000A0578;
// DMA BD43 7
const unsigned int mem_dma_bd43_7 = 0x000A057C;
// DMA BD44 0
const unsigned int mem_dma_bd44_0 = 0x000A0580;
// DMA BD44 1
const unsigned int mem_dma_bd44_1 = 0x000A0584;
// DMA BD44 2
const unsigned int mem_dma_bd44_2 = 0x000A0588;
// DMA BD44 3
const unsigned int mem_dma_bd44_3 = 0x000A058C;
// DMA BD44 4
const unsigned int mem_dma_bd44_4 = 0x000A0590;
// DMA BD44 5
const unsigned int mem_dma_bd44_5 = 0x000A0594;
// DMA BD44 6
const unsigned int mem_dma_bd44_6 = 0x000A0598;
// DMA BD44 7
const unsigned int mem_dma_bd44_7 = 0x000A059C;
// DMA BD45 0
const unsigned int mem_dma_bd45_0 = 0x000A05A0;
// DMA BD45 1
const unsigned int mem_dma_bd45_1 = 0x000A05A4;
// DMA BD45 2
const unsigned int mem_dma_bd45_2 = 0x000A05A8;
// DMA BD45 3
const unsigned int mem_dma_bd45_3 = 0x000A05AC;
// DMA BD45 4
const unsigned int mem_dma_bd45_4 = 0x000A05B0;
// DMA BD45 5
const unsigned int mem_dma_bd45_5 = 0x000A05B4;
// DMA BD45 6
const unsigned int mem_dma_bd45_6 = 0x000A05B8;
// DMA BD45 7
const unsigned int mem_dma_bd45_7 = 0x000A05BC;
// DMA BD46 0
const unsigned int mem_dma_bd46_0 = 0x000A05C0;
// DMA BD46 1
const unsigned int mem_dma_bd46_1 = 0x000A05C4;
// DMA BD46 2
const unsigned int mem_dma_bd46_2 = 0x000A05C8;
// DMA BD46 3
const unsigned int mem_dma_bd46_3 = 0x000A05CC;
// DMA BD46 4
const unsigned int mem_dma_bd46_4 = 0x000A05D0;
// DMA BD46 5
const unsigned int mem_dma_bd46_5 = 0x000A05D4;
// DMA BD46 6
const unsigned int mem_dma_bd46_6 = 0x000A05D8;
// DMA BD46 7
const unsigned int mem_dma_bd46_7 = 0x000A05DC;
// DMA BD47 0
const unsigned int mem_dma_bd47_0 = 0x000A05E0;
// DMA BD47 1
const unsigned int mem_dma_bd47_1 = 0x000A05E4;
// DMA BD47 2
const unsigned int mem_dma_bd47_2 = 0x000A05E8;
// DMA BD47 3
const unsigned int mem_dma_bd47_3 = 0x000A05EC;
// DMA BD47 4
const unsigned int mem_dma_bd47_4 = 0x000A05F0;
// DMA BD47 5
const unsigned int mem_dma_bd47_5 = 0x000A05F4;
// DMA BD47 6
const unsigned int mem_dma_bd47_6 = 0x000A05F8;
// DMA BD47 7
const unsigned int mem_dma_bd47_7 = 0x000A05FC;
// DMA BD4 0
const unsigned int mem_dma_bd4_0 = 0x000A0080;
// DMA BD4 1
const unsigned int mem_dma_bd4_1 = 0x000A0084;
// DMA BD4 2
const unsigned int mem_dma_bd4_2 = 0x000A0088;
// DMA BD4 3
const unsigned int mem_dma_bd4_3 = 0x000A008C;
// DMA BD4 4
const unsigned int mem_dma_bd4_4 = 0x000A0090;
// DMA BD4 5
const unsigned int mem_dma_bd4_5 = 0x000A0094;
// DMA BD4 6
const unsigned int mem_dma_bd4_6 = 0x000A0098;
// DMA BD4 7
const unsigned int mem_dma_bd4_7 = 0x000A009C;
// DMA BD5 0
const unsigned int mem_dma_bd5_0 = 0x000A00A0;
// DMA BD5 1
const unsigned int mem_dma_bd5_1 = 0x000A00A4;
// DMA BD5 2
const unsigned int mem_dma_bd5_2 = 0x000A00A8;
// DMA BD5 3
const unsigned int mem_dma_bd5_3 = 0x000A00AC;
// DMA BD5 4
const unsigned int mem_dma_bd5_4 = 0x000A00B0;
// DMA BD5 5
const unsigned int mem_dma_bd5_5 = 0x000A00B4;
// DMA BD5 6
const unsigned int mem_dma_bd5_6 = 0x000A00B8;
// DMA BD5 7
const unsigned int mem_dma_bd5_7 = 0x000A00BC;
// DMA BD6 0
const unsigned int mem_dma_bd6_0 = 0x000A00C0;
// DMA BD6 1
const unsigned int mem_dma_bd6_1 = 0x000A00C4;
// DMA BD6 2
const unsigned int mem_dma_bd6_2 = 0x000A00C8;
// DMA BD6 3
const unsigned int mem_dma_bd6_3 = 0x000A00CC;
// DMA BD6 4
const unsigned int mem_dma_bd6_4 = 0x000A00D0;
// DMA BD6 5
const unsigned int mem_dma_bd6_5 = 0x000A00D4;
// DMA BD6 6
const unsigned int mem_dma_bd6_6 = 0x000A00D8;
// DMA BD6 7
const unsigned int mem_dma_bd6_7 = 0x000A00DC;
// DMA BD7 0
const unsigned int mem_dma_bd7_0 = 0x000A00E0;
// DMA BD7 1
const unsigned int mem_dma_bd7_1 = 0x000A00E4;
// DMA BD7 2
const unsigned int mem_dma_bd7_2 = 0x000A00E8;
// DMA BD7 3
const unsigned int mem_dma_bd7_3 = 0x000A00EC;
// DMA BD7 4
const unsigned int mem_dma_bd7_4 = 0x000A00F0;
// DMA BD7 5
const unsigned int mem_dma_bd7_5 = 0x000A00F4;
// DMA BD7 6
const unsigned int mem_dma_bd7_6 = 0x000A00F8;
// DMA BD7 7
const unsigned int mem_dma_bd7_7 = 0x000A00FC;
// DMA BD8 0
const unsigned int mem_dma_bd8_0 = 0x000A0100;
// DMA BD8 1
const unsigned int mem_dma_bd8_1 = 0x000A0104;
// DMA BD8 2
const unsigned int mem_dma_bd8_2 = 0x000A0108;
// DMA BD8 3
const unsigned int mem_dma_bd8_3 = 0x000A010C;
// DMA BD8 4
const unsigned int mem_dma_bd8_4 = 0x000A0110;
// DMA BD8 5
const unsigned int mem_dma_bd8_5 = 0x000A0114;
// DMA BD8 6
const unsigned int mem_dma_bd8_6 = 0x000A0118;
// DMA BD8 7
const unsigned int mem_dma_bd8_7 = 0x000A011C;
// DMA BD9 0
const unsigned int mem_dma_bd9_0 = 0x000A0120;
// DMA BD9 1
const unsigned int mem_dma_bd9_1 = 0x000A0124;
// DMA BD9 2
const unsigned int mem_dma_bd9_2 = 0x000A0128;
// DMA BD9 3
const unsigned int mem_dma_bd9_3 = 0x000A012C;
// DMA BD9 4
const unsigned int mem_dma_bd9_4 = 0x000A0130;
// DMA BD9 5
const unsigned int mem_dma_bd9_5 = 0x000A0134;
// DMA BD9 6
const unsigned int mem_dma_bd9_6 = 0x000A0138;
// DMA BD9 7
const unsigned int mem_dma_bd9_7 = 0x000A013C;
// DMA Event Channel Selection
const unsigned int mem_dma_event_channel_selection = 0x000A06A0;
// DMA MM2S 0 Constant Pad Value
const unsigned int mem_dma_mm2s_0_constant_pad_value = 0x000A06E0;
// DMA MM2S 0 Ctrl
const unsigned int mem_dma_mm2s_0_ctrl = 0x000A0630;
// DMA MM2S 0 Start Queue
const unsigned int mem_dma_mm2s_0_start_queue = 0x000A0634;
// DMA MM2S 1 Constant Pad Value
const unsigned int mem_dma_mm2s_1_constant_pad_value = 0x000A06E4;
// DMA MM2S 1 Ctrl
const unsigned int mem_dma_mm2s_1_ctrl = 0x000A0638;
// DMA MM2S 1 Start Queue
const unsigned int mem_dma_mm2s_1_start_queue = 0x000A063C;
// DMA MM2S 2 Constant Pad Value
const unsigned int mem_dma_mm2s_2_constant_pad_value = 0x000A06E8;
// DMA MM2S 2 Ctrl
const unsigned int mem_dma_mm2s_2_ctrl = 0x000A0640;
// DMA MM2S 2 Start Queue
const unsigned int mem_dma_mm2s_2_start_queue = 0x000A0644;
// DMA MM2S 3 Constant Pad Value
const unsigned int mem_dma_mm2s_3_constant_pad_value = 0x000A06EC;
// DMA MM2S 3 Ctrl
const unsigned int mem_dma_mm2s_3_ctrl = 0x000A0648;
// DMA MM2S 3 Start Queue
const unsigned int mem_dma_mm2s_3_start_queue = 0x000A064C;
// DMA MM2S 4 Constant Pad Value
const unsigned int mem_dma_mm2s_4_constant_pad_value = 0x000A06F0;
// DMA MM2S 4 Ctrl
const unsigned int mem_dma_mm2s_4_ctrl = 0x000A0650;
// DMA MM2S 4 Start Queue
const unsigned int mem_dma_mm2s_4_start_queue = 0x000A0654;
// DMA MM2S 5 Constant Pad Value
const unsigned int mem_dma_mm2s_5_constant_pad_value = 0x000A06F4;
// DMA MM2S 5 Ctrl
const unsigned int mem_dma_mm2s_5_ctrl = 0x000A0658;
// DMA MM2S 5 Start Queue
const unsigned int mem_dma_mm2s_5_start_queue = 0x000A065C;
// DMA MM2S Status 0
const unsigned int mem_dma_mm2s_status_0 = 0x000A0680;
// DMA MM2S Status 1
const unsigned int mem_dma_mm2s_status_1 = 0x000A0684;
// DMA MM2S Status 2
const unsigned int mem_dma_mm2s_status_2 = 0x000A0688;
// DMA MM2S Status 3
const unsigned int mem_dma_mm2s_status_3 = 0x000A068C;
// DMA MM2S Status 4
const unsigned int mem_dma_mm2s_status_4 = 0x000A0690;
// DMA MM2S Status 5
const unsigned int mem_dma_mm2s_status_5 = 0x000A0694;
// DMA S2MM 0 Ctrl
const unsigned int mem_dma_s2mm_0_ctrl = 0x000A0600;
// DMA S2MM 0 Start Queue
const unsigned int mem_dma_s2mm_0_start_queue = 0x000A0604;
// DMA S2MM 1 Ctrl
const unsigned int mem_dma_s2mm_1_ctrl = 0x000A0608;
// DMA S2MM 1 Start Queue
const unsigned int mem_dma_s2mm_1_start_queue = 0x000A060C;
// DMA S2MM 2 Ctrl
const unsigned int mem_dma_s2mm_2_ctrl = 0x000A0610;
// DMA S2MM 2 Start Queue
const unsigned int mem_dma_s2mm_2_start_queue = 0x000A0614;
// DMA S2MM 3 Ctrl
const unsigned int mem_dma_s2mm_3_ctrl = 0x000A0618;
// DMA S2MM 3 Start Queue
const unsigned int mem_dma_s2mm_3_start_queue = 0x000A061C;
// DMA S2MM 4 Ctrl
const unsigned int mem_dma_s2mm_4_ctrl = 0x000A0620;
// DMA S2MM 4 Start Queue
const unsigned int mem_dma_s2mm_4_start_queue = 0x000A0624;
// DMA S2MM 5 Ctrl
const unsigned int mem_dma_s2mm_5_ctrl = 0x000A0628;
// DMA S2MM 5 Start Queue
const unsigned int mem_dma_s2mm_5_start_queue = 0x000A062C;
// DMA S2MM Current Write Count 0
const unsigned int mem_dma_s2mm_current_write_count_0 = 0x000A06B0;
// DMA S2MM Current Write Count 1
const unsigned int mem_dma_s2mm_current_write_count_1 = 0x000A06B4;
// DMA S2MM Current Write Count 2
const unsigned int mem_dma_s2mm_current_write_count_2 = 0x000A06B8;
// DMA S2MM Current Write Count 3
const unsigned int mem_dma_s2mm_current_write_count_3 = 0x000A06BC;
// DMA S2MM Current Write Count 4
const unsigned int mem_dma_s2mm_current_write_count_4 = 0x000A06C0;
// DMA S2MM Current Write Count 5
const unsigned int mem_dma_s2mm_current_write_count_5 = 0x000A06C4;
// DMA S2MM FoT Count FIFO Pop 0
const unsigned int mem_dma_s2mm_fot_count_fifo_pop_0 = 0x000A06C8;
// DMA S2MM FoT Count FIFO Pop 1
const unsigned int mem_dma_s2mm_fot_count_fifo_pop_1 = 0x000A06CC;
// DMA S2MM FoT Count FIFO Pop 2
const unsigned int mem_dma_s2mm_fot_count_fifo_pop_2 = 0x000A06D0;
// DMA S2MM FoT Count FIFO Pop 3
const unsigned int mem_dma_s2mm_fot_count_fifo_pop_3 = 0x000A06D4;
// DMA S2MM FoT Count FIFO Pop 4
const unsigned int mem_dma_s2mm_fot_count_fifo_pop_4 = 0x000A06D8;
// DMA S2MM FoT Count FIFO Pop 5
const unsigned int mem_dma_s2mm_fot_count_fifo_pop_5 = 0x000A06DC;
// DMA S2MM Status 0
const unsigned int mem_dma_s2mm_status_0 = 0x000A0660;
// DMA S2MM Status 1
const unsigned int mem_dma_s2mm_status_1 = 0x000A0664;
// DMA S2MM Status 2
const unsigned int mem_dma_s2mm_status_2 = 0x000A0668;
// DMA S2MM Status 3
const unsigned int mem_dma_s2mm_status_3 = 0x000A066C;
// DMA S2MM Status 4
const unsigned int mem_dma_s2mm_status_4 = 0x000A0670;
// DMA S2MM Status 5
const unsigned int mem_dma_s2mm_status_5 = 0x000A0674;
// ECC Failing Address
const unsigned int mem_ecc_failing_address = 0x00092120;
// ECC Scrubbing Event
const unsigned int mem_ecc_scrubbing_event = 0x00092110;
// Edge Detection event control
const unsigned int mem_edge_detection_event_control = 0x00094408;
// Event Broadcast0
const unsigned int mem_event_broadcast0 = 0x00094010;
// Event Broadcast1
const unsigned int mem_event_broadcast1 = 0x00094014;
// Event Broadcast10
const unsigned int mem_event_broadcast10 = 0x00094038;
// Event Broadcast11
const unsigned int mem_event_broadcast11 = 0x0009403C;
// Event Broadcast12
const unsigned int mem_event_broadcast12 = 0x00094040;
// Event Broadcast13
const unsigned int mem_event_broadcast13 = 0x00094044;
// Event Broadcast14
const unsigned int mem_event_broadcast14 = 0x00094048;
// Event Broadcast15
const unsigned int mem_event_broadcast15 = 0x0009404C;
// Event Broadcast2
const unsigned int mem_event_broadcast2 = 0x00094018;
// Event Broadcast3
const unsigned int mem_event_broadcast3 = 0x0009401C;
// Event Broadcast4
const unsigned int mem_event_broadcast4 = 0x00094020;
// Event Broadcast5
const unsigned int mem_event_broadcast5 = 0x00094024;
// Event Broadcast6
const unsigned int mem_event_broadcast6 = 0x00094028;
// Event Broadcast7
const unsigned int mem_event_broadcast7 = 0x0009402C;
// Event Broadcast8
const unsigned int mem_event_broadcast8 = 0x00094030;
// Event Broadcast9
const unsigned int mem_event_broadcast9 = 0x00094034;
// Event Broadcast A Block East Clr
const unsigned int mem_event_broadcast_a_block_east_clr = 0x00094084;
// Event Broadcast A Block East Set
const unsigned int mem_event_broadcast_a_block_east_set = 0x00094080;
// Event Broadcast A Block East Value
const unsigned int mem_event_broadcast_a_block_east_value = 0x00094088;
// Event Broadcast A Block North Clr
const unsigned int mem_event_broadcast_a_block_north_clr = 0x00094074;
// Event Broadcast A Block North Set
const unsigned int mem_event_broadcast_a_block_north_set = 0x00094070;
// Event Broadcast A Block North Value
const unsigned int mem_event_broadcast_a_block_north_value = 0x00094078;
// Event Broadcast A Block South Clr
const unsigned int mem_event_broadcast_a_block_south_clr = 0x00094054;
// Event Broadcast A Block South Set
const unsigned int mem_event_broadcast_a_block_south_set = 0x00094050;
// Event Broadcast A Block South Value
const unsigned int mem_event_broadcast_a_block_south_value = 0x00094058;
// Event Broadcast A Block West Clr
const unsigned int mem_event_broadcast_a_block_west_clr = 0x00094064;
// Event Broadcast A Block West Set
const unsigned int mem_event_broadcast_a_block_west_set = 0x00094060;
// Event Broadcast A Block West Value
const unsigned int mem_event_broadcast_a_block_west_value = 0x00094068;
// Event Broadcast B Block East Clr
const unsigned int mem_event_broadcast_b_block_east_clr = 0x000940C4;
// Event Broadcast B Block East Set
const unsigned int mem_event_broadcast_b_block_east_set = 0x000940C0;
// Event Broadcast B Block East Value
const unsigned int mem_event_broadcast_b_block_east_value = 0x000940C8;
// Event Broadcast B Block North Clr
const unsigned int mem_event_broadcast_b_block_north_clr = 0x000940B4;
// Event Broadcast B Block North Set
const unsigned int mem_event_broadcast_b_block_north_set = 0x000940B0;
// Event Broadcast B Block North Value
const unsigned int mem_event_broadcast_b_block_north_value = 0x000940B8;
// Event Broadcast B Block South Clr
const unsigned int mem_event_broadcast_b_block_south_clr = 0x00094094;
// Event Broadcast B Block South Set
const unsigned int mem_event_broadcast_b_block_south_set = 0x00094090;
// Event Broadcast B Block South Value
const unsigned int mem_event_broadcast_b_block_south_value = 0x00094098;
// Event Broadcast B Block West Clr
const unsigned int mem_event_broadcast_b_block_west_clr = 0x000940A4;
// Event Broadcast B Block West Set
const unsigned int mem_event_broadcast_b_block_west_set = 0x000940A0;
// Event Broadcast B Block West Value
const unsigned int mem_event_broadcast_b_block_west_value = 0x000940A8;
// Event Generate
const unsigned int mem_event_generate = 0x00094008;
// Event Group 0 Enable
const unsigned int mem_event_group_0_enable = 0x00094500;
// Event Group Broadcast Enable
const unsigned int mem_event_group_broadcast_enable = 0x0009451C;
// Event Group DMA Enable
const unsigned int mem_event_group_dma_enable = 0x00094508;
// Event Group Error Enable
const unsigned int mem_event_group_error_enable = 0x00094518;
// Event Group Lock Enable
const unsigned int mem_event_group_lock_enable = 0x0009450C;
// Event Group Memory Conflict Enable
const unsigned int mem_event_group_memory_conflict_enable = 0x00094514;
// Event Group Stream Switch Enable
const unsigned int mem_event_group_stream_switch_enable = 0x00094510;
// Event Group User Event Enable
const unsigned int mem_event_group_user_event_enable = 0x00094520;
// Event Group Watchpoint Enable
const unsigned int mem_event_group_watchpoint_enable = 0x00094504;
// Event Status0
const unsigned int mem_event_status0 = 0x00094200;
// Event Status1
const unsigned int mem_event_status1 = 0x00094204;
// Event Status2
const unsigned int mem_event_status2 = 0x00094208;
// Event Status3
const unsigned int mem_event_status3 = 0x0009420C;
// Event Status4
const unsigned int mem_event_status4 = 0x00094210;
// Event Status5
const unsigned int mem_event_status5 = 0x00094214;
// Lock0 value
const unsigned int mem_lock0_value = 0x000C0000;
// Lock10 value
const unsigned int mem_lock10_value = 0x000C00A0;
// Lock11 value
const unsigned int mem_lock11_value = 0x000C00B0;
// Lock12 value
const unsigned int mem_lock12_value = 0x000C00C0;
// Lock13 value
const unsigned int mem_lock13_value = 0x000C00D0;
// Lock14 value
const unsigned int mem_lock14_value = 0x000C00E0;
// Lock15 value
const unsigned int mem_lock15_value = 0x000C00F0;
// Lock16 value
const unsigned int mem_lock16_value = 0x000C0100;
// Lock17 value
const unsigned int mem_lock17_value = 0x000C0110;
// Lock18 value
const unsigned int mem_lock18_value = 0x000C0120;
// Lock19 value
const unsigned int mem_lock19_value = 0x000C0130;
// Lock1 value
const unsigned int mem_lock1_value = 0x000C0010;
// Lock20 value
const unsigned int mem_lock20_value = 0x000C0140;
// Lock21 value
const unsigned int mem_lock21_value = 0x000C0150;
// Lock22 value
const unsigned int mem_lock22_value = 0x000C0160;
// Lock23 value
const unsigned int mem_lock23_value = 0x000C0170;
// Lock24 value
const unsigned int mem_lock24_value = 0x000C0180;
// Lock25 value
const unsigned int mem_lock25_value = 0x000C0190;
// Lock26 value
const unsigned int mem_lock26_value = 0x000C01A0;
// Lock27 value
const unsigned int mem_lock27_value = 0x000C01B0;
// Lock28 value
const unsigned int mem_lock28_value = 0x000C01C0;
// Lock29 value
const unsigned int mem_lock29_value = 0x000C01D0;
// Lock2 value
const unsigned int mem_lock2_value = 0x000C0020;
// Lock30 value
const unsigned int mem_lock30_value = 0x000C01E0;
// Lock31 value
const unsigned int mem_lock31_value = 0x000C01F0;
// Lock32 value
const unsigned int mem_lock32_value = 0x000C0200;
// Lock33 value
const unsigned int mem_lock33_value = 0x000C0210;
// Lock34 value
const unsigned int mem_lock34_value = 0x000C0220;
// Lock35 value
const unsigned int mem_lock35_value = 0x000C0230;
// Lock36 value
const unsigned int mem_lock36_value = 0x000C0240;
// Lock37 value
const unsigned int mem_lock37_value = 0x000C0250;
// Lock38 value
const unsigned int mem_lock38_value = 0x000C0260;
// Lock39 value
const unsigned int mem_lock39_value = 0x000C0270;
// Lock3 value
const unsigned int mem_lock3_value = 0x000C0030;
// Lock40 value
const unsigned int mem_lock40_value = 0x000C0280;
// Lock41 value
const unsigned int mem_lock41_value = 0x000C0290;
// Lock42 value
const unsigned int mem_lock42_value = 0x000C02A0;
// Lock43 value
const unsigned int mem_lock43_value = 0x000C02B0;
// Lock44 value
const unsigned int mem_lock44_value = 0x000C02C0;
// Lock45 value
const unsigned int mem_lock45_value = 0x000C02D0;
// Lock46 value
const unsigned int mem_lock46_value = 0x000C02E0;
// Lock47 value
const unsigned int mem_lock47_value = 0x000C02F0;
// Lock48 value
const unsigned int mem_lock48_value = 0x000C0300;
// Lock49 value
const unsigned int mem_lock49_value = 0x000C0310;
// Lock4 value
const unsigned int mem_lock4_value = 0x000C0040;
// Lock50 value
const unsigned int mem_lock50_value = 0x000C0320;
// Lock51 value
const unsigned int mem_lock51_value = 0x000C0330;
// Lock52 value
const unsigned int mem_lock52_value = 0x000C0340;
// Lock53 value
const unsigned int mem_lock53_value = 0x000C0350;
// Lock54 value
const unsigned int mem_lock54_value = 0x000C0360;
// Lock55 value
const unsigned int mem_lock55_value = 0x000C0370;
// Lock56 value
const unsigned int mem_lock56_value = 0x000C0380;
// Lock57 value
const unsigned int mem_lock57_value = 0x000C0390;
// Lock58 value
const unsigned int mem_lock58_value = 0x000C03A0;
// Lock59 value
const unsigned int mem_lock59_value = 0x000C03B0;
// Lock5 value
const unsigned int mem_lock5_value = 0x000C0050;
// Lock60 value
const unsigned int mem_lock60_value = 0x000C03C0;
// Lock61 value
const unsigned int mem_lock61_value = 0x000C03D0;
// Lock62 value
const unsigned int mem_lock62_value = 0x000C03E0;
// Lock63 value
const unsigned int mem_lock63_value = 0x000C03F0;
// Lock6 value
const unsigned int mem_lock6_value = 0x000C0060;
// Lock7 value
const unsigned int mem_lock7_value = 0x000C0070;
// Lock8 value
const unsigned int mem_lock8_value = 0x000C0080;
// Lock9 value
const unsigned int mem_lock9_value = 0x000C0090;
// Lock Request
const unsigned int mem_lock_request = 0x000D0000;
// Locks Event Selection 0
const unsigned int mem_locks_event_selection_0 = 0x000C0400;
// Locks Event Selection 1
const unsigned int mem_locks_event_selection_1 = 0x000C0404;
// Locks Event Selection 2
const unsigned int mem_locks_event_selection_2 = 0x000C0408;
// Locks Event Selection 3
const unsigned int mem_locks_event_selection_3 = 0x000C040C;
// Locks Event Selection 4
const unsigned int mem_locks_event_selection_4 = 0x000C0410;
// Locks Event Selection 5
const unsigned int mem_locks_event_selection_5 = 0x000C0414;
// Locks Event Selection 6
const unsigned int mem_locks_event_selection_6 = 0x000C0418;
// Locks Event Selection 7
const unsigned int mem_locks_event_selection_7 = 0x000C041C;
// Locks Overflow 0
const unsigned int mem_locks_overflow_0 = 0x000C0420;
// Locks Overflow 1
const unsigned int mem_locks_overflow_1 = 0x000C0424;
// Locks Underflow 0
const unsigned int mem_locks_underflow_0 = 0x000C0428;
// Locks Underflow 1
const unsigned int mem_locks_underflow_1 = 0x000C042C;
// Memory Control
const unsigned int mem_memory_control = 0x00096048;
// Module Clock Control
const unsigned int mem_module_clock_control = 0x000FFF00;
// Module Reset Control
const unsigned int mem_module_reset_control = 0x000FFF10;
// Performance Control0
const unsigned int mem_performance_control0 = 0x00091000;
// Performance Control1
const unsigned int mem_performance_control1 = 0x00091004;
// Performance Control2
const unsigned int mem_performance_control2 = 0x00091008;
// Performance Control3
const unsigned int mem_performance_control3 = 0x0009100C;
// Performance Control4
const unsigned int mem_performance_control4 = 0x00091010;
// Performance Counter0
const unsigned int mem_performance_counter0 = 0x00091020;
// Performance Counter0 Event Value
const unsigned int mem_performance_counter0_event_value = 0x00091080;
// Performance Counter1
const unsigned int mem_performance_counter1 = 0x00091024;
// Performance Counter1 Event Value
const unsigned int mem_performance_counter1_event_value = 0x00091084;
// Performance Counter2
const unsigned int mem_performance_counter2 = 0x00091028;
// Performance Counter2 Event Value
const unsigned int mem_performance_counter2_event_value = 0x00091088;
// Performance Counter3
const unsigned int mem_performance_counter3 = 0x0009102C;
// Performance Counter3 Event Value
const unsigned int mem_performance_counter3_event_value = 0x0009108C;
// Performance Counter4
const unsigned int mem_performance_counter4 = 0x00091030;
// Performance Counter5
const unsigned int mem_performance_counter5 = 0x00091034;
// Spare Reg
const unsigned int mem_spare_reg = 0x000FFFF4;
// Spare Reg Privileged
const unsigned int mem_spare_reg_privileged = 0x000FFFF0;
// Stream Switch Adaptive Clock Gate Abort Period
const unsigned int mem_stream_switch_adaptive_clock_gate_abort_period = 0x000B0F38;
// Stream Switch Adaptive Clock Gate Status
const unsigned int mem_stream_switch_adaptive_clock_gate_status = 0x000B0F34;
// Stream Switch Deterministic Merge arb0 ctrl
const unsigned int mem_stream_switch_deterministic_merge_arb0_ctrl = 0x000B0808;
// Stream Switch Deterministic Merge arb0 slave0 1
const unsigned int mem_stream_switch_deterministic_merge_arb0_slave0_1 = 0x000B0800;
// Stream Switch Deterministic Merge arb0 slave2 3
const unsigned int mem_stream_switch_deterministic_merge_arb0_slave2_3 = 0x000B0804;
// Stream Switch Deterministic Merge arb1 ctrl
const unsigned int mem_stream_switch_deterministic_merge_arb1_ctrl = 0x000B0818;
// Stream Switch Deterministic Merge arb1 slave0 1
const unsigned int mem_stream_switch_deterministic_merge_arb1_slave0_1 = 0x000B0810;
// Stream Switch Deterministic Merge arb1 slave2 3
const unsigned int mem_stream_switch_deterministic_merge_arb1_slave2_3 = 0x000B0814;
// Stream Switch Event Port Selection 0
const unsigned int mem_stream_switch_event_port_selection_0 = 0x000B0F00;
// Stream Switch Event Port Selection 1
const unsigned int mem_stream_switch_event_port_selection_1 = 0x000B0F04;
// Stream Switch Master Config DMA0
const unsigned int mem_stream_switch_master_config_dma0 = 0x000B0000;
// Stream Switch Master Config DMA1
const unsigned int mem_stream_switch_master_config_dma1 = 0x000B0004;
// Stream Switch Master Config DMA2
const unsigned int mem_stream_switch_master_config_dma2 = 0x000B0008;
// Stream Switch Master Config DMA3
const unsigned int mem_stream_switch_master_config_dma3 = 0x000B000C;
// Stream Switch Master Config DMA4
const unsigned int mem_stream_switch_master_config_dma4 = 0x000B0010;
// Stream Switch Master Config DMA5
const unsigned int mem_stream_switch_master_config_dma5 = 0x000B0014;
// Stream Switch Master Config North0
const unsigned int mem_stream_switch_master_config_north0 = 0x000B002C;
// Stream Switch Master Config North1
const unsigned int mem_stream_switch_master_config_north1 = 0x000B0030;
// Stream Switch Master Config North2
const unsigned int mem_stream_switch_master_config_north2 = 0x000B0034;
// Stream Switch Master Config North3
const unsigned int mem_stream_switch_master_config_north3 = 0x000B0038;
// Stream Switch Master Config North4
const unsigned int mem_stream_switch_master_config_north4 = 0x000B003C;
// Stream Switch Master Config North5
const unsigned int mem_stream_switch_master_config_north5 = 0x000B0040;
// Stream Switch Master Config South0
const unsigned int mem_stream_switch_master_config_south0 = 0x000B001C;
// Stream Switch Master Config South1
const unsigned int mem_stream_switch_master_config_south1 = 0x000B0020;
// Stream Switch Master Config South2
const unsigned int mem_stream_switch_master_config_south2 = 0x000B0024;
// Stream Switch Master Config South3
const unsigned int mem_stream_switch_master_config_south3 = 0x000B0028;
// Stream Switch Master Config Tile Ctrl
const unsigned int mem_stream_switch_master_config_tile_ctrl = 0x000B0018;
// Stream Switch Parity Injection
const unsigned int mem_stream_switch_parity_injection = 0x000B0F20;
// Stream Switch Parity Status
const unsigned int mem_stream_switch_parity_status = 0x000B0F10;
// Stream Switch Slave Config DMA 0
const unsigned int mem_stream_switch_slave_config_dma_0 = 0x000B0100;
// Stream Switch Slave Config DMA 1
const unsigned int mem_stream_switch_slave_config_dma_1 = 0x000B0104;
// Stream Switch Slave Config DMA 2
const unsigned int mem_stream_switch_slave_config_dma_2 = 0x000B0108;
// Stream Switch Slave Config DMA 3
const unsigned int mem_stream_switch_slave_config_dma_3 = 0x000B010C;
// Stream Switch Slave Config DMA 4
const unsigned int mem_stream_switch_slave_config_dma_4 = 0x000B0110;
// Stream Switch Slave Config DMA 5
const unsigned int mem_stream_switch_slave_config_dma_5 = 0x000B0114;
// Stream Switch Slave Config North 0
const unsigned int mem_stream_switch_slave_config_north_0 = 0x000B0134;
// Stream Switch Slave Config North 1
const unsigned int mem_stream_switch_slave_config_north_1 = 0x000B0138;
// Stream Switch Slave Config North 2
const unsigned int mem_stream_switch_slave_config_north_2 = 0x000B013C;
// Stream Switch Slave Config North 3
const unsigned int mem_stream_switch_slave_config_north_3 = 0x000B0140;
// Stream Switch Slave Config South 0
const unsigned int mem_stream_switch_slave_config_south_0 = 0x000B011C;
// Stream Switch Slave Config South 1
const unsigned int mem_stream_switch_slave_config_south_1 = 0x000B0120;
// Stream Switch Slave Config South 2
const unsigned int mem_stream_switch_slave_config_south_2 = 0x000B0124;
// Stream Switch Slave Config South 3
const unsigned int mem_stream_switch_slave_config_south_3 = 0x000B0128;
// Stream Switch Slave Config South 4
const unsigned int mem_stream_switch_slave_config_south_4 = 0x000B012C;
// Stream Switch Slave Config South 5
const unsigned int mem_stream_switch_slave_config_south_5 = 0x000B0130;
// Stream Switch Slave Config Tile Ctrl
const unsigned int mem_stream_switch_slave_config_tile_ctrl = 0x000B0118;
// Stream Switch Slave Config Trace
const unsigned int mem_stream_switch_slave_config_trace = 0x000B0144;
// Stream Switch Slave DMA 0 Slot0
const unsigned int mem_stream_switch_slave_dma_0_slot0 = 0x000B0200;
// Stream Switch Slave DMA 0 Slot1
const unsigned int mem_stream_switch_slave_dma_0_slot1 = 0x000B0204;
// Stream Switch Slave DMA 0 Slot2
const unsigned int mem_stream_switch_slave_dma_0_slot2 = 0x000B0208;
// Stream Switch Slave DMA 0 Slot3
const unsigned int mem_stream_switch_slave_dma_0_slot3 = 0x000B020C;
// Stream Switch Slave DMA 1 Slot0
const unsigned int mem_stream_switch_slave_dma_1_slot0 = 0x000B0210;
// Stream Switch Slave DMA 1 Slot1
const unsigned int mem_stream_switch_slave_dma_1_slot1 = 0x000B0214;
// Stream Switch Slave DMA 1 Slot2
const unsigned int mem_stream_switch_slave_dma_1_slot2 = 0x000B0218;
// Stream Switch Slave DMA 1 Slot3
const unsigned int mem_stream_switch_slave_dma_1_slot3 = 0x000B021C;
// Stream Switch Slave DMA 2 Slot0
const unsigned int mem_stream_switch_slave_dma_2_slot0 = 0x000B0220;
// Stream Switch Slave DMA 2 Slot1
const unsigned int mem_stream_switch_slave_dma_2_slot1 = 0x000B0224;
// Stream Switch Slave DMA 2 Slot2
const unsigned int mem_stream_switch_slave_dma_2_slot2 = 0x000B0228;
// Stream Switch Slave DMA 2 Slot3
const unsigned int mem_stream_switch_slave_dma_2_slot3 = 0x000B022C;
// Stream Switch Slave DMA 3 Slot0
const unsigned int mem_stream_switch_slave_dma_3_slot0 = 0x000B0230;
// Stream Switch Slave DMA 3 Slot1
const unsigned int mem_stream_switch_slave_dma_3_slot1 = 0x000B0234;
// Stream Switch Slave DMA 3 Slot2
const unsigned int mem_stream_switch_slave_dma_3_slot2 = 0x000B0238;
// Stream Switch Slave DMA 3 Slot3
const unsigned int mem_stream_switch_slave_dma_3_slot3 = 0x000B023C;
// Stream Switch Slave DMA 4 Slot0
const unsigned int mem_stream_switch_slave_dma_4_slot0 = 0x000B0240;
// Stream Switch Slave DMA 4 Slot1
const unsigned int mem_stream_switch_slave_dma_4_slot1 = 0x000B0244;
// Stream Switch Slave DMA 4 Slot2
const unsigned int mem_stream_switch_slave_dma_4_slot2 = 0x000B0248;
// Stream Switch Slave DMA 4 Slot3
const unsigned int mem_stream_switch_slave_dma_4_slot3 = 0x000B024C;
// Stream Switch Slave DMA 5 Slot0
const unsigned int mem_stream_switch_slave_dma_5_slot0 = 0x000B0250;
// Stream Switch Slave DMA 5 Slot1
const unsigned int mem_stream_switch_slave_dma_5_slot1 = 0x000B0254;
// Stream Switch Slave DMA 5 Slot2
const unsigned int mem_stream_switch_slave_dma_5_slot2 = 0x000B0258;
// Stream Switch Slave DMA 5 Slot3
const unsigned int mem_stream_switch_slave_dma_5_slot3 = 0x000B025C;
// Stream Switch Slave North 0 Slot0
const unsigned int mem_stream_switch_slave_north_0_slot0 = 0x000B02D0;
// Stream Switch Slave North 0 Slot1
const unsigned int mem_stream_switch_slave_north_0_slot1 = 0x000B02D4;
// Stream Switch Slave North 0 Slot2
const unsigned int mem_stream_switch_slave_north_0_slot2 = 0x000B02D8;
// Stream Switch Slave North 0 Slot3
const unsigned int mem_stream_switch_slave_north_0_slot3 = 0x000B02DC;
// Stream Switch Slave North 1 Slot0
const unsigned int mem_stream_switch_slave_north_1_slot0 = 0x000B02E0;
// Stream Switch Slave North 1 Slot1
const unsigned int mem_stream_switch_slave_north_1_slot1 = 0x000B02E4;
// Stream Switch Slave North 1 Slot2
const unsigned int mem_stream_switch_slave_north_1_slot2 = 0x000B02E8;
// Stream Switch Slave North 1 Slot3
const unsigned int mem_stream_switch_slave_north_1_slot3 = 0x000B02EC;
// Stream Switch Slave North 2 Slot0
const unsigned int mem_stream_switch_slave_north_2_slot0 = 0x000B02F0;
// Stream Switch Slave North 2 Slot1
const unsigned int mem_stream_switch_slave_north_2_slot1 = 0x000B02F4;
// Stream Switch Slave North 2 Slot2
const unsigned int mem_stream_switch_slave_north_2_slot2 = 0x000B02F8;
// Stream Switch Slave North 2 Slot3
const unsigned int mem_stream_switch_slave_north_2_slot3 = 0x000B02FC;
// Stream Switch Slave North 3 Slot0
const unsigned int mem_stream_switch_slave_north_3_slot0 = 0x000B0300;
// Stream Switch Slave North 3 Slot1
const unsigned int mem_stream_switch_slave_north_3_slot1 = 0x000B0304;
// Stream Switch Slave North 3 Slot2
const unsigned int mem_stream_switch_slave_north_3_slot2 = 0x000B0308;
// Stream Switch Slave North 3 Slot3
const unsigned int mem_stream_switch_slave_north_3_slot3 = 0x000B030C;
// Stream Switch Slave South 0 Slot0
const unsigned int mem_stream_switch_slave_south_0_slot0 = 0x000B0270;
// Stream Switch Slave South 0 Slot1
const unsigned int mem_stream_switch_slave_south_0_slot1 = 0x000B0274;
// Stream Switch Slave South 0 Slot2
const unsigned int mem_stream_switch_slave_south_0_slot2 = 0x000B0278;
// Stream Switch Slave South 0 Slot3
const unsigned int mem_stream_switch_slave_south_0_slot3 = 0x000B027C;
// Stream Switch Slave South 1 Slot0
const unsigned int mem_stream_switch_slave_south_1_slot0 = 0x000B0280;
// Stream Switch Slave South 1 Slot1
const unsigned int mem_stream_switch_slave_south_1_slot1 = 0x000B0284;
// Stream Switch Slave South 1 Slot2
const unsigned int mem_stream_switch_slave_south_1_slot2 = 0x000B0288;
// Stream Switch Slave South 1 Slot3
const unsigned int mem_stream_switch_slave_south_1_slot3 = 0x000B028C;
// Stream Switch Slave South 2 Slot0
const unsigned int mem_stream_switch_slave_south_2_slot0 = 0x000B0290;
// Stream Switch Slave South 2 Slot1
const unsigned int mem_stream_switch_slave_south_2_slot1 = 0x000B0294;
// Stream Switch Slave South 2 Slot2
const unsigned int mem_stream_switch_slave_south_2_slot2 = 0x000B0298;
// Stream Switch Slave South 2 Slot3
const unsigned int mem_stream_switch_slave_south_2_slot3 = 0x000B029C;
// Stream Switch Slave South 3 Slot0
const unsigned int mem_stream_switch_slave_south_3_slot0 = 0x000B02A0;
// Stream Switch Slave South 3 Slot1
const unsigned int mem_stream_switch_slave_south_3_slot1 = 0x000B02A4;
// Stream Switch Slave South 3 Slot2
const unsigned int mem_stream_switch_slave_south_3_slot2 = 0x000B02A8;
// Stream Switch Slave South 3 Slot3
const unsigned int mem_stream_switch_slave_south_3_slot3 = 0x000B02AC;
// Stream Switch Slave South 4 Slot0
const unsigned int mem_stream_switch_slave_south_4_slot0 = 0x000B02B0;
// Stream Switch Slave South 4 Slot1
const unsigned int mem_stream_switch_slave_south_4_slot1 = 0x000B02B4;
// Stream Switch Slave South 4 Slot2
const unsigned int mem_stream_switch_slave_south_4_slot2 = 0x000B02B8;
// Stream Switch Slave South 4 Slot3
const unsigned int mem_stream_switch_slave_south_4_slot3 = 0x000B02BC;
// Stream Switch Slave South 5 Slot0
const unsigned int mem_stream_switch_slave_south_5_slot0 = 0x000B02C0;
// Stream Switch Slave South 5 Slot1
const unsigned int mem_stream_switch_slave_south_5_slot1 = 0x000B02C4;
// Stream Switch Slave South 5 Slot2
const unsigned int mem_stream_switch_slave_south_5_slot2 = 0x000B02C8;
// Stream Switch Slave South 5 Slot3
const unsigned int mem_stream_switch_slave_south_5_slot3 = 0x000B02CC;
// Stream Switch Slave Tile Ctrl Slot0
const unsigned int mem_stream_switch_slave_tile_ctrl_slot0 = 0x000B0260;
// Stream Switch Slave Tile Ctrl Slot1
const unsigned int mem_stream_switch_slave_tile_ctrl_slot1 = 0x000B0264;
// Stream Switch Slave Tile Ctrl Slot2
const unsigned int mem_stream_switch_slave_tile_ctrl_slot2 = 0x000B0268;
// Stream Switch Slave Tile Ctrl Slot3
const unsigned int mem_stream_switch_slave_tile_ctrl_slot3 = 0x000B026C;
// Stream Switch Slave Trace Slot0
const unsigned int mem_stream_switch_slave_trace_slot0 = 0x000B0310;
// Stream Switch Slave Trace Slot1
const unsigned int mem_stream_switch_slave_trace_slot1 = 0x000B0314;
// Stream Switch Slave Trace Slot2
const unsigned int mem_stream_switch_slave_trace_slot2 = 0x000B0318;
// Stream Switch Slave Trace Slot3
const unsigned int mem_stream_switch_slave_trace_slot3 = 0x000B031C;
// Tile Control
const unsigned int mem_tile_control = 0x000FFF20;
// Tile Control Packet Handler Status
const unsigned int mem_tile_control_packet_handler_status = 0x000B0F30;
// Timer Control
const unsigned int mem_timer_control = 0x00094000;
// Timer High
const unsigned int mem_timer_high = 0x000940FC;
// Timer Low
const unsigned int mem_timer_low = 0x000940F8;
// Timer Trig Event High Value
const unsigned int mem_timer_trig_event_high_value = 0x000940F4;
// Timer Trig Event Low Value
const unsigned int mem_timer_trig_event_low_value = 0x000940F0;
// Trace Control0
const unsigned int mem_trace_control0 = 0x000940D0;
// Trace Control1
const unsigned int mem_trace_control1 = 0x000940D4;
// Trace Event0
const unsigned int mem_trace_event0 = 0x000940E0;
// Trace Event1
const unsigned int mem_trace_event1 = 0x000940E4;
// Trace Status
const unsigned int mem_trace_status = 0x000940D8;
// WatchPoint0
const unsigned int mem_watchpoint0 = 0x00094100;
// WatchPoint1
const unsigned int mem_watchpoint1 = 0x00094104;
// WatchPoint2
const unsigned int mem_watchpoint2 = 0x00094108;
// WatchPoint3
const unsigned int mem_watchpoint3 = 0x0009410C;

// Register definitions for MM
// ######################################
// Checkbit Error Generation
const unsigned int mm_checkbit_error_generation = 0x00012000;
// Combo event control
const unsigned int mm_combo_event_control = 0x00014404;
// Combo event inputs
const unsigned int mm_combo_event_inputs = 0x00014400;
// DMA BD0 0
const unsigned int mm_dma_bd0_0 = 0x0001D000;
// DMA BD0 1
const unsigned int mm_dma_bd0_1 = 0x0001D004;
// DMA BD0 2
const unsigned int mm_dma_bd0_2 = 0x0001D008;
// DMA BD0 3
const unsigned int mm_dma_bd0_3 = 0x0001D00C;
// DMA BD0 4
const unsigned int mm_dma_bd0_4 = 0x0001D010;
// DMA BD0 5
const unsigned int mm_dma_bd0_5 = 0x0001D014;
// DMA BD10 0
const unsigned int mm_dma_bd10_0 = 0x0001D140;
// DMA BD10 1
const unsigned int mm_dma_bd10_1 = 0x0001D144;
// DMA BD10 2
const unsigned int mm_dma_bd10_2 = 0x0001D148;
// DMA BD10 3
const unsigned int mm_dma_bd10_3 = 0x0001D14C;
// DMA BD10 4
const unsigned int mm_dma_bd10_4 = 0x0001D150;
// DMA BD10 5
const unsigned int mm_dma_bd10_5 = 0x0001D154;
// DMA BD11 0
const unsigned int mm_dma_bd11_0 = 0x0001D160;
// DMA BD11 1
const unsigned int mm_dma_bd11_1 = 0x0001D164;
// DMA BD11 2
const unsigned int mm_dma_bd11_2 = 0x0001D168;
// DMA BD11 3
const unsigned int mm_dma_bd11_3 = 0x0001D16C;
// DMA BD11 4
const unsigned int mm_dma_bd11_4 = 0x0001D170;
// DMA BD11 5
const unsigned int mm_dma_bd11_5 = 0x0001D174;
// DMA BD12 0
const unsigned int mm_dma_bd12_0 = 0x0001D180;
// DMA BD12 1
const unsigned int mm_dma_bd12_1 = 0x0001D184;
// DMA BD12 2
const unsigned int mm_dma_bd12_2 = 0x0001D188;
// DMA BD12 3
const unsigned int mm_dma_bd12_3 = 0x0001D18C;
// DMA BD12 4
const unsigned int mm_dma_bd12_4 = 0x0001D190;
// DMA BD12 5
const unsigned int mm_dma_bd12_5 = 0x0001D194;
// DMA BD13 0
const unsigned int mm_dma_bd13_0 = 0x0001D1A0;
// DMA BD13 1
const unsigned int mm_dma_bd13_1 = 0x0001D1A4;
// DMA BD13 2
const unsigned int mm_dma_bd13_2 = 0x0001D1A8;
// DMA BD13 3
const unsigned int mm_dma_bd13_3 = 0x0001D1AC;
// DMA BD13 4
const unsigned int mm_dma_bd13_4 = 0x0001D1B0;
// DMA BD13 5
const unsigned int mm_dma_bd13_5 = 0x0001D1B4;
// DMA BD14 0
const unsigned int mm_dma_bd14_0 = 0x0001D1C0;
// DMA BD14 1
const unsigned int mm_dma_bd14_1 = 0x0001D1C4;
// DMA BD14 2
const unsigned int mm_dma_bd14_2 = 0x0001D1C8;
// DMA BD14 3
const unsigned int mm_dma_bd14_3 = 0x0001D1CC;
// DMA BD14 4
const unsigned int mm_dma_bd14_4 = 0x0001D1D0;
// DMA BD14 5
const unsigned int mm_dma_bd14_5 = 0x0001D1D4;
// DMA BD15 0
const unsigned int mm_dma_bd15_0 = 0x0001D1E0;
// DMA BD15 1
const unsigned int mm_dma_bd15_1 = 0x0001D1E4;
// DMA BD15 2
const unsigned int mm_dma_bd15_2 = 0x0001D1E8;
// DMA BD15 3
const unsigned int mm_dma_bd15_3 = 0x0001D1EC;
// DMA BD15 4
const unsigned int mm_dma_bd15_4 = 0x0001D1F0;
// DMA BD15 5
const unsigned int mm_dma_bd15_5 = 0x0001D1F4;
// DMA BD1 0
const unsigned int mm_dma_bd1_0 = 0x0001D020;
// DMA BD1 1
const unsigned int mm_dma_bd1_1 = 0x0001D024;
// DMA BD1 2
const unsigned int mm_dma_bd1_2 = 0x0001D028;
// DMA BD1 3
const unsigned int mm_dma_bd1_3 = 0x0001D02C;
// DMA BD1 4
const unsigned int mm_dma_bd1_4 = 0x0001D030;
// DMA BD1 5
const unsigned int mm_dma_bd1_5 = 0x0001D034;
// DMA BD2 0
const unsigned int mm_dma_bd2_0 = 0x0001D040;
// DMA BD2 1
const unsigned int mm_dma_bd2_1 = 0x0001D044;
// DMA BD2 2
const unsigned int mm_dma_bd2_2 = 0x0001D048;
// DMA BD2 3
const unsigned int mm_dma_bd2_3 = 0x0001D04C;
// DMA BD2 4
const unsigned int mm_dma_bd2_4 = 0x0001D050;
// DMA BD2 5
const unsigned int mm_dma_bd2_5 = 0x0001D054;
// DMA BD3 0
const unsigned int mm_dma_bd3_0 = 0x0001D060;
// DMA BD3 1
const unsigned int mm_dma_bd3_1 = 0x0001D064;
// DMA BD3 2
const unsigned int mm_dma_bd3_2 = 0x0001D068;
// DMA BD3 3
const unsigned int mm_dma_bd3_3 = 0x0001D06C;
// DMA BD3 4
const unsigned int mm_dma_bd3_4 = 0x0001D070;
// DMA BD3 5
const unsigned int mm_dma_bd3_5 = 0x0001D074;
// DMA BD4 0
const unsigned int mm_dma_bd4_0 = 0x0001D080;
// DMA BD4 1
const unsigned int mm_dma_bd4_1 = 0x0001D084;
// DMA BD4 2
const unsigned int mm_dma_bd4_2 = 0x0001D088;
// DMA BD4 3
const unsigned int mm_dma_bd4_3 = 0x0001D08C;
// DMA BD4 4
const unsigned int mm_dma_bd4_4 = 0x0001D090;
// DMA BD4 5
const unsigned int mm_dma_bd4_5 = 0x0001D094;
// DMA BD5 0
const unsigned int mm_dma_bd5_0 = 0x0001D0A0;
// DMA BD5 1
const unsigned int mm_dma_bd5_1 = 0x0001D0A4;
// DMA BD5 2
const unsigned int mm_dma_bd5_2 = 0x0001D0A8;
// DMA BD5 3
const unsigned int mm_dma_bd5_3 = 0x0001D0AC;
// DMA BD5 4
const unsigned int mm_dma_bd5_4 = 0x0001D0B0;
// DMA BD5 5
const unsigned int mm_dma_bd5_5 = 0x0001D0B4;
// DMA BD6 0
const unsigned int mm_dma_bd6_0 = 0x0001D0C0;
// DMA BD6 1
const unsigned int mm_dma_bd6_1 = 0x0001D0C4;
// DMA BD6 2
const unsigned int mm_dma_bd6_2 = 0x0001D0C8;
// DMA BD6 3
const unsigned int mm_dma_bd6_3 = 0x0001D0CC;
// DMA BD6 4
const unsigned int mm_dma_bd6_4 = 0x0001D0D0;
// DMA BD6 5
const unsigned int mm_dma_bd6_5 = 0x0001D0D4;
// DMA BD7 0
const unsigned int mm_dma_bd7_0 = 0x0001D0E0;
// DMA BD7 1
const unsigned int mm_dma_bd7_1 = 0x0001D0E4;
// DMA BD7 2
const unsigned int mm_dma_bd7_2 = 0x0001D0E8;
// DMA BD7 3
const unsigned int mm_dma_bd7_3 = 0x0001D0EC;
// DMA BD7 4
const unsigned int mm_dma_bd7_4 = 0x0001D0F0;
// DMA BD7 5
const unsigned int mm_dma_bd7_5 = 0x0001D0F4;
// DMA BD8 0
const unsigned int mm_dma_bd8_0 = 0x0001D100;
// DMA BD8 1
const unsigned int mm_dma_bd8_1 = 0x0001D104;
// DMA BD8 2
const unsigned int mm_dma_bd8_2 = 0x0001D108;
// DMA BD8 3
const unsigned int mm_dma_bd8_3 = 0x0001D10C;
// DMA BD8 4
const unsigned int mm_dma_bd8_4 = 0x0001D110;
// DMA BD8 5
const unsigned int mm_dma_bd8_5 = 0x0001D114;
// DMA BD9 0
const unsigned int mm_dma_bd9_0 = 0x0001D120;
// DMA BD9 1
const unsigned int mm_dma_bd9_1 = 0x0001D124;
// DMA BD9 2
const unsigned int mm_dma_bd9_2 = 0x0001D128;
// DMA BD9 3
const unsigned int mm_dma_bd9_3 = 0x0001D12C;
// DMA BD9 4
const unsigned int mm_dma_bd9_4 = 0x0001D130;
// DMA BD9 5
const unsigned int mm_dma_bd9_5 = 0x0001D134;
// DMA MM2S 0 Ctrl
const unsigned int mm_dma_mm2s_0_ctrl = 0x0001DE10;
// DMA MM2S 0 Start Queue
const unsigned int mm_dma_mm2s_0_start_queue = 0x0001DE14;
// DMA MM2S 1 Ctrl
const unsigned int mm_dma_mm2s_1_ctrl = 0x0001DE18;
// DMA MM2S 1 Start Queue
const unsigned int mm_dma_mm2s_1_start_queue = 0x0001DE1C;
// DMA MM2S Status 0
const unsigned int mm_dma_mm2s_status_0 = 0x0001DF10;
// DMA MM2S Status 1
const unsigned int mm_dma_mm2s_status_1 = 0x0001DF14;
// DMA S2MM 0 Ctrl
const unsigned int mm_dma_s2mm_0_ctrl = 0x0001DE00;
// DMA S2MM 0 Start Queue
const unsigned int mm_dma_s2mm_0_start_queue = 0x0001DE04;
// DMA S2MM 1 Ctrl
const unsigned int mm_dma_s2mm_1_ctrl = 0x0001DE08;
// DMA S2MM 1 Start Queue
const unsigned int mm_dma_s2mm_1_start_queue = 0x0001DE0C;
// DMA S2MM Current Write Count 0
const unsigned int mm_dma_s2mm_current_write_count_0 = 0x0001DF18;
// DMA S2MM Current Write Count 1
const unsigned int mm_dma_s2mm_current_write_count_1 = 0x0001DF1C;
// DMA S2MM FoT Count FIFO Pop 0
const unsigned int mm_dma_s2mm_fot_count_fifo_pop_0 = 0x0001DF20;
// DMA S2MM FoT Count FIFO Pop 1
const unsigned int mm_dma_s2mm_fot_count_fifo_pop_1 = 0x0001DF24;
// DMA S2MM Status 0
const unsigned int mm_dma_s2mm_status_0 = 0x0001DF00;
// DMA S2MM Status 1
const unsigned int mm_dma_s2mm_status_1 = 0x0001DF04;
// ECC Failing Address
const unsigned int mm_ecc_failing_address = 0x00012120;
// ECC Scrubbing Event
const unsigned int mm_ecc_scrubbing_event = 0x00012110;
// Edge Detection event control
const unsigned int mm_edge_detection_event_control = 0x00014408;
// Event Broadcast0
const unsigned int mm_event_broadcast0 = 0x00014010;
// Event Broadcast1
const unsigned int mm_event_broadcast1 = 0x00014014;
// Event Broadcast10
const unsigned int mm_event_broadcast10 = 0x00014038;
// Event Broadcast11
const unsigned int mm_event_broadcast11 = 0x0001403C;
// Event Broadcast12
const unsigned int mm_event_broadcast12 = 0x00014040;
// Event Broadcast13
const unsigned int mm_event_broadcast13 = 0x00014044;
// Event Broadcast14
const unsigned int mm_event_broadcast14 = 0x00014048;
// Event Broadcast15
const unsigned int mm_event_broadcast15 = 0x0001404C;
// Event Broadcast2
const unsigned int mm_event_broadcast2 = 0x00014018;
// Event Broadcast3
const unsigned int mm_event_broadcast3 = 0x0001401C;
// Event Broadcast4
const unsigned int mm_event_broadcast4 = 0x00014020;
// Event Broadcast5
const unsigned int mm_event_broadcast5 = 0x00014024;
// Event Broadcast6
const unsigned int mm_event_broadcast6 = 0x00014028;
// Event Broadcast7
const unsigned int mm_event_broadcast7 = 0x0001402C;
// Event Broadcast8
const unsigned int mm_event_broadcast8 = 0x00014030;
// Event Broadcast9
const unsigned int mm_event_broadcast9 = 0x00014034;
// Event Broadcast Block East Clr
const unsigned int mm_event_broadcast_block_east_clr = 0x00014084;
// Event Broadcast Block East Set
const unsigned int mm_event_broadcast_block_east_set = 0x00014080;
// Event Broadcast Block East Value
const unsigned int mm_event_broadcast_block_east_value = 0x00014088;
// Event Broadcast Block North Clr
const unsigned int mm_event_broadcast_block_north_clr = 0x00014074;
// Event Broadcast Block North Set
const unsigned int mm_event_broadcast_block_north_set = 0x00014070;
// Event Broadcast Block North Value
const unsigned int mm_event_broadcast_block_north_value = 0x00014078;
// Event Broadcast Block South Clr
const unsigned int mm_event_broadcast_block_south_clr = 0x00014054;
// Event Broadcast Block South Set
const unsigned int mm_event_broadcast_block_south_set = 0x00014050;
// Event Broadcast Block South Value
const unsigned int mm_event_broadcast_block_south_value = 0x00014058;
// Event Broadcast Block West Clr
const unsigned int mm_event_broadcast_block_west_clr = 0x00014064;
// Event Broadcast Block West Set
const unsigned int mm_event_broadcast_block_west_set = 0x00014060;
// Event Broadcast Block West Value
const unsigned int mm_event_broadcast_block_west_value = 0x00014068;
// Event Generate
const unsigned int mm_event_generate = 0x00014008;
// Event Group 0 Enable
const unsigned int mm_event_group_0_enable = 0x00014500;
// Event Group Broadcast Enable
const unsigned int mm_event_group_broadcast_enable = 0x00014518;
// Event Group DMA Enable
const unsigned int mm_event_group_dma_enable = 0x00014508;
// Event Group Error Enable
const unsigned int mm_event_group_error_enable = 0x00014514;
// Event Group Lock Enable
const unsigned int mm_event_group_lock_enable = 0x0001450C;
// Event Group Memory Conflict Enable
const unsigned int mm_event_group_memory_conflict_enable = 0x00014510;
// Event Group User Event Enable
const unsigned int mm_event_group_user_event_enable = 0x0001451C;
// Event Group Watchpoint Enable
const unsigned int mm_event_group_watchpoint_enable = 0x00014504;
// Event Status0
const unsigned int mm_event_status0 = 0x00014200;
// Event Status1
const unsigned int mm_event_status1 = 0x00014204;
// Event Status2
const unsigned int mm_event_status2 = 0x00014208;
// Event Status3
const unsigned int mm_event_status3 = 0x0001420C;
// Lock0 value
const unsigned int mm_lock0_value = 0x0001F000;
// Lock10 value
const unsigned int mm_lock10_value = 0x0001F0A0;
// Lock11 value
const unsigned int mm_lock11_value = 0x0001F0B0;
// Lock12 value
const unsigned int mm_lock12_value = 0x0001F0C0;
// Lock13 value
const unsigned int mm_lock13_value = 0x0001F0D0;
// Lock14 value
const unsigned int mm_lock14_value = 0x0001F0E0;
// Lock15 value
const unsigned int mm_lock15_value = 0x0001F0F0;
// Lock1 value
const unsigned int mm_lock1_value = 0x0001F010;
// Lock2 value
const unsigned int mm_lock2_value = 0x0001F020;
// Lock3 value
const unsigned int mm_lock3_value = 0x0001F030;
// Lock4 value
const unsigned int mm_lock4_value = 0x0001F040;
// Lock5 value
const unsigned int mm_lock5_value = 0x0001F050;
// Lock6 value
const unsigned int mm_lock6_value = 0x0001F060;
// Lock7 value
const unsigned int mm_lock7_value = 0x0001F070;
// Lock8 value
const unsigned int mm_lock8_value = 0x0001F080;
// Lock9 value
const unsigned int mm_lock9_value = 0x0001F090;
// Lock Request
const unsigned int mm_lock_request = 0x00040000;
// Locks Event Selection 0
const unsigned int mm_locks_event_selection_0 = 0x0001F100;
// Locks Event Selection 1
const unsigned int mm_locks_event_selection_1 = 0x0001F104;
// Locks Event Selection 2
const unsigned int mm_locks_event_selection_2 = 0x0001F108;
// Locks Event Selection 3
const unsigned int mm_locks_event_selection_3 = 0x0001F10C;
// Locks Event Selection 4
const unsigned int mm_locks_event_selection_4 = 0x0001F110;
// Locks Event Selection 5
const unsigned int mm_locks_event_selection_5 = 0x0001F114;
// Locks Event Selection 6
const unsigned int mm_locks_event_selection_6 = 0x0001F118;
// Locks Event Selection 7
const unsigned int mm_locks_event_selection_7 = 0x0001F11C;
// Locks Overflow
const unsigned int mm_locks_overflow = 0x0001F120;
// Locks Underflow
const unsigned int mm_locks_underflow = 0x0001F128;
// Memory Control
const unsigned int mm_memory_control = 0x00016010;
// Parity Failing Address
const unsigned int mm_parity_failing_address = 0x00012124;
// Performance Control0
const unsigned int mm_performance_control0 = 0x00011000;
// Performance Control1
const unsigned int mm_performance_control1 = 0x00011008;
// Performance Control2
const unsigned int mm_performance_control2 = 0x0001100C;
// Performance Control3
const unsigned int mm_performance_control3 = 0x00011010;
// Performance Counter0
const unsigned int mm_performance_counter0 = 0x00011020;
// Performance Counter0 Event Value
const unsigned int mm_performance_counter0_event_value = 0x00011080;
// Performance Counter1
const unsigned int mm_performance_counter1 = 0x00011024;
// Performance Counter1 Event Value
const unsigned int mm_performance_counter1_event_value = 0x00011084;
// Performance Counter2
const unsigned int mm_performance_counter2 = 0x00011028;
// Performance Counter3
const unsigned int mm_performance_counter3 = 0x0001102C;
// Spare Reg
const unsigned int mm_spare_reg = 0x00016000;
// Timer Control
const unsigned int mm_timer_control = 0x00014000;
// Timer High
const unsigned int mm_timer_high = 0x000140FC;
// Timer Low
const unsigned int mm_timer_low = 0x000140F8;
// Timer Trig Event High Value
const unsigned int mm_timer_trig_event_high_value = 0x000140F4;
// Timer Trig Event Low Value
const unsigned int mm_timer_trig_event_low_value = 0x000140F0;
// Trace Control0
const unsigned int mm_trace_control0 = 0x000140D0;
// Trace Control1
const unsigned int mm_trace_control1 = 0x000140D4;
// Trace Event0
const unsigned int mm_trace_event0 = 0x000140E0;
// Trace Event1
const unsigned int mm_trace_event1 = 0x000140E4;
// Trace Status
const unsigned int mm_trace_status = 0x000140D8;
// WatchPoint0
const unsigned int mm_watchpoint0 = 0x00014100;
// WatchPoint1
const unsigned int mm_watchpoint1 = 0x00014104;

// Register definitions for NPI
// ###################################
// Interrupt Disable Register
const unsigned int npi_me_idr0 = 0x00000040;
// Interrupt Disable Register
const unsigned int npi_me_idr1 = 0x0000004C;
// Interrupt Disable Register
const unsigned int npi_me_idr2 = 0x00000058;
// Interrupt Disable Register
const unsigned int npi_me_idr3 = 0x00000064;
// Interrupte Enable Register
const unsigned int npi_me_ier0 = 0x0000003C;
// Interrupte Enable Register
const unsigned int npi_me_ier1 = 0x00000048;
// Interrupte Enable Register
const unsigned int npi_me_ier2 = 0x00000054;
// Interrupte Enable Register
const unsigned int npi_me_ier3 = 0x00000060;
// Interrupt Mask Register
const unsigned int npi_me_imr0 = 0x00000038;
// Interrupt Mask Register
const unsigned int npi_me_imr1 = 0x00000044;
// Interrupt Mask Register
const unsigned int npi_me_imr2 = 0x00000050;
// Interrupt Mask Register
const unsigned int npi_me_imr3 = 0x0000005C;
// Interrupt Offset Register
const unsigned int npi_me_ior = 0x0000006C;
// Interrupt Status Register
const unsigned int npi_me_isr = 0x00000030;
// Interrupt Trigger Register
const unsigned int npi_me_itr = 0x00000034;
// Tells the status of the PLLs. Note: All Status Registers have no predefined Reset value. The value shown in reset is a typical value that will be read after reset. The actual value read can differ from this.
const unsigned int npi_me_pll_status = 0x0000010C;
// ME secure register
const unsigned int npi_me_secure_reg = 0x00000208;

// Register definitions for SHIM
// ######################################
// AXI MM Outstanding Transactions
const unsigned int shim_axi_mm_outstanding_transactions = 0x00002120;
// BISR cache ctrl
const unsigned int shim_bisr_cache_ctrl = 0x00036000;
// BISR cache data0
const unsigned int shim_bisr_cache_data0 = 0x00036010;
// BISR cache data1
const unsigned int shim_bisr_cache_data1 = 0x00036014;
// BISR cache data2
const unsigned int shim_bisr_cache_data2 = 0x00036018;
// BISR cache data3
const unsigned int shim_bisr_cache_data3 = 0x0003601C;
// BISR cache data4
const unsigned int shim_bisr_cache_data4 = 0x00036020;
// BISR cache data5
const unsigned int shim_bisr_cache_data5 = 0x00036024;
// BISR cache data6
const unsigned int shim_bisr_cache_data6 = 0x00036028;
// BISR cache data7
const unsigned int shim_bisr_cache_data7 = 0x0003602C;
// BISR cache status
const unsigned int shim_bisr_cache_status = 0x00036008;
// BISR test data0
const unsigned int shim_bisr_test_data0 = 0x00036030;
// BISR test data1
const unsigned int shim_bisr_test_data1 = 0x00036034;
// BISR test data2
const unsigned int shim_bisr_test_data2 = 0x00036038;
// BISR test data3
const unsigned int shim_bisr_test_data3 = 0x0003603C;
// BISR test data4
const unsigned int shim_bisr_test_data4 = 0x00036040;
// BISR test data5
const unsigned int shim_bisr_test_data5 = 0x00036044;
// BISR test data6
const unsigned int shim_bisr_test_data6 = 0x00036048;
// BISR test data7
const unsigned int shim_bisr_test_data7 = 0x0003604C;
// Column Clock Control
const unsigned int shim_column_clock_control = 0x0007FF20;
// Column Reset Control
const unsigned int shim_column_reset_control = 0x0007FF28;
// Combo event control
const unsigned int shim_combo_event_control = 0x00034404;
// Combo event inputs
const unsigned int shim_combo_event_inputs = 0x00034400;
// Control Packet Handler Status
const unsigned int shim_control_packet_handler_status = 0x0003FF30;
// Core Control
const unsigned int shim_core_control = 0x000C0004;
// Core Interrupt Status
const unsigned int shim_core_interrupt_status = 0x000C0008;
// Core Status
const unsigned int shim_core_status = 0x000C0000;
// CSSD Trigger
const unsigned int shim_cssd_trigger = 0x0007FF4C;
// Demux Config
const unsigned int shim_demux_config = 0x00002108;
// DMA BD0 0
const unsigned int shim_dma_bd0_0 = 0x00009000;
// DMA BD0 1
const unsigned int shim_dma_bd0_1 = 0x00009004;
// DMA BD0 2
const unsigned int shim_dma_bd0_2 = 0x00009008;
// DMA BD0 3
const unsigned int shim_dma_bd0_3 = 0x0000900C;
// DMA BD0 4
const unsigned int shim_dma_bd0_4 = 0x00009010;
// DMA BD0 5
const unsigned int shim_dma_bd0_5 = 0x00009014;
// DMA BD0 6
const unsigned int shim_dma_bd0_6 = 0x00009018;
// DMA BD0 7
const unsigned int shim_dma_bd0_7 = 0x0000901C;
// DMA BD0 8
const unsigned int shim_dma_bd0_8 = 0x00009020;
// DMA BD10 0
const unsigned int shim_dma_bd10_0 = 0x000091E0;
// DMA BD10 1
const unsigned int shim_dma_bd10_1 = 0x000091E4;
// DMA BD10 2
const unsigned int shim_dma_bd10_2 = 0x000091E8;
// DMA BD10 3
const unsigned int shim_dma_bd10_3 = 0x000091EC;
// DMA BD10 4
const unsigned int shim_dma_bd10_4 = 0x000091F0;
// DMA BD10 5
const unsigned int shim_dma_bd10_5 = 0x000091F4;
// DMA BD10 6
const unsigned int shim_dma_bd10_6 = 0x000091F8;
// DMA BD10 7
const unsigned int shim_dma_bd10_7 = 0x000091FC;
// DMA BD10 8
const unsigned int shim_dma_bd10_8 = 0x00009200;
// DMA BD11 0
const unsigned int shim_dma_bd11_0 = 0x00009210;
// DMA BD11 1
const unsigned int shim_dma_bd11_1 = 0x00009214;
// DMA BD11 2
const unsigned int shim_dma_bd11_2 = 0x00009218;
// DMA BD11 3
const unsigned int shim_dma_bd11_3 = 0x0000921C;
// DMA BD11 4
const unsigned int shim_dma_bd11_4 = 0x00009220;
// DMA BD11 5
const unsigned int shim_dma_bd11_5 = 0x00009224;
// DMA BD11 6
const unsigned int shim_dma_bd11_6 = 0x00009228;
// DMA BD11 7
const unsigned int shim_dma_bd11_7 = 0x0000922C;
// DMA BD11 8
const unsigned int shim_dma_bd11_8 = 0x00009230;
// DMA BD12 0
const unsigned int shim_dma_bd12_0 = 0x00009240;
// DMA BD12 1
const unsigned int shim_dma_bd12_1 = 0x00009244;
// DMA BD12 2
const unsigned int shim_dma_bd12_2 = 0x00009248;
// DMA BD12 3
const unsigned int shim_dma_bd12_3 = 0x0000924C;
// DMA BD12 4
const unsigned int shim_dma_bd12_4 = 0x00009250;
// DMA BD12 5
const unsigned int shim_dma_bd12_5 = 0x00009254;
// DMA BD12 6
const unsigned int shim_dma_bd12_6 = 0x00009258;
// DMA BD12 7
const unsigned int shim_dma_bd12_7 = 0x0000925C;
// DMA BD12 8
const unsigned int shim_dma_bd12_8 = 0x00009260;
// DMA BD13 0
const unsigned int shim_dma_bd13_0 = 0x00009270;
// DMA BD13 1
const unsigned int shim_dma_bd13_1 = 0x00009274;
// DMA BD13 2
const unsigned int shim_dma_bd13_2 = 0x00009278;
// DMA BD13 3
const unsigned int shim_dma_bd13_3 = 0x0000927C;
// DMA BD13 4
const unsigned int shim_dma_bd13_4 = 0x00009280;
// DMA BD13 5
const unsigned int shim_dma_bd13_5 = 0x00009284;
// DMA BD13 6
const unsigned int shim_dma_bd13_6 = 0x00009288;
// DMA BD13 7
const unsigned int shim_dma_bd13_7 = 0x0000928C;
// DMA BD13 8
const unsigned int shim_dma_bd13_8 = 0x00009290;
// DMA BD14 0
const unsigned int shim_dma_bd14_0 = 0x000092A0;
// DMA BD14 1
const unsigned int shim_dma_bd14_1 = 0x000092A4;
// DMA BD14 2
const unsigned int shim_dma_bd14_2 = 0x000092A8;
// DMA BD14 3
const unsigned int shim_dma_bd14_3 = 0x000092AC;
// DMA BD14 4
const unsigned int shim_dma_bd14_4 = 0x000092B0;
// DMA BD14 5
const unsigned int shim_dma_bd14_5 = 0x000092B4;
// DMA BD14 6
const unsigned int shim_dma_bd14_6 = 0x000092B8;
// DMA BD14 7
const unsigned int shim_dma_bd14_7 = 0x000092BC;
// DMA BD14 8
const unsigned int shim_dma_bd14_8 = 0x000092C0;
// DMA BD15 0
const unsigned int shim_dma_bd15_0 = 0x000092D0;
// DMA BD15 1
const unsigned int shim_dma_bd15_1 = 0x000092D4;
// DMA BD15 2
const unsigned int shim_dma_bd15_2 = 0x000092D8;
// DMA BD15 3
const unsigned int shim_dma_bd15_3 = 0x000092DC;
// DMA BD15 4
const unsigned int shim_dma_bd15_4 = 0x000092E0;
// DMA BD15 5
const unsigned int shim_dma_bd15_5 = 0x000092E4;
// DMA BD15 6
const unsigned int shim_dma_bd15_6 = 0x000092E8;
// DMA BD15 7
const unsigned int shim_dma_bd15_7 = 0x000092EC;
// DMA BD15 8
const unsigned int shim_dma_bd15_8 = 0x000092F0;
// DMA BD1 0
const unsigned int shim_dma_bd1_0 = 0x00009030;
// DMA BD1 1
const unsigned int shim_dma_bd1_1 = 0x00009034;
// DMA BD1 2
const unsigned int shim_dma_bd1_2 = 0x00009038;
// DMA BD1 3
const unsigned int shim_dma_bd1_3 = 0x0000903C;
// DMA BD1 4
const unsigned int shim_dma_bd1_4 = 0x00009040;
// DMA BD1 5
const unsigned int shim_dma_bd1_5 = 0x00009044;
// DMA BD1 6
const unsigned int shim_dma_bd1_6 = 0x00009048;
// DMA BD1 7
const unsigned int shim_dma_bd1_7 = 0x0000904C;
// DMA BD1 8
const unsigned int shim_dma_bd1_8 = 0x00009050;
// DMA BD2 0
const unsigned int shim_dma_bd2_0 = 0x00009060;
// DMA BD2 1
const unsigned int shim_dma_bd2_1 = 0x00009064;
// DMA BD2 2
const unsigned int shim_dma_bd2_2 = 0x00009068;
// DMA BD2 3
const unsigned int shim_dma_bd2_3 = 0x0000906C;
// DMA BD2 4
const unsigned int shim_dma_bd2_4 = 0x00009070;
// DMA BD2 5
const unsigned int shim_dma_bd2_5 = 0x00009074;
// DMA BD2 6
const unsigned int shim_dma_bd2_6 = 0x00009078;
// DMA BD2 7
const unsigned int shim_dma_bd2_7 = 0x0000907C;
// DMA BD2 8
const unsigned int shim_dma_bd2_8 = 0x00009080;
// DMA BD3 0
const unsigned int shim_dma_bd3_0 = 0x00009090;
// DMA BD3 1
const unsigned int shim_dma_bd3_1 = 0x00009094;
// DMA BD3 2
const unsigned int shim_dma_bd3_2 = 0x00009098;
// DMA BD3 3
const unsigned int shim_dma_bd3_3 = 0x0000909C;
// DMA BD3 4
const unsigned int shim_dma_bd3_4 = 0x000090A0;
// DMA BD3 5
const unsigned int shim_dma_bd3_5 = 0x000090A4;
// DMA BD3 6
const unsigned int shim_dma_bd3_6 = 0x000090A8;
// DMA BD3 7
const unsigned int shim_dma_bd3_7 = 0x000090AC;
// DMA BD3 8
const unsigned int shim_dma_bd3_8 = 0x000090B0;
// DMA BD4 0
const unsigned int shim_dma_bd4_0 = 0x000090C0;
// DMA BD4 1
const unsigned int shim_dma_bd4_1 = 0x000090C4;
// DMA BD4 2
const unsigned int shim_dma_bd4_2 = 0x000090C8;
// DMA BD4 3
const unsigned int shim_dma_bd4_3 = 0x000090CC;
// DMA BD4 4
const unsigned int shim_dma_bd4_4 = 0x000090D0;
// DMA BD4 5
const unsigned int shim_dma_bd4_5 = 0x000090D4;
// DMA BD4 6
const unsigned int shim_dma_bd4_6 = 0x000090D8;
// DMA BD4 7
const unsigned int shim_dma_bd4_7 = 0x000090DC;
// DMA BD4 8
const unsigned int shim_dma_bd4_8 = 0x000090E0;
// DMA BD5 0
const unsigned int shim_dma_bd5_0 = 0x000090F0;
// DMA BD5 1
const unsigned int shim_dma_bd5_1 = 0x000090F4;
// DMA BD5 2
const unsigned int shim_dma_bd5_2 = 0x000090F8;
// DMA BD5 3
const unsigned int shim_dma_bd5_3 = 0x000090FC;
// DMA BD5 4
const unsigned int shim_dma_bd5_4 = 0x00009100;
// DMA BD5 5
const unsigned int shim_dma_bd5_5 = 0x00009104;
// DMA BD5 6
const unsigned int shim_dma_bd5_6 = 0x00009108;
// DMA BD5 7
const unsigned int shim_dma_bd5_7 = 0x0000910C;
// DMA BD5 8
const unsigned int shim_dma_bd5_8 = 0x00009110;
// DMA BD6 0
const unsigned int shim_dma_bd6_0 = 0x00009120;
// DMA BD6 1
const unsigned int shim_dma_bd6_1 = 0x00009124;
// DMA BD6 2
const unsigned int shim_dma_bd6_2 = 0x00009128;
// DMA BD6 3
const unsigned int shim_dma_bd6_3 = 0x0000912C;
// DMA BD6 4
const unsigned int shim_dma_bd6_4 = 0x00009130;
// DMA BD6 5
const unsigned int shim_dma_bd6_5 = 0x00009134;
// DMA BD6 6
const unsigned int shim_dma_bd6_6 = 0x00009138;
// DMA BD6 7
const unsigned int shim_dma_bd6_7 = 0x0000913C;
// DMA BD6 8
const unsigned int shim_dma_bd6_8 = 0x00009140;
// DMA BD7 0
const unsigned int shim_dma_bd7_0 = 0x00009150;
// DMA BD7 1
const unsigned int shim_dma_bd7_1 = 0x00009154;
// DMA BD7 2
const unsigned int shim_dma_bd7_2 = 0x00009158;
// DMA BD7 3
const unsigned int shim_dma_bd7_3 = 0x0000915C;
// DMA BD7 4
const unsigned int shim_dma_bd7_4 = 0x00009160;
// DMA BD7 5
const unsigned int shim_dma_bd7_5 = 0x00009164;
// DMA BD7 6
const unsigned int shim_dma_bd7_6 = 0x00009168;
// DMA BD7 7
const unsigned int shim_dma_bd7_7 = 0x0000916C;
// DMA BD7 8
const unsigned int shim_dma_bd7_8 = 0x00009170;
// DMA BD8 0
const unsigned int shim_dma_bd8_0 = 0x00009180;
// DMA BD8 1
const unsigned int shim_dma_bd8_1 = 0x00009184;
// DMA BD8 2
const unsigned int shim_dma_bd8_2 = 0x00009188;
// DMA BD8 3
const unsigned int shim_dma_bd8_3 = 0x0000918C;
// DMA BD8 4
const unsigned int shim_dma_bd8_4 = 0x00009190;
// DMA BD8 5
const unsigned int shim_dma_bd8_5 = 0x00009194;
// DMA BD8 6
const unsigned int shim_dma_bd8_6 = 0x00009198;
// DMA BD8 7
const unsigned int shim_dma_bd8_7 = 0x0000919C;
// DMA BD8 8
const unsigned int shim_dma_bd8_8 = 0x000091A0;
// DMA BD9 0
const unsigned int shim_dma_bd9_0 = 0x000091B0;
// DMA BD9 1
const unsigned int shim_dma_bd9_1 = 0x000091B4;
// DMA BD9 2
const unsigned int shim_dma_bd9_2 = 0x000091B8;
// DMA BD9 3
const unsigned int shim_dma_bd9_3 = 0x000091BC;
// DMA BD9 4
const unsigned int shim_dma_bd9_4 = 0x000091C0;
// DMA BD9 5
const unsigned int shim_dma_bd9_5 = 0x000091C4;
// DMA BD9 6
const unsigned int shim_dma_bd9_6 = 0x000091C8;
// DMA BD9 7
const unsigned int shim_dma_bd9_7 = 0x000091CC;
// DMA BD9 8
const unsigned int shim_dma_bd9_8 = 0x000091D0;
// Step size between DMA BD register groups
const unsigned int shim_dma_bd_step_size = 0x20;
// DMA DM2MM AXI Control
const unsigned int shim_dma_dm2mm_axi_control = 0x000C0108;
// DMA DM2MM Control
const unsigned int shim_dma_dm2mm_control = 0x000C0104;
// DMA DM2MM Status
const unsigned int shim_dma_dm2mm_status = 0x000C0100;
// DMA MM2DM AXI Control
const unsigned int shim_dma_mm2dm_axi_control = 0x000C0118;
// DMA MM2DM Control
const unsigned int shim_dma_mm2dm_control = 0x000C0114;
// DMA MM2DM Status
const unsigned int shim_dma_mm2dm_status = 0x000C0110;
// DMA MM2S 0 Ctrl
const unsigned int shim_dma_mm2s_0_ctrl = 0x00009310;
// DMA MM2S 0 Response FIFO Parity Error Injection
const unsigned int shim_dma_mm2s_0_response_fifo_parity_error_injection = 0x00009340;
// DMA MM2S 0 Task Queue
const unsigned int shim_dma_mm2s_0_task_queue = 0x00009314;
// DMA MM2S 1 Ctrl
const unsigned int shim_dma_mm2s_1_ctrl = 0x00009318;
// DMA MM2S 1 Response FIFO Parity Error Injection
const unsigned int shim_dma_mm2s_1_response_fifo_parity_error_injection = 0x00009344;
// DMA MM2S 1 Task Queue
const unsigned int shim_dma_mm2s_1_task_queue = 0x0000931C;
// DMA MM2S Status 0
const unsigned int shim_dma_mm2s_status_0 = 0x00009328;
// DMA MM2S Status 1
const unsigned int shim_dma_mm2s_status_1 = 0x0000932C;
// DMA Pause
const unsigned int shim_dma_pause = 0x000C0120;
// DMA S2MM 0 Ctrl
const unsigned int shim_dma_s2mm_0_ctrl = 0x00009300;
// DMA S2MM 0 Task Queue
const unsigned int shim_dma_s2mm_0_task_queue = 0x00009304;
// DMA S2MM 1 Ctrl
const unsigned int shim_dma_s2mm_1_ctrl = 0x00009308;
// DMA S2MM 1 Task Queue
const unsigned int shim_dma_s2mm_1_task_queue = 0x0000930C;
// DMA S2MM Current Write Count 0
const unsigned int shim_dma_s2mm_current_write_count_0 = 0x00009330;
// DMA S2MM Current Write Count 1
const unsigned int shim_dma_s2mm_current_write_count_1 = 0x00009334;
// DMA S2MM FoT Count FIFO Pop 0
const unsigned int shim_dma_s2mm_fot_count_fifo_pop_0 = 0x00009338;
// DMA S2MM FoT Count FIFO Pop 1
const unsigned int shim_dma_s2mm_fot_count_fifo_pop_1 = 0x0000933C;
// DMA S2MM Status 0
const unsigned int shim_dma_s2mm_status_0 = 0x00009320;
// DMA S2MM Status 1
const unsigned int shim_dma_s2mm_status_1 = 0x00009324;
// Step size between DMA S2MM register groups
const unsigned int shim_dma_s2mm_step_size = 0x8;
// Edge Detection event control
const unsigned int shim_edge_detection_event_control = 0x00034408;
// Control of which Internal Event to Broadcast0
const unsigned int shim_event_broadcast_a_0 = 0x00034010;
// Control of which Internal Event to Broadcast1
const unsigned int shim_event_broadcast_a_1 = 0x00034014;
// Control of which Internal Event to Broadcast10
const unsigned int shim_event_broadcast_a_10 = 0x00034038;
// Control of which Internal Event to Broadcast11
const unsigned int shim_event_broadcast_a_11 = 0x0003403C;
// Control of which Internal Event to Broadcast12
const unsigned int shim_event_broadcast_a_12 = 0x00034040;
// Control of which Internal Event to Broadcast13
const unsigned int shim_event_broadcast_a_13 = 0x00034044;
// Control of which Internal Event to Broadcast14
const unsigned int shim_event_broadcast_a_14 = 0x00034048;
// Control of which Internal Event to Broadcast15
const unsigned int shim_event_broadcast_a_15 = 0x0003404C;
// Control of which Internal Event to Broadcast2
const unsigned int shim_event_broadcast_a_2 = 0x00034018;
// Control of which Internal Event to Broadcast3
const unsigned int shim_event_broadcast_a_3 = 0x0003401C;
// Control of which Internal Event to Broadcast4
const unsigned int shim_event_broadcast_a_4 = 0x00034020;
// Control of which Internal Event to Broadcast5
const unsigned int shim_event_broadcast_a_5 = 0x00034024;
// Control of which Internal Event to Broadcast6
const unsigned int shim_event_broadcast_a_6 = 0x00034028;
// Control of which Internal Event to Broadcast7
const unsigned int shim_event_broadcast_a_7 = 0x0003402C;
// Control of which Internal Event to Broadcast8
const unsigned int shim_event_broadcast_a_8 = 0x00034030;
// Control of which Internal Event to Broadcast9
const unsigned int shim_event_broadcast_a_9 = 0x00034034;
// Event Broadcast A Block East Clr
const unsigned int shim_event_broadcast_a_block_east_clr = 0x00034084;
// Event Broadcast A Block East Set
const unsigned int shim_event_broadcast_a_block_east_set = 0x00034080;
// Event Broadcast A Block East Value
const unsigned int shim_event_broadcast_a_block_east_value = 0x00034088;
// Event Broadcast A Block North Clr
const unsigned int shim_event_broadcast_a_block_north_clr = 0x00034074;
// Event Broadcast A Block North Set
const unsigned int shim_event_broadcast_a_block_north_set = 0x00034070;
// Event Broadcast A Block North Value
const unsigned int shim_event_broadcast_a_block_north_value = 0x00034078;
// Event Broadcast A Block South Clr
const unsigned int shim_event_broadcast_a_block_south_clr = 0x00034054;
// Event Broadcast A Block South Set
const unsigned int shim_event_broadcast_a_block_south_set = 0x00034050;
// Event Broadcast A Block South Value
const unsigned int shim_event_broadcast_a_block_south_value = 0x00034058;
// Event Broadcast A Block West Clr
const unsigned int shim_event_broadcast_a_block_west_clr = 0x00034064;
// Event Broadcast A Block West Set
const unsigned int shim_event_broadcast_a_block_west_set = 0x00034060;
// Event Broadcast A Block West Value
const unsigned int shim_event_broadcast_a_block_west_value = 0x00034068;
// Event Broadcast B Block East Clr
const unsigned int shim_event_broadcast_b_block_east_clr = 0x000340C4;
// Event Broadcast B Block East Set
const unsigned int shim_event_broadcast_b_block_east_set = 0x000340C0;
// Event Broadcast B Block East Value
const unsigned int shim_event_broadcast_b_block_east_value = 0x000340C8;
// Event Broadcast B Block North Clr
const unsigned int shim_event_broadcast_b_block_north_clr = 0x000340B4;
// Event Broadcast B Block North Set
const unsigned int shim_event_broadcast_b_block_north_set = 0x000340B0;
// Event Broadcast B Block North Value
const unsigned int shim_event_broadcast_b_block_north_value = 0x000340B8;
// Event Broadcast B Block South Clr
const unsigned int shim_event_broadcast_b_block_south_clr = 0x00034094;
// Event Broadcast B Block South Set
const unsigned int shim_event_broadcast_b_block_south_set = 0x00034090;
// Event Broadcast B Block South Value
const unsigned int shim_event_broadcast_b_block_south_value = 0x00034098;
// Event Broadcast B Block West Clr
const unsigned int shim_event_broadcast_b_block_west_clr = 0x000340A4;
// Event Broadcast B Block West Set
const unsigned int shim_event_broadcast_b_block_west_set = 0x000340A0;
// Event Broadcast B Block West Value
const unsigned int shim_event_broadcast_b_block_west_value = 0x000340A8;
// Event Generate
const unsigned int shim_event_generate = 0x00034008;
// Event Group 0 Enable
const unsigned int shim_event_group_0_enable = 0x00034500;
// Event Group Broadcast A Enable
const unsigned int shim_event_group_broadcast_a_enable = 0x0003451C;
// Event enable for DMA Group
const unsigned int shim_event_group_dma_enable = 0x00034504;
// Event Group Errors Enable
const unsigned int shim_event_group_errors_enable = 0x00034514;
// Event Group NoC 0 Lock Enable
const unsigned int shim_event_group_noc_0_lock_enable = 0x0003450C;
// Event Group NoC 1 DMA Activity Enable
const unsigned int shim_event_group_noc_1_dma_activity_enable = 0x00034508;
// Event Group NoC 1 Lock Enable
const unsigned int shim_event_group_noc_1_lock_enable = 0x00034510;
// Event Group Stream Switch Enable
const unsigned int shim_event_group_stream_switch_enable = 0x00034518;
// Event Group uC Core Program Flow Enable
const unsigned int shim_event_group_uc_core_program_flow_enable = 0x0003452C;
// Event Group uC Core Streams Enable
const unsigned int shim_event_group_uc_core_streams_enable = 0x00034528;
// Event Group uC DMA Activity Enable
const unsigned int shim_event_group_uc_dma_activity_enable = 0x00034520;
// Event Group uC Module Errors Enable
const unsigned int shim_event_group_uc_module_errors_enable = 0x00034524;
// Event Status0
const unsigned int shim_event_status0 = 0x00034200;
// Event Status1
const unsigned int shim_event_status1 = 0x00034204;
// Event Status2
const unsigned int shim_event_status2 = 0x00034208;
// Event Status3
const unsigned int shim_event_status3 = 0x0003420C;
// Event Status4
const unsigned int shim_event_status4 = 0x00034210;
// Event Status5
const unsigned int shim_event_status5 = 0x00034214;
// Event Status6
const unsigned int shim_event_status6 = 0x00034218;
// Event Status7
const unsigned int shim_event_status7 = 0x0003421C;
// Interrupt controller 1st level block north in A clear
const unsigned int shim_interrupt_controller_1st_level_block_north_in_a_clear = 0x0003501C;
// Interrupt controller 1st level block north in A set
const unsigned int shim_interrupt_controller_1st_level_block_north_in_a_set = 0x00035018;
// Interrupt controller 1st level block north in A value
const unsigned int shim_interrupt_controller_1st_level_block_north_in_a_value = 0x00035020;
// Interrupt controller 1st level block north in B clear
const unsigned int shim_interrupt_controller_1st_level_block_north_in_b_clear = 0x0003504C;
// Interrupt controller 1st level block north in B set
const unsigned int shim_interrupt_controller_1st_level_block_north_in_b_set = 0x00035048;
// Interrupt controller 1st level block north in B value
const unsigned int shim_interrupt_controller_1st_level_block_north_in_b_value = 0x00035050;
// Interrupt controller 1st level disable A
const unsigned int shim_interrupt_controller_1st_level_disable_a = 0x00035008;
// Interrupt controller 1st level disable B
const unsigned int shim_interrupt_controller_1st_level_disable_b = 0x00035038;
// Interrupt controller 1st level enable A
const unsigned int shim_interrupt_controller_1st_level_enable_a = 0x00035004;
// Interrupt controller 1st level enable B
const unsigned int shim_interrupt_controller_1st_level_enable_b = 0x00035034;
// Interrupt controller 1st level irq event A
const unsigned int shim_interrupt_controller_1st_level_irq_event_a = 0x00035014;
// Interrupt controller 1st level irq event B
const unsigned int shim_interrupt_controller_1st_level_irq_event_b = 0x00035044;
// Interrupt controller 1st level irq no A
const unsigned int shim_interrupt_controller_1st_level_irq_no_a = 0x00035010;
// Interrupt controller 1st level irq no B
const unsigned int shim_interrupt_controller_1st_level_irq_no_b = 0x00035040;
// Interrupt controller 1st level mask A
const unsigned int shim_interrupt_controller_1st_level_mask_a = 0x00035000;
// Interrupt controller 1st level mask B
const unsigned int shim_interrupt_controller_1st_level_mask_b = 0x00035030;
// Interrupt controller 1st level status A
const unsigned int shim_interrupt_controller_1st_level_status_a = 0x0003500C;
// Interrupt controller 1st level status B
const unsigned int shim_interrupt_controller_1st_level_status_b = 0x0003503C;
// Interrupt controller 2nd level disable
const unsigned int shim_interrupt_controller_2nd_level_disable = 0x00001008;
// Interrupt controller 2nd level enable
const unsigned int shim_interrupt_controller_2nd_level_enable = 0x00001004;
// Interrupt controller 2nd level interrupt
const unsigned int shim_interrupt_controller_2nd_level_interrupt = 0x00001010;
// Interrupt controller 2nd level mask
const unsigned int shim_interrupt_controller_2nd_level_mask = 0x00001000;
// Interrupt controller 2nd level status
const unsigned int shim_interrupt_controller_2nd_level_status = 0x0000100C;
// Interrupt controller Hw Error interrupt
const unsigned int shim_interrupt_controller_hw_error_interrupt = 0x0007FF58;
// Interrupt controller Hw Error mask
const unsigned int shim_interrupt_controller_hw_error_mask = 0x0007FF50;
// Interrupt controller Hw Error status
const unsigned int shim_interrupt_controller_hw_error_status = 0x0007FF54;
// Lock0 value
const unsigned int shim_lock0_value = 0x00000000;
// Lock10 value
const unsigned int shim_lock10_value = 0x000000A0;
// Lock11 value
const unsigned int shim_lock11_value = 0x000000B0;
// Lock12 value
const unsigned int shim_lock12_value = 0x000000C0;
// Lock13 value
const unsigned int shim_lock13_value = 0x000000D0;
// Lock14 value
const unsigned int shim_lock14_value = 0x000000E0;
// Lock15 value
const unsigned int shim_lock15_value = 0x000000F0;
// Lock1 value
const unsigned int shim_lock1_value = 0x00000010;
// Lock2 value
const unsigned int shim_lock2_value = 0x00000020;
// Lock3 value
const unsigned int shim_lock3_value = 0x00000030;
// Lock4 value
const unsigned int shim_lock4_value = 0x00000040;
// Lock5 value
const unsigned int shim_lock5_value = 0x00000050;
// Lock6 value
const unsigned int shim_lock6_value = 0x00000060;
// Lock7 value
const unsigned int shim_lock7_value = 0x00000070;
// Lock8 value
const unsigned int shim_lock8_value = 0x00000080;
// Lock9 value
const unsigned int shim_lock9_value = 0x00000090;
// Lock Request
const unsigned int shim_lock_request = 0x0000C000;
// Step size between lock registers
const unsigned int shim_lock_step_size = 0x10;
// Locks Event Selection 0
const unsigned int shim_locks_event_selection_0 = 0x00000100;
// Locks Event Selection 1
const unsigned int shim_locks_event_selection_1 = 0x00000104;
// Locks Event Selection 2
const unsigned int shim_locks_event_selection_2 = 0x00000108;
// Locks Event Selection 3
const unsigned int shim_locks_event_selection_3 = 0x0000010C;
// Locks Event Selection 4
const unsigned int shim_locks_event_selection_4 = 0x00000110;
// Locks Event Selection 5
const unsigned int shim_locks_event_selection_5 = 0x00000114;
// Locks Overflow
const unsigned int shim_locks_overflow = 0x00000120;
// Locks Underflow
const unsigned int shim_locks_underflow = 0x00000128;
// MDM Dbg Ctrl Status
const unsigned int shim_mdm_dbg_ctrl_status = 0x000B0010;
// MDM Dbg Data
const unsigned int shim_mdm_dbg_data = 0x000B0014;
// MDM Dbg Lock
const unsigned int shim_mdm_dbg_lock = 0x000B0018;
// MDM PCCMDR
const unsigned int shim_mdm_pccmdr = 0x000B5480;
// MDM PCCTRLR
const unsigned int shim_mdm_pcctrlr = 0x000B5440;
// MDM PCDRR
const unsigned int shim_mdm_pcdrr = 0x000B5580;
// MDM PCSR
const unsigned int shim_mdm_pcsr = 0x000B54C0;
// MDM PCWR
const unsigned int shim_mdm_pcwr = 0x000B55C0;
// ME AXIMM Config
const unsigned int shim_me_aximm_config = 0x00002100;
// Memory DM ECC Error Generation
const unsigned int shim_memory_dm_ecc_error_generation = 0x000C003C;
// Memory DM ECC Scrubbing Period
const unsigned int shim_memory_dm_ecc_scrubbing_period = 0x000C0038;
// Memory Privileged
const unsigned int shim_memory_privileged = 0x000C0034;
// Memory Zeroization
const unsigned int shim_memory_zeroization = 0x000C0030;
// Module AXI MM Outstanding Transactions
const unsigned int shim_module_axi_mm_outstanding_transactions = 0x000C0024;
// Module AXIMM Offset
const unsigned int shim_module_aximm_offset = 0x000C0020;
// Module Clock Control 0
const unsigned int shim_module_clock_control_0 = 0x0007FF00;
// Module Clock Control 1
const unsigned int shim_module_clock_control_1 = 0x0007FF04;
// Module Reset Control 0
const unsigned int shim_module_reset_control_0 = 0x0007FF10;
// Module Reset Control 1
const unsigned int shim_module_reset_control_1 = 0x0007FF14;
// Mux Config
const unsigned int shim_mux_config = 0x00002104;
// NMU Switches Config
const unsigned int shim_nmu_switches_config = 0x0007FF48;
// Performance Counters 1-0 Start and Stop Events
const unsigned int shim_performance_control0 = 0x00031000;
const unsigned int shim_performance_start_stop_0_1 = 0x00031000;
// Performance Counters 1-0 Reset Events
const unsigned int shim_performance_control1 = 0x00031008;
const unsigned int shim_performance_reset_0_1 = 0x00031008;
// Performance Counters 3-2 Start and Stop Events
const unsigned int shim_performance_control2 = 0x0003100C;
const unsigned int shim_performance_start_stop_2_3 = 0x0003100C;
// Performance Counters 3-2 Reset Events
const unsigned int shim_performance_control3 = 0x00031010;
const unsigned int shim_performance_reset_2_3 = 0x00031010;
// Performance Counters 5-4 Start and Stop Events
const unsigned int shim_performance_control4 = 0x00031014;
const unsigned int shim_performance_start_stop_4_5 = 0x00031014;
// Performance Counters 5-4 Reset Events
const unsigned int shim_performance_control5 = 0x00031018;
const unsigned int shim_performance_reset_4_5 = 0x00031018;
// Performance Counter0
const unsigned int shim_performance_counter0 = 0x00031020;
// Performance Counter0 Event Value
const unsigned int shim_performance_counter0_event_value = 0x00031080;
// Performance Counter1
const unsigned int shim_performance_counter1 = 0x00031024;
// Performance Counter1 Event Value
const unsigned int shim_performance_counter1_event_value = 0x00031084;
// Performance Counter2
const unsigned int shim_performance_counter2 = 0x00031028;
// Performance Counter3
const unsigned int shim_performance_counter3 = 0x0003102C;
// Performance Counter4
const unsigned int shim_performance_counter4 = 0x00031030;
// Performance Counter5
const unsigned int shim_performance_counter5 = 0x00031034;
// PL Interface Downsizer Bypass
const unsigned int shim_pl_interface_downsizer_bypass = 0x0003000C;
// PL Interface Downsizer Config
const unsigned int shim_pl_interface_downsizer_config = 0x00030004;
// PL Interface Downsizer Enable
const unsigned int shim_pl_interface_downsizer_enable = 0x00030008;
// PL Interface Upsizer Config
const unsigned int shim_pl_interface_upsizer_config = 0x00030000;
// SMID
const unsigned int shim_smid = 0x00003000;
// Spare Reg
const unsigned int shim_spare_reg = 0x0007FF34;
// Spare Reg Privileged
const unsigned int shim_spare_reg_privileged = 0x0007FF30;
// Stream Switch Adaptive Clock Gate Abort Period
const unsigned int shim_stream_switch_adaptive_clock_gate_abort_period = 0x0003FF38;
// Stream Switch Adaptive Clock Gate Status
const unsigned int shim_stream_switch_adaptive_clock_gate_status = 0x0003FF34;
// Stream Switch Deterministic Merge arb0 ctrl
const unsigned int shim_stream_switch_deterministic_merge_arb0_ctrl = 0x0003F808;
// Stream Switch Deterministic Merge arb0 slave0 1
const unsigned int shim_stream_switch_deterministic_merge_arb0_slave0_1 = 0x0003F800;
// Stream Switch Deterministic Merge arb0 slave2 3
const unsigned int shim_stream_switch_deterministic_merge_arb0_slave2_3 = 0x0003F804;
// Stream Switch Deterministic Merge arb1 ctrl
const unsigned int shim_stream_switch_deterministic_merge_arb1_ctrl = 0x0003F818;
// Stream Switch Deterministic Merge arb1 slave0 1
const unsigned int shim_stream_switch_deterministic_merge_arb1_slave0_1 = 0x0003F810;
// Stream Switch Deterministic Merge arb1 slave2 3
const unsigned int shim_stream_switch_deterministic_merge_arb1_slave2_3 = 0x0003F814;
// Stream Switch Event Port Selection 0
const unsigned int shim_stream_switch_event_port_selection_0 = 0x0003FF00;
// Stream Switch Event Port Selection 1
const unsigned int shim_stream_switch_event_port_selection_1 = 0x0003FF04;
// Stream Switch Master Config East0
const unsigned int shim_stream_switch_master_config_east0 = 0x0003F048;
// Stream Switch Master Config East1
const unsigned int shim_stream_switch_master_config_east1 = 0x0003F04C;
// Stream Switch Master Config East2
const unsigned int shim_stream_switch_master_config_east2 = 0x0003F050;
// Stream Switch Master Config East3
const unsigned int shim_stream_switch_master_config_east3 = 0x0003F054;
// Stream Switch Master Config FIFO0
const unsigned int shim_stream_switch_master_config_fifo0 = 0x0003F004;
// Stream Switch Master Config North0
const unsigned int shim_stream_switch_master_config_north0 = 0x0003F030;
// Stream Switch Master Config North1
const unsigned int shim_stream_switch_master_config_north1 = 0x0003F034;
// Stream Switch Master Config North2
const unsigned int shim_stream_switch_master_config_north2 = 0x0003F038;
// Stream Switch Master Config North3
const unsigned int shim_stream_switch_master_config_north3 = 0x0003F03C;
// Stream Switch Master Config North4
const unsigned int shim_stream_switch_master_config_north4 = 0x0003F040;
// Stream Switch Master Config North5
const unsigned int shim_stream_switch_master_config_north5 = 0x0003F044;
// Stream Switch Master Config South0
const unsigned int shim_stream_switch_master_config_south0 = 0x0003F008;
// Stream Switch Master Config South1
const unsigned int shim_stream_switch_master_config_south1 = 0x0003F00C;
// Stream Switch Master Config South2
const unsigned int shim_stream_switch_master_config_south2 = 0x0003F010;
// Stream Switch Master Config South3
const unsigned int shim_stream_switch_master_config_south3 = 0x0003F014;
// Stream Switch Master Config South4
const unsigned int shim_stream_switch_master_config_south4 = 0x0003F018;
// Stream Switch Master Config South5
const unsigned int shim_stream_switch_master_config_south5 = 0x0003F01C;
// Stream Switch Master Config Tile Ctrl
const unsigned int shim_stream_switch_master_config_tile_ctrl = 0x0003F000;
// Stream Switch Master Config uController
const unsigned int shim_stream_switch_master_config_ucontroller = 0x0003F058;
// Stream Switch Master Config West0
const unsigned int shim_stream_switch_master_config_west0 = 0x0003F020;
// Stream Switch Master Config West1
const unsigned int shim_stream_switch_master_config_west1 = 0x0003F024;
// Stream Switch Master Config West2
const unsigned int shim_stream_switch_master_config_west2 = 0x0003F028;
// Stream Switch Master Config West3
const unsigned int shim_stream_switch_master_config_west3 = 0x0003F02C;
// Stream Switch Parity Injection
const unsigned int shim_stream_switch_parity_injection = 0x0003FF20;
// Stream Switch Parity Status
const unsigned int shim_stream_switch_parity_status = 0x0003FF10;
// Stream Switch Slave Config East 0
const unsigned int shim_stream_switch_slave_config_east_0 = 0x0003F148;
// Stream Switch Slave Config East 1
const unsigned int shim_stream_switch_slave_config_east_1 = 0x0003F14C;
// Stream Switch Slave Config East 2
const unsigned int shim_stream_switch_slave_config_east_2 = 0x0003F150;
// Stream Switch Slave Config East 3
const unsigned int shim_stream_switch_slave_config_east_3 = 0x0003F154;
// Stream Switch Slave Config FIFO 0
const unsigned int shim_stream_switch_slave_config_fifo_0 = 0x0003F104;
// Stream Switch Slave Config North 0
const unsigned int shim_stream_switch_slave_config_north_0 = 0x0003F138;
// Stream Switch Slave Config North 1
const unsigned int shim_stream_switch_slave_config_north_1 = 0x0003F13C;
// Stream Switch Slave Config North 2
const unsigned int shim_stream_switch_slave_config_north_2 = 0x0003F140;
// Stream Switch Slave Config North 3
const unsigned int shim_stream_switch_slave_config_north_3 = 0x0003F144;
// Stream Switch Slave Config South 0
const unsigned int shim_stream_switch_slave_config_south_0 = 0x0003F108;
// Stream Switch Slave Config South 1
const unsigned int shim_stream_switch_slave_config_south_1 = 0x0003F10C;
// Stream Switch Slave Config South 2
const unsigned int shim_stream_switch_slave_config_south_2 = 0x0003F110;
// Stream Switch Slave Config South 3
const unsigned int shim_stream_switch_slave_config_south_3 = 0x0003F114;
// Stream Switch Slave Config South 4
const unsigned int shim_stream_switch_slave_config_south_4 = 0x0003F118;
// Stream Switch Slave Config South 5
const unsigned int shim_stream_switch_slave_config_south_5 = 0x0003F11C;
// Stream Switch Slave Config South 6
const unsigned int shim_stream_switch_slave_config_south_6 = 0x0003F120;
// Stream Switch Slave Config South 7
const unsigned int shim_stream_switch_slave_config_south_7 = 0x0003F124;
// Stream Switch Slave Config Tile Ctrl
const unsigned int shim_stream_switch_slave_config_tile_ctrl = 0x0003F100;
// Stream Switch Slave Config Trace
const unsigned int shim_stream_switch_slave_config_trace = 0x0003F158;
// Stream Switch Slave Config uController
const unsigned int shim_stream_switch_slave_config_ucontroller = 0x0003F15C;
// Stream Switch Slave Config West 0
const unsigned int shim_stream_switch_slave_config_west_0 = 0x0003F128;
// Stream Switch Slave Config West 1
const unsigned int shim_stream_switch_slave_config_west_1 = 0x0003F12C;
// Stream Switch Slave Config West 2
const unsigned int shim_stream_switch_slave_config_west_2 = 0x0003F130;
// Stream Switch Slave Config West 3
const unsigned int shim_stream_switch_slave_config_west_3 = 0x0003F134;
// Stream Switch Slave East 0 Slot0
const unsigned int shim_stream_switch_slave_east_0_slot0 = 0x0003F320;
// Stream Switch Slave East 0 Slot1
const unsigned int shim_stream_switch_slave_east_0_slot1 = 0x0003F324;
// Stream Switch Slave East 0 Slot2
const unsigned int shim_stream_switch_slave_east_0_slot2 = 0x0003F328;
// Stream Switch Slave East 0 Slot3
const unsigned int shim_stream_switch_slave_east_0_slot3 = 0x0003F32C;
// Stream Switch Slave East 1 Slot0
const unsigned int shim_stream_switch_slave_east_1_slot0 = 0x0003F330;
// Stream Switch Slave East 1 Slot1
const unsigned int shim_stream_switch_slave_east_1_slot1 = 0x0003F334;
// Stream Switch Slave East 1 Slot2
const unsigned int shim_stream_switch_slave_east_1_slot2 = 0x0003F338;
// Stream Switch Slave East 1 Slot3
const unsigned int shim_stream_switch_slave_east_1_slot3 = 0x0003F33C;
// Stream Switch Slave East 2 Slot0
const unsigned int shim_stream_switch_slave_east_2_slot0 = 0x0003F340;
// Stream Switch Slave East 2 Slot1
const unsigned int shim_stream_switch_slave_east_2_slot1 = 0x0003F344;
// Stream Switch Slave East 2 Slot2
const unsigned int shim_stream_switch_slave_east_2_slot2 = 0x0003F348;
// Stream Switch Slave East 2 Slot3
const unsigned int shim_stream_switch_slave_east_2_slot3 = 0x0003F34C;
// Stream Switch Slave East 3 Slot0
const unsigned int shim_stream_switch_slave_east_3_slot0 = 0x0003F350;
// Stream Switch Slave East 3 Slot1
const unsigned int shim_stream_switch_slave_east_3_slot1 = 0x0003F354;
// Stream Switch Slave East 3 Slot2
const unsigned int shim_stream_switch_slave_east_3_slot2 = 0x0003F358;
// Stream Switch Slave East 3 Slot3
const unsigned int shim_stream_switch_slave_east_3_slot3 = 0x0003F35C;
// Stream Switch Slave FIFO 0 Slot0
const unsigned int shim_stream_switch_slave_fifo_0_slot0 = 0x0003F210;
// Stream Switch Slave FIFO 0 Slot1
const unsigned int shim_stream_switch_slave_fifo_0_slot1 = 0x0003F214;
// Stream Switch Slave FIFO 0 Slot2
const unsigned int shim_stream_switch_slave_fifo_0_slot2 = 0x0003F218;
// Stream Switch Slave FIFO 0 Slot3
const unsigned int shim_stream_switch_slave_fifo_0_slot3 = 0x0003F21C;
// Stream Switch Slave North 0 Slot0
const unsigned int shim_stream_switch_slave_north_0_slot0 = 0x0003F2E0;
// Stream Switch Slave North 0 Slot1
const unsigned int shim_stream_switch_slave_north_0_slot1 = 0x0003F2E4;
// Stream Switch Slave North 0 Slot2
const unsigned int shim_stream_switch_slave_north_0_slot2 = 0x0003F2E8;
// Stream Switch Slave North 0 Slot3
const unsigned int shim_stream_switch_slave_north_0_slot3 = 0x0003F2EC;
// Stream Switch Slave North 1 Slot0
const unsigned int shim_stream_switch_slave_north_1_slot0 = 0x0003F2F0;
// Stream Switch Slave North 1 Slot1
const unsigned int shim_stream_switch_slave_north_1_slot1 = 0x0003F2F4;
// Stream Switch Slave North 1 Slot2
const unsigned int shim_stream_switch_slave_north_1_slot2 = 0x0003F2F8;
// Stream Switch Slave North 1 Slot3
const unsigned int shim_stream_switch_slave_north_1_slot3 = 0x0003F2FC;
// Stream Switch Slave North 2 Slot0
const unsigned int shim_stream_switch_slave_north_2_slot0 = 0x0003F300;
// Stream Switch Slave North 2 Slot1
const unsigned int shim_stream_switch_slave_north_2_slot1 = 0x0003F304;
// Stream Switch Slave North 2 Slot2
const unsigned int shim_stream_switch_slave_north_2_slot2 = 0x0003F308;
// Stream Switch Slave North 2 Slot3
const unsigned int shim_stream_switch_slave_north_2_slot3 = 0x0003F30C;
// Stream Switch Slave North 3 Slot0
const unsigned int shim_stream_switch_slave_north_3_slot0 = 0x0003F310;
// Stream Switch Slave North 3 Slot1
const unsigned int shim_stream_switch_slave_north_3_slot1 = 0x0003F314;
// Stream Switch Slave North 3 Slot2
const unsigned int shim_stream_switch_slave_north_3_slot2 = 0x0003F318;
// Stream Switch Slave North 3 Slot3
const unsigned int shim_stream_switch_slave_north_3_slot3 = 0x0003F31C;
// Stream Switch Slave South 0 Slot0
const unsigned int shim_stream_switch_slave_south_0_slot0 = 0x0003F220;
// Stream Switch Slave South 0 Slot1
const unsigned int shim_stream_switch_slave_south_0_slot1 = 0x0003F224;
// Stream Switch Slave South 0 Slot2
const unsigned int shim_stream_switch_slave_south_0_slot2 = 0x0003F228;
// Stream Switch Slave South 0 Slot3
const unsigned int shim_stream_switch_slave_south_0_slot3 = 0x0003F22C;
// Stream Switch Slave South 1 Slot0
const unsigned int shim_stream_switch_slave_south_1_slot0 = 0x0003F230;
// Stream Switch Slave South 1 Slot1
const unsigned int shim_stream_switch_slave_south_1_slot1 = 0x0003F234;
// Stream Switch Slave South 1 Slot2
const unsigned int shim_stream_switch_slave_south_1_slot2 = 0x0003F238;
// Stream Switch Slave South 1 Slot3
const unsigned int shim_stream_switch_slave_south_1_slot3 = 0x0003F23C;
// Stream Switch Slave South 2 Slot0
const unsigned int shim_stream_switch_slave_south_2_slot0 = 0x0003F240;
// Stream Switch Slave South 2 Slot1
const unsigned int shim_stream_switch_slave_south_2_slot1 = 0x0003F244;
// Stream Switch Slave South 2 Slot2
const unsigned int shim_stream_switch_slave_south_2_slot2 = 0x0003F248;
// Stream Switch Slave South 2 Slot3
const unsigned int shim_stream_switch_slave_south_2_slot3 = 0x0003F24C;
// Stream Switch Slave South 3 Slot0
const unsigned int shim_stream_switch_slave_south_3_slot0 = 0x0003F250;
// Stream Switch Slave South 3 Slot1
const unsigned int shim_stream_switch_slave_south_3_slot1 = 0x0003F254;
// Stream Switch Slave South 3 Slot2
const unsigned int shim_stream_switch_slave_south_3_slot2 = 0x0003F258;
// Stream Switch Slave South 3 Slot3
const unsigned int shim_stream_switch_slave_south_3_slot3 = 0x0003F25C;
// Stream Switch Slave South 4 Slot0
const unsigned int shim_stream_switch_slave_south_4_slot0 = 0x0003F260;
// Stream Switch Slave South 4 Slot1
const unsigned int shim_stream_switch_slave_south_4_slot1 = 0x0003F264;
// Stream Switch Slave South 4 Slot2
const unsigned int shim_stream_switch_slave_south_4_slot2 = 0x0003F268;
// Stream Switch Slave South 4 Slot3
const unsigned int shim_stream_switch_slave_south_4_slot3 = 0x0003F26C;
// Stream Switch Slave South 5 Slot0
const unsigned int shim_stream_switch_slave_south_5_slot0 = 0x0003F270;
// Stream Switch Slave South 5 Slot1
const unsigned int shim_stream_switch_slave_south_5_slot1 = 0x0003F274;
// Stream Switch Slave South 5 Slot2
const unsigned int shim_stream_switch_slave_south_5_slot2 = 0x0003F278;
// Stream Switch Slave South 5 Slot3
const unsigned int shim_stream_switch_slave_south_5_slot3 = 0x0003F27C;
// Stream Switch Slave South 6 Slot0
const unsigned int shim_stream_switch_slave_south_6_slot0 = 0x0003F280;
// Stream Switch Slave South 6 Slot1
const unsigned int shim_stream_switch_slave_south_6_slot1 = 0x0003F284;
// Stream Switch Slave South 6 Slot2
const unsigned int shim_stream_switch_slave_south_6_slot2 = 0x0003F288;
// Stream Switch Slave South 6 Slot3
const unsigned int shim_stream_switch_slave_south_6_slot3 = 0x0003F28C;
// Stream Switch Slave South 7 Slot0
const unsigned int shim_stream_switch_slave_south_7_slot0 = 0x0003F290;
// Stream Switch Slave South 7 Slot1
const unsigned int shim_stream_switch_slave_south_7_slot1 = 0x0003F294;
// Stream Switch Slave South 7 Slot2
const unsigned int shim_stream_switch_slave_south_7_slot2 = 0x0003F298;
// Stream Switch Slave South 7 Slot3
const unsigned int shim_stream_switch_slave_south_7_slot3 = 0x0003F29C;
// Stream Switch Slave Tile Ctrl Slot0
const unsigned int shim_stream_switch_slave_tile_ctrl_slot0 = 0x0003F200;
// Stream Switch Slave Tile Ctrl Slot1
const unsigned int shim_stream_switch_slave_tile_ctrl_slot1 = 0x0003F204;
// Stream Switch Slave Tile Ctrl Slot2
const unsigned int shim_stream_switch_slave_tile_ctrl_slot2 = 0x0003F208;
// Stream Switch Slave Tile Ctrl Slot3
const unsigned int shim_stream_switch_slave_tile_ctrl_slot3 = 0x0003F20C;
// Stream Switch Slave Trace Slot0
const unsigned int shim_stream_switch_slave_trace_slot0 = 0x0003F360;
// Stream Switch Slave Trace Slot1
const unsigned int shim_stream_switch_slave_trace_slot1 = 0x0003F364;
// Stream Switch Slave Trace Slot2
const unsigned int shim_stream_switch_slave_trace_slot2 = 0x0003F368;
// Stream Switch Slave Trace Slot3
const unsigned int shim_stream_switch_slave_trace_slot3 = 0x0003F36C;
// Stream Switch Slave uController Slot0
const unsigned int shim_stream_switch_slave_ucontroller_slot0 = 0x0003F370;
// Stream Switch Slave uController Slot1
const unsigned int shim_stream_switch_slave_ucontroller_slot1 = 0x0003F374;
// Stream Switch Slave uController Slot2
const unsigned int shim_stream_switch_slave_ucontroller_slot2 = 0x0003F378;
// Stream Switch Slave uController Slot3
const unsigned int shim_stream_switch_slave_ucontroller_slot3 = 0x0003F37C;
// Stream Switch Slave West 0 Slot0
const unsigned int shim_stream_switch_slave_west_0_slot0 = 0x0003F2A0;
// Stream Switch Slave West 0 Slot1
const unsigned int shim_stream_switch_slave_west_0_slot1 = 0x0003F2A4;
// Stream Switch Slave West 0 Slot2
const unsigned int shim_stream_switch_slave_west_0_slot2 = 0x0003F2A8;
// Stream Switch Slave West 0 Slot3
const unsigned int shim_stream_switch_slave_west_0_slot3 = 0x0003F2AC;
// Stream Switch Slave West 1 Slot0
const unsigned int shim_stream_switch_slave_west_1_slot0 = 0x0003F2B0;
// Stream Switch Slave West 1 Slot1
const unsigned int shim_stream_switch_slave_west_1_slot1 = 0x0003F2B4;
// Stream Switch Slave West 1 Slot2
const unsigned int shim_stream_switch_slave_west_1_slot2 = 0x0003F2B8;
// Stream Switch Slave West 1 Slot3
const unsigned int shim_stream_switch_slave_west_1_slot3 = 0x0003F2BC;
// Stream Switch Slave West 2 Slot0
const unsigned int shim_stream_switch_slave_west_2_slot0 = 0x0003F2C0;
// Stream Switch Slave West 2 Slot1
const unsigned int shim_stream_switch_slave_west_2_slot1 = 0x0003F2C4;
// Stream Switch Slave West 2 Slot2
const unsigned int shim_stream_switch_slave_west_2_slot2 = 0x0003F2C8;
// Stream Switch Slave West 2 Slot3
const unsigned int shim_stream_switch_slave_west_2_slot3 = 0x0003F2CC;
// Stream Switch Slave West 3 Slot0
const unsigned int shim_stream_switch_slave_west_3_slot0 = 0x0003F2D0;
// Stream Switch Slave West 3 Slot1
const unsigned int shim_stream_switch_slave_west_3_slot1 = 0x0003F2D4;
// Stream Switch Slave West 3 Slot2
const unsigned int shim_stream_switch_slave_west_3_slot2 = 0x0003F2D8;
// Stream Switch Slave West 3 Slot3
const unsigned int shim_stream_switch_slave_west_3_slot3 = 0x0003F2DC;
// Tile Control
const unsigned int shim_tile_control = 0x0007FF40;
// Tile Control AXI MM
const unsigned int shim_tile_control_axi_mm = 0x0007FF44;
// Timer Control
const unsigned int shim_timer_control = 0x00034000;
// Timer High
const unsigned int shim_timer_high = 0x000340FC;
// Timer Low
const unsigned int shim_timer_low = 0x000340F8;
// Timer Trig Event High Value
const unsigned int shim_timer_trig_event_high_value = 0x000340F4;
// Timer Trig Event Low Value
const unsigned int shim_timer_trig_event_low_value = 0x000340F0;
// Trace Control0
const unsigned int shim_trace_control0 = 0x000340D0;
// Trace Control1
const unsigned int shim_trace_control1 = 0x000340D4;
// Trace Event0
const unsigned int shim_trace_event0 = 0x000340E0;
// Trace Event1
const unsigned int shim_trace_event1 = 0x000340E4;
// Trace Status
const unsigned int shim_trace_status = 0x000340D8;
// uC Core Interrupt Event
const unsigned int shim_uc_core_interrupt_event = 0x00034600;

// Register definitions for UC
// ###################################
// Base address of uC module
const unsigned int uc_base_address = 0x00000000;
// uC-Core control bits
const unsigned int uc_core_control = 0x000C0004;
// uC-Core external interrupts sticky bits
const unsigned int uc_core_interrupt_status = 0x000C0008;
// uC-Core status bits
const unsigned int uc_core_status = 0x000C0000;
// MM2DM DMA External AXI Control
const unsigned int uc_dma_dm2mm_axi_control = 0x000C0108;
// MM2DM DMA Control
const unsigned int uc_dma_dm2mm_control = 0x000C0104;
// MM2DM DMA Status
const unsigned int uc_dma_dm2mm_status = 0x000C0100;
// DM2MM DMA External AXI Control
const unsigned int uc_dma_mm2dm_axi_control = 0x000C0118;
// DM2MM DMA Control
const unsigned int uc_dma_mm2dm_control = 0x000C0114;
// DM2MM DMA Status
const unsigned int uc_dma_mm2dm_status = 0x000C0110;
// DMA Pause (privileged)
const unsigned int uc_dma_pause = 0x000C0120;
// Debug register access control and status
const unsigned int uc_mdm_dbg_ctrl_status = 0x000B0010;
// Debug register access data
const unsigned int uc_mdm_dbg_data = 0x000B0014;
// Debug register access lock
const unsigned int uc_mdm_dbg_lock = 0x000B0018;
// Microblaze Performance Counter Command
const unsigned int uc_mdm_pccmdr = 0x000B5480;
// Microblaze Performance Counter Control
const unsigned int uc_mdm_pcctrlr = 0x000B5440;
// Microblaze Performance Counter Data Read
const unsigned int uc_mdm_pcdrr = 0x000B5580;
// Microblaze Performance Counter Status
const unsigned int uc_mdm_pcsr = 0x000B54C0;
// Microblaze Performance Counter Data Write
const unsigned int uc_mdm_pcwr = 0x000B55C0;
// Inhibits ECC check bits update to uC-DM on writes (privileged)
const unsigned int uc_memory_dm_ecc_error_generation = 0x000C003C;
// uC-DM ECC scrubbing period (privileged)
const unsigned int uc_memory_dm_ecc_scrubbing_period = 0x000C0038;
// Control of uC-PM, uC-Private-DM and uC-Core debug port privileged status (privileged)
const unsigned int uc_memory_privileged = 0x000C0034;
// Control of memory zeroization (privileged)
const unsigned int uc_memory_zeroization = 0x000C0030;
// AXI-MM outstanding transactions monitors
const unsigned int uc_module_axi_mm_outstanding_transactions = 0x000C0024;
// AXI-MM address offset
const unsigned int uc_module_aximm_offset = 0x000C0020;

} // namespace aie2ps

#endif /* AIE2PS_REGISTERS_H_ */