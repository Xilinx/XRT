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

/************************ AXI Interface Monitor (AIM, earlier SPM) ***********************/

/* Address offsets in core */
#define XAIM_CONTROL_OFFSET                          0x08
#define XAIM_TRACE_CTRL_OFFSET                       0x10
#define XAIM_EVENT_OFFSET                            0x18
#define XAIM_SAMPLE_OFFSET                           0x20
#define XAIM_FIFO_COUNTS_OFFSET                      0x28
#define XAIM_FIFO_READ_COUNTS_OFFSET                 0x30
#define XAIM_WRITE_BYTES_OFFSET                      0x40
#define XAIM_WRITE_TRANX_OFFSET                      0x44
#define XAIM_WRITE_LATENCY_OFFSET                    0x48
#define XAIM_READ_BYTES_OFFSET                       0x4C
#define XAIM_READ_TRANX_OFFSET                       0x50
#define XAIM_READ_LATENCY_OFFSET                     0x54
//#define XAIM_MIN_MAX_WRITE_LATENCY_OFFSET          0x58
//#define XAIM_MIN_MAX_READ_LATENCY_OFFSET           0x5C
#define XAIM_OUTSTANDING_COUNTS_OFFSET               0x58
#define XAIM_LAST_WRITE_ADDRESS_OFFSET               0x5C
#define XAIM_LAST_WRITE_DATA_OFFSET                  0x60
#define XAIM_LAST_READ_ADDRESS_OFFSET                0x64
#define XAIM_LAST_READ_DATA_OFFSET                   0x68
#define XAIM_SAMPLE_WRITE_BYTES_OFFSET               0x80
#define XAIM_SAMPLE_WRITE_TRANX_OFFSET               0x84
#define XAIM_SAMPLE_WRITE_LATENCY_OFFSET             0x88
#define XAIM_SAMPLE_READ_BYTES_OFFSET                0x8C
#define XAIM_SAMPLE_READ_TRANX_OFFSET                0x90
#define XAIM_SAMPLE_READ_LATENCY_OFFSET              0x94
// The following two registers are still in the hardware,
//  but are unused
//#define XAIM_SAMPLE_MIN_MAX_WRITE_LATENCY_OFFSET   0x98
//#define XAIM_SAMPLE_MIN_MAX_READ_LATENCY_OFFSET    0x9C
#define XAIM_SAMPLE_OUTSTANDING_COUNTS_OFFSET        0xA0
#define XAIM_SAMPLE_LAST_WRITE_ADDRESS_OFFSET        0xA4
#define XAIM_SAMPLE_LAST_WRITE_DATA_OFFSET           0xA8
#define XAIM_SAMPLE_LAST_READ_ADDRESS_OFFSET         0xAC
#define XAIM_SAMPLE_LAST_READ_DATA_OFFSET            0xB0
#define XAIM_SAMPLE_WRITE_BYTES_UPPER_OFFSET         0xC0
#define XAIM_SAMPLE_WRITE_TRANX_UPPER_OFFSET         0xC4
#define XAIM_SAMPLE_WRITE_LATENCY_UPPER_OFFSET       0xC8
#define XAIM_SAMPLE_READ_BYTES_UPPER_OFFSET          0xCC
#define XAIM_SAMPLE_READ_TRANX_UPPER_OFFSET          0xD0
#define XAIM_SAMPLE_READ_LATENCY_UPPER_OFFSET        0xD4
// Reserved for high 32-bits of MIN_MAX_WRITE_LATENCY - 0xD8
// Reserved for high 32-bits of MIN_MAX_READ_LATENCY  - 0xDC
#define XAIM_SAMPLE_OUTSTANDING_COUNTS_UPPER_OFFSET  0xE0
#define XAIM_SAMPLE_LAST_WRITE_ADDRESS_UPPER_OFFSET  0xE4
#define XAIM_SAMPLE_LAST_WRITE_DATA_UPPER_OFFSET     0xE8
#define XAIM_SAMPLE_LAST_READ_ADDRESS_UPPER_OFFSET   0xEC
#define XAIM_SAMPLE_LAST_READ_DATA_UPPER_OFFSET      0xF0

/* SPM Control Register masks */
#define XAIM_CR_RESET_ON_SAMPLE_MASK             0x00000010
#define XAIM_CR_FIFO_RESET_MASK                  0x00000008
#define XAIM_CR_COUNTER_RESET_MASK               0x00000002
#define XAIM_CR_COUNTER_ENABLE_MASK              0x00000001
#define XAIM_TRACE_CTRL_MASK                     0x00000003        

/* Debug IP layout properties mask bits */
#define XAIM_HOST_PROPERTY_MASK                  0x4
#define XAIM_64BIT_PROPERTY_MASK                 0x8

/************************ Accelerator Monitor (AM, earlier SAM) ************************/

#define XAM_CONTROL_OFFSET                          0x08
#define XAM_TRACE_CTRL_OFFSET                       0x10
#define XAM_SAMPLE_OFFSET                           0x20
#define XAM_ACCEL_EXECUTION_COUNT_OFFSET            0x80
#define XAM_ACCEL_EXECUTION_CYCLES_OFFSET           0x84  
#define XAM_ACCEL_STALL_INT_OFFSET                  0x88
#define XAM_ACCEL_STALL_STR_OFFSET                  0x8c
#define XAM_ACCEL_STALL_EXT_OFFSET                  0x90
#define XAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET       0x94
#define XAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET       0x98
#define XAM_ACCEL_TOTAL_CU_START_OFFSET             0x9c
#define XAM_ACCEL_EXECUTION_COUNT_UPPER_OFFSET      0xA0
#define XAM_ACCEL_EXECUTION_CYCLES_UPPER_OFFSET     0xA4
#define XAM_ACCEL_STALL_INT_UPPER_OFFSET            0xA8
#define XAM_ACCEL_STALL_STR_UPPER_OFFSET            0xAc
#define XAM_ACCEL_STALL_EXT_UPPER_OFFSET            0xB0
#define XAM_ACCEL_MIN_EXECUTION_CYCLES_UPPER_OFFSET 0xB4
#define XAM_ACCEL_MAX_EXECUTION_CYCLES_UPPER_OFFSET 0xB8
#define XAM_ACCEL_TOTAL_CU_START_UPPER_OFFSET       0xbc
#define XAM_BUSY_CYCLES_OFFSET                      0xC0
#define XAM_BUSY_CYCLES_UPPER_OFFSET                0xC4
#define XAM_MAX_PARALLEL_ITER_OFFSET                0xC8
#define XAM_MAX_PARALLEL_ITER_UPPER_OFFSET          0xCC

/* SAM Trace Control Masks */
#define XAM_TRACE_STALL_SELECT_MASK    0x0000001c
#define XAM_COUNTER_RESET_MASK         0x00000002
#define XAM_DATAFLOW_EN_MASK           0x00000008

/* Debug IP layout properties mask bits */
#define XAM_STALL_PROPERTY_MASK        0x4
#define XAM_64BIT_PROPERTY_MASK        0x8

/************************** AXI Stream Monitor (ASM, earlier SSPM) *********************/

#define XASM_SAMPLE_OFFSET            0x20
#define XASM_NUM_TRANX_OFFSET         0x80
#define XASM_DATA_BYTES_OFFSET        0x88
#define XASM_BUSY_CYCLES_OFFSET       0x90
#define XASM_STALL_CYCLES_OFFSET      0x98
#define XASM_STARVE_CYCLES_OFFSET     0xA0

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
