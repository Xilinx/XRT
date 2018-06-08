// Copyright Xilinx, Inc 2014-2016
// Author: Sonal Santan
// Register definition for the APPMGMT

#ifndef __AWSMGMT_CW_H__
#define __AWSMGMT_CW_H__

#define OCL_CTLR_OFFSET         0x000000
#define AXI_GATE_OFFSET         0x030000
#define AXI_GATE_OFFSET_READ    0x030008

#define FEATURE_ID              0x031000

#define GENERAL_STATUS          0x032000

#define OCL_CLKWIZ_OFFSET       0x050000
#define OCL_CLKWIZ_BASEADDR     0x050000
#define OCL_CLKWIZ_BASEADDR2    0x051000
#define OCL_CLKWIZ_SYSCLOCK     0x052000

#define OCL_CLKWIZ_STATUS_OFFSET      0x4
#define OCL_CLKWIZ_CONFIG_OFFSET(n)   (0x200 + 4 * (n))

// These are kept only for backwards compatipility. These macros should
// not be used anymore.
#define OCL_CLKWIZ_STATUS     (OCL_CLKWIZ_BASEADDR + OCL_CLKWIZ_STATUS_OFFSET)
#define OCL_CLKWIZ_CONFIG(n)  (OCL_CLKWIZ_BASEADDR + OCL_CLKWIZ_CONFIG_OFFSET(n))


/************************** Constant Definitions ****************************/

/* Input frequency */
#define AMAZON_INPUT_FREQ 250


#define OCL_CU_CTRL_RANGE      0x1000

#define APPMGMT_7V3_CLKWIZ_CONFIG0 0x04000a01
#define APPMGMT_KU3_CLKWIZ_CONFIG0 0x04000a01

#endif


