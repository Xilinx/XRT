/*
 * Zynq Performance Monitoring Internal Parameters
 */

#ifndef _ZYNQ_PERFMON_PARAMS_H
#define _ZYNQ_PERFMON_PARAMS_H

// Address offset of SW trace register in trace monitor
#define ZYNQ_PERFMON_OFFSET 0xA000

/************************ AXI Stream FIFOs ************************************/

/* Address offsets in core */
#define AXI_FIFO_RDFR                   0x18
#define AXI_FIFO_RDFO                   0x1c
#define AXI_FIFO_RDFD                   0x20
#define AXI_FIFO_RDFD_AXI_FULL          0x1000
#define AXI_FIFO_TDFD                   0x10
#define AXI_FIFO_RLR                    0x24
#define AXI_FIFO_SRR                    0x28
#define AXI_FIFO_RESET_VALUE            0xA5

/************************ SDx Performance Monitor (SPM) ***********************/

/* Address offsets in core */
#define XSPM_CONTROL_OFFSET                          0x08
#define XSPM_TRACE_CTRL_OFFSET                       0x10
#define XSPM_EVENT_OFFSET                            0x18
#define XSPM_SAMPLE_OFFSET                           0x20
#define XSPM_FIFO_COUNTS_OFFSET                      0x28
#define XSPM_FIFO_READ_COUNTS_OFFSET                 0x30
#define XSPM_WRITE_BYTES_OFFSET                      0x40
#define XSPM_WRITE_TRANX_OFFSET                      0x44
#define XSPM_WRITE_LATENCY_OFFSET                    0x48
#define XSPM_READ_BYTES_OFFSET                       0x4C
#define XSPM_READ_TRANX_OFFSET                       0x50
#define XSPM_READ_LATENCY_OFFSET                     0x54
//#define XSPM_MIN_MAX_WRITE_LATENCY_OFFSET          0x58
//#define XSPM_MIN_MAX_READ_LATENCY_OFFSET           0x5C
#define XSPM_OUTSTANDING_COUNTS_OFFSET               0x58
#define XSPM_LAST_WRITE_ADDRESS_OFFSET               0x5C
#define XSPM_LAST_WRITE_DATA_OFFSET                  0x60
#define XSPM_LAST_READ_ADDRESS_OFFSET                0x64
#define XSPM_LAST_READ_DATA_OFFSET                   0x68
#define XSPM_SAMPLE_WRITE_BYTES_OFFSET               0x80
#define XSPM_SAMPLE_WRITE_TRANX_OFFSET               0x84
#define XSPM_SAMPLE_WRITE_LATENCY_OFFSET             0x88
#define XSPM_SAMPLE_READ_BYTES_OFFSET                0x8C
#define XSPM_SAMPLE_READ_TRANX_OFFSET                0x90
#define XSPM_SAMPLE_READ_LATENCY_OFFSET              0x94
// The following two registers are still in the hardware,
//  but are unused
//#define XSPM_SAMPLE_MIN_MAX_WRITE_LATENCY_OFFSET   0x98
//#define XSPM_SAMPLE_MIN_MAX_READ_LATENCY_OFFSET    0x9C
#define XSPM_SAMPLE_OUTSTANDING_COUNTS_OFFSET        0xA0
#define XSPM_SAMPLE_LAST_WRITE_ADDRESS_OFFSET        0xA4
#define XSPM_SAMPLE_LAST_WRITE_DATA_OFFSET           0xA8
#define XSPM_SAMPLE_LAST_READ_ADDRESS_OFFSET         0xAC
#define XSPM_SAMPLE_LAST_READ_DATA_OFFSET            0xB0
#define XSPM_SAMPLE_WRITE_BYTES_UPPER_OFFSET         0xC0
#define XSPM_SAMPLE_WRITE_TRANX_UPPER_OFFSET         0xC4
#define XSPM_SAMPLE_WRITE_LATENCY_UPPER_OFFSET       0xC8
#define XSPM_SAMPLE_READ_BYTES_UPPER_OFFSET          0xCC
#define XSPM_SAMPLE_READ_TRANX_UPPER_OFFSET          0xD0
#define XSPM_SAMPLE_READ_LATENCY_UPPER_OFFSET        0xD4
// Reserved for high 32-bits of MIN_MAX_WRITE_LATENCY - 0xD8
// Reserved for high 32-bits of MIN_MAX_READ_LATENCY  - 0xDC
#define XSPM_SAMPLE_OUTSTANDING_COUNTS_UPPER_OFFSET  0xE0
#define XSPM_SAMPLE_LAST_WRITE_ADDRESS_UPPER_OFFSET  0xE4
#define XSPM_SAMPLE_LAST_WRITE_DATA_UPPER_OFFSET     0xE8
#define XSPM_SAMPLE_LAST_READ_ADDRESS_UPPER_OFFSET   0xEC
#define XSPM_SAMPLE_LAST_READ_DATA_UPPER_OFFSET      0xF0

