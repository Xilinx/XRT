#include <unordered_map>
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#include <dictobject.h>
#include <stdlib.h>
#include <iostream>

#include "xclhal2.h"
#include "xclbin.h"
#include "device_meta.h"
#include "device_management_api.h"
#include "buffer_management_api.h"
#include "kernel_management_api.h"
#include "register_read_write_api.h"
#include "type_conversion.h"
#include "input_validation.h"
#include "error_report.h"

namespace py = boost::python;
namespace np = boost::python::numpy;
using namespace std;

// global variables for the purpose of getting around data type
// changes from c++ to python and the other way around
unordered_map<string, DeviceMeta *> device_dict;
int known_device_cnt = 0;

BOOST_PYTHON_MODULE(
#if defined(SW_EMU)
	sw_emu_hal
#elif defined(HW_EMU)
	hw_emu_hal
#else
	hal
#endif
)
{
	Py_Initialize();
	np::initialize();
	py::def("probe", probe_devices);
	py::def("open", open_device);
	py::def("close", close_device);
	py::def("reset", reset_device);
	py::def("info", get_device_info);
	py::def("reclock", reclock_device);
	py::def("lock", lock_device);
	py::def("unlock", unlock_device);
	py::def("version", get_version);
	py::def("load", load_bitstream);
	py::def("bitstream", get_bitstream_info);
	py::def("allocate_buffer", allocate_buffer);
	py::def("write_buffer", write_buffer);
	py::def("read_buffer", read_buffer);
	py::def("free_buffer", free_buffer);
	py::def("map_buffer", map_buffer);
	py::def("buffer_property", buffer_property);
	py::def("sync_buffer", sync_buffer);
#if !defined(SW_EMU) && !defined(HW_EMU)
	py::def("usage", get_device_usage);
	py::def("error", get_device_error);
	py::def("default", remove_scan_fpga);
#endif

	/* Kernel Management API */
	py::def("configure_ert", configure_ert);
	py::def("start_kernel", start_kernel);
#if !defined(SW_EMU)
	py::def("execute_buffer", execute_buffer);
	py::def("execute_wait", execute_wait);
#endif

	/* Register Read/Write API */
	py::def("read_register", read_register);
}
