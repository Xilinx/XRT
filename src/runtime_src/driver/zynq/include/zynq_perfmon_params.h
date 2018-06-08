/*
 * Zynq Performance Monitoring Internal Parameters
 */

#ifndef _ZYNQ_PERFMON_PARAMS_H
#define _ZYNQ_PERFMON_PARAMS_H

// Address offset of SW trace register in trace monitor
#define ZYNQ_PERFMON_OFFSET 0xA000

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
