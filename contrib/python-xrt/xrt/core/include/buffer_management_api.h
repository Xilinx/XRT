#ifndef __BUFFER_MANAGEMENT_API__
#define __BUFFER_MANAGEMENT_API__

#include <boost/python.hpp>
#include <boost/python/numpy.hpp>

#include "xclhal2.h"
#include "hal_global_variable.h"
#include "device_meta.h"
#include "input_validation.h"
#include "error_report.h"
#include "type_conversion.h"

namespace py = boost::python;
namespace np = boost::python::numpy;
using namespace std;

unsigned allocate_buffer(string device_name, string type, unsigned flags, unsigned size);

void free_buffer(string device_name, unsigned buffer_handle);

void write_buffer(string device_name, unsigned buffer_handle, np::ndarray data);

np::ndarray read_buffer(string device_name, unsigned buffer_handle, unsigned size, unsigned skip, string type);

void map_buffer(string device_name, unsigned buffer_handle, bool write);

py::dict buffer_property(string device_name, unsigned buffer_handle);

void sync_buffer(string device_name, unsigned buffer_handle, string type, unsigned size, unsigned offset);

#endif