#ifndef __KERNEL_MANAGEMENT_API__
#define __KERNEL_MANAGEMENT_API__

#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#include <vector>

#include "xclhal2.h"
#include "hal_global_variable.h"
#include "device_meta.h"
#include "input_validation.h"
#include "error_report.h"
#include "type_conversion.h"
#include "kernel_management_config.h"

namespace py = boost::python;
namespace np = boost::python::numpy;
using namespace std;

void configure_ert(string device_name, unsigned buffer_handle, py::dict config);

void start_kernel(string device_name, unsigned buffer_handle, py::dict command);

void execute_buffer(string device_name, unsigned buffer_handle);

void execute_wait(string device_name, int timeout);

#endif