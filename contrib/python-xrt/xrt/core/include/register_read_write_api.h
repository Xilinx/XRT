#ifndef __REGISTER_READ_WRITE_API__
#define __REGISTER_READ_WRITE_API__

#include <boost/python.hpp>
#include <vector>

#include "xclhal2.h"
#include "hal_global_variable.h"
#include "device_meta.h"
#include "input_validation.h"
#include "error_report.h"
#include "type_conversion.h"
#include "kernel_management_config.h"

namespace py = boost::python;
using namespace std;

py::list read_register(string device_name, string domain, unsigned long offset, unsigned size);

void write_register(string device_name, string domain, unsigned long offset, unsigned size, py::list data);

#endif