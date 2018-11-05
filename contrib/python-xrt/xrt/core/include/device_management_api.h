#ifndef __DEVICE_MANAGEMENT_API__
#define __DEVICE_MANAGEMENT_API__

#include <string>
#include <fstream>

#include "xclhal2.h"
#include "hal_global_variable.h"
#include "device_meta.h"
#include "input_validation.h"
#include "error_report.h"
#include "type_conversion.h"

using namespace std;

/// Probe Device API
/** This is the python wrapper for the xclProbe API in the Xilinx XRT */
int probe_devices();

void open_device(int device_index, string device_name, string verbosity_level);

void close_device(string device_name);

void reset_device(string device_name, string reset_type);

py::dict get_device_info(string device_name);

py::dict get_device_usage(string device_name);

py::dict get_device_error(string device_name);

void reclock_device(string device_name, int target_region, int target_freq);

void lock_device(string device_name);

void unlock_device(string device_name);

py::dict load_bitstream(string device_name, string filename);

py::dict get_bitstream_info(string device_name, string type, int index);

void open_context(string device_name, string xclbin_name, int ip_index, bool shared);

void close_context(string device_name, string xclbin_name, int ip_index);

void boot_device(string device_name);

void remove_scan_fpga();

int get_version();

#endif