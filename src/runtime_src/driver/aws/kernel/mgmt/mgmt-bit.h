#ifndef __AWSMGT_BIT_H__
#define __AWSMGT_BIT_H__

/*
 * Booting FPGA from PROM
 * http://www.xilinx.com/support/documentation/user_guides/ug470_7Series_Config.pdf
 * Table 7.1
 */

#define DUMMY_WORD         0xFFFFFFFF
#define SYNC_WORD          0xAA995566
#define TYPE1_NOOP         0x20000000
#define TYPE1_WRITE_WBSTAR 0x30020001
#define WBSTAR_ADD10       0x00000000
#define WBSTAR_ADD11       0x01000000
#define TYPE1_WRITE_CMD    0x30008001
#define IPROG_CMD          0x0000000F

/**
 * Copyright Xilinx, Inc 2017
 * Author: Sonal Santan
 * ICAP Register definition for the AWS-MGT
 */

/* ICAP register definition */
#define XHWICAP_GIER            HWICAP_OFFSET+0x1c
#define XHWICAP_ISR             HWICAP_OFFSET+0x20
#define XHWICAP_IER             HWICAP_OFFSET+0x28
#define XHWICAP_WF              HWICAP_OFFSET+0x100
#define XHWICAP_RF              HWICAP_OFFSET+0x104
#define XHWICAP_SZ              HWICAP_OFFSET+0x108
#define XHWICAP_CR              HWICAP_OFFSET+0x10c
#define XHWICAP_SR              HWICAP_OFFSET+0x110
#define XHWICAP_WFV             HWICAP_OFFSET+0x114
#define XHWICAP_RFO             HWICAP_OFFSET+0x118
#define XHWICAP_ASR             HWICAP_OFFSET+0x11c

/**
* Bitstream header information.
*/
typedef struct {
  unsigned int HeaderLength;     /* Length of header in 32 bit words */
  unsigned int BitstreamLength;  /* Length of bitstream to read in bytes*/
  unsigned char *DesignName;     /* Design name read from bitstream header */
  unsigned char *PartName;       /* Part name read from bitstream header */
  unsigned char *Date;           /* Date read from bitstream header */
  unsigned char *Time;           /* Bitstream creation time read from header */
  unsigned int MagicLength;      /* Length of the magic numbers in header */
} XHwIcap_Bit_Header;

/* Used for parsing bitstream header */
#define XHI_EVEN_MAGIC_BYTE     0x0f
#define XHI_ODD_MAGIC_BYTE      0xf0

/* Extra mode for IDLE */
#define XHI_OP_IDLE  -1

#define XHI_BIT_HEADER_FAILURE -1

/* The imaginary module length register */
#define XHI_MLR                  15

#define DMA_HWICAP_BITFILE_BUFFER_SIZE 1024

#endif


