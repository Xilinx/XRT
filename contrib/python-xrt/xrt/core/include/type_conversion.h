#ifndef __TYPE_CONVERSION__
#define __TYPE_CONVERSION__

#include <iostream>
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>

#include "xclhal2.h"
#include "ert.h"

using namespace std;
namespace py = boost::python;
namespace np = boost::python::numpy;

string generate_log_filename(string &device_name);

xclVerbosityLevel convert_verbosity_level(string &level);

xclResetKind convert_reset_kind(string &type);

py::dict convert_device_info(xclDeviceInfo2 *info);

py::dict convert_usage_info(xclDeviceUsage *info);

py::dict convert_error_info(xclErrorStatus *info);

std::string convert_ip_type(IP_TYPE type);

std::string convert_debug_ip_layout(DEBUG_IP_TYPE type);

xclBOKind convert_buffer_type(string type);

np::dtype convert_buffer_data_type(string type);

ert_cmd_state convert_ert_command_state(py::object state_object);

ert_cmd_opcode convert_ert_command_opcode(py::object opcode_object);

xclBOSyncDirection convert_sync_buffer_type(string type);

xclAddressSpace convert_register_domain(string domain);

bool is_64bit_arch(vector<unsigned long>& kernel_arguments);

#endif