/* SPM Control Register masks */
#define XSPM_CR_RESET_ON_SAMPLE_MASK             0x00000010
#define XSPM_CR_FIFO_RESET_MASK                  0x00000008
#define XSPM_CR_COUNTER_RESET_MASK               0x00000002
#define XSPM_CR_COUNTER_ENABLE_MASK              0x00000001
#define XSPM_TRACE_CTRL_MASK                     0x00000003        

/* Debug IP layout properties mask bits */
#define XSPM_HOST_PROPERTY_MASK                  0x4
#define XSPM_64BIT_PROPERTY_MASK                 0x8

/************************ SDx Accelerator Monitor (SAM) ************************/

#define XSAM_CONTROL_OFFSET                          0x08
#define XSAM_TRACE_CTRL_OFFSET                       0x10
#define XSAM_SAMPLE_OFFSET                           0x20
#define XSAM_ACCEL_EXECUTION_COUNT_OFFSET            0x80
#define XSAM_ACCEL_EXECUTION_CYCLES_OFFSET           0x84  
#define XSAM_ACCEL_STALL_INT_OFFSET                  0x88
#define XSAM_ACCEL_STALL_STR_OFFSET                  0x8c
#define XSAM_ACCEL_STALL_EXT_OFFSET                  0x90
#define XSAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET       0x94
#define XSAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET       0x98
#define XSAM_ACCEL_TOTAL_CU_START_OFFSET             0x9c
#define XSAM_ACCEL_EXECUTION_COUNT_UPPER_OFFSET      0xA0
#define XSAM_ACCEL_EXECUTION_CYCLES_UPPER_OFFSET     0xA4
#define XSAM_ACCEL_STALL_INT_UPPER_OFFSET            0xA8
#define XSAM_ACCEL_STALL_STR_UPPER_OFFSET            0xAc
#define XSAM_ACCEL_STALL_EXT_UPPER_OFFSET            0xB0
#define XSAM_ACCEL_MIN_EXECUTION_CYCLES_UPPER_OFFSET 0xB4
#define XSAM_ACCEL_MAX_EXECUTION_CYCLES_UPPER_OFFSET 0xB8
#define XSAM_ACCEL_TOTAL_CU_START_UPPER_OFFSET       0xbc

/* SAM Trace Control Masks */
#define XSAM_TRACE_STALL_SELECT_MASK    0x0000001c

/* Debug IP layout properties mask bits */
#define XSAM_STALL_PROPERTY_MASK        0x4
#define XSAM_64BIT_PROPERTY_MASK        0x8

/************************** SDx AXI Stream Monitor (SSPM) *********************/

#define XSSPM_SAMPLE_OFFSET            0x20
#define XSSPM_NUM_TRANX_OFFSET         0x80
#define XSSPM_DATA_BYTES_OFFSET        0x88
#define XSSPM_BUSY_CYCLES_OFFSET       0x90
#define XSSPM_STALL_CYCLES_OFFSET      0x98
#define XSSPM_STARVE_CYCLES_OFFSET     0xA0

//Following status registers are available at each base
#define LAPC_OVERALL_STATUS_OFFSET        0x0
#define LAPC_CUMULATIVE_STATUS_0_OFFSET   0x100
#define LAPC_CUMULATIVE_STATUS_1_OFFSET   0x104
#define LAPC_CUMULATIVE_STATUS_2_OFFSET   0x108
#define LAPC_CUMULATIVE_STATUS_3_OFFSET   0x10c

#define LAPC_SNAPSHOT_STATUS_0_OFFSET     0x200
#define LAPC_SNAPSHOT_STATUS_1_OFFSET     0x204
#define LAPC_SNAPSHOT_STATUS_2_OFFSET     0x208
#define LAPC_SNAPSHOT_STATUS_3_OFFSET     0x20c

// Trace event types
// NOTE: defined as xclPerfMonEventType in xclperf.h
//#define ZYNQ_START_EVENT 0x4
//#define ZYNQ_START_EVENT 0x5

// Trace event IDs
// NOTE: defined as xclPerfMonEventID in xclperf.h
//#define ZYNQ_API_GENERAL_ID 2000
//#define ZYNQ_API_QUEUE_ID   2001
//#define ZYNQ_HOST_READ_ID   2002
//#define ZYNQ_HOST_WRITE_ID  2003

#endif
