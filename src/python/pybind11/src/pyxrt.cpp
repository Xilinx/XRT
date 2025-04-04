/* SPDX-License-Identifier: Apache-2.0 */

/**
 * Pybind11 module for XRT C++ APIs
 *
 * Copyright (C) 2019-2022 Xilinx, Inc
 *
 * Authors: graham.schelle@xilinx.com
 *          sonal.santan@xilinx.com
 */

// XRT includes
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"
#include "xrt/experimental/xrt_message.h"
#include "xrt/experimental/xrt_system.h"
#include "xrt/experimental/xrt_xclbin.h"
#include "xrt/experimental/xrt_elf.h"
#include "xrt/experimental/xrt_aie.h"

// Pybind11 includes
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/stl_bind.h>

// C++11 includes
#include <string>

namespace py = pybind11;

PYBIND11_MAKE_OPAQUE(std::vector<xrt::xclbin::ip>);

PYBIND11_MODULE(pyxrt, m) {
    m.doc() = "Pybind11 module for XRT";

/*
 *
 * Constants and Enums
 *
 *
 */
    m.attr("XCL_BO_FLAGS_NONE") = py::int_(XCL_BO_FLAGS_NONE);

    py::enum_<xclBOSyncDirection>(m, "xclBOSyncDirection", "DMA flags used with DMA API")
    .value("XCL_BO_SYNC_BO_TO_DEVICE", xclBOSyncDirection::XCL_BO_SYNC_BO_TO_DEVICE)
        .value("XCL_BO_SYNC_BO_FROM_DEVICE", xclBOSyncDirection::XCL_BO_SYNC_BO_FROM_DEVICE)
        .value("XCL_BO_SYNC_BO_GMIO_TO_AIE", xclBOSyncDirection::XCL_BO_SYNC_BO_GMIO_TO_AIE)
        .value("XCL_BO_SYNC_BO_AIE_TO_GMIO", xclBOSyncDirection::XCL_BO_SYNC_BO_AIE_TO_GMIO);

    py::enum_<ert_cmd_state>(m, "ert_cmd_state", "Kernel execution status")
        .value("ERT_CMD_STATE_NEW", ert_cmd_state::ERT_CMD_STATE_NEW)
        .value("ERT_CMD_STATE_QUEUED", ert_cmd_state::ERT_CMD_STATE_QUEUED)
        .value("ERT_CMD_STATE_COMPLETED", ert_cmd_state::ERT_CMD_STATE_COMPLETED)
        .value("ERT_CMD_STATE_ERROR", ert_cmd_state::ERT_CMD_STATE_ERROR)
        .value("ERT_CMD_STATE_ABORT", ert_cmd_state::ERT_CMD_STATE_ABORT)
        .value("ERT_CMD_STATE_SUBMITTED", ert_cmd_state::ERT_CMD_STATE_SUBMITTED)
        .value("ERT_CMD_STATE_TIMEOUT", ert_cmd_state::ERT_CMD_STATE_TIMEOUT)
        .value("ERT_CMD_STATE_NORESPONSE", ert_cmd_state::ERT_CMD_STATE_NORESPONSE)
        .value("ERT_CMD_STATE_SKERROR", ert_cmd_state::ERT_CMD_STATE_SKERROR)
        .value("ERT_CMD_STATE_SKCRASHED", ert_cmd_state::ERT_CMD_STATE_SKCRASHED);

    py::enum_<xrt::info::device>(m, "xrt_info_device", "Device feature and sensor information")
        .value("bdf", xrt::info::device::bdf)
        .value("interface_uuid", xrt::info::device::interface_uuid)
        .value("kdma", xrt::info::device::kdma)
        .value("max_clock_frequency_mhz", xrt::info::device::max_clock_frequency_mhz)
        .value("m2m", xrt::info::device::m2m)
        .value("name", xrt::info::device::name)
        .value("nodma", xrt::info::device::nodma)
        .value("offline", xrt::info::device::offline)
        .value("electrical", xrt::info::device::electrical)
        .value("thermal", xrt::info::device::thermal)
        .value("mechanical", xrt::info::device::mechanical)
        .value("memory", xrt::info::device::memory)
        .value("platform", xrt::info::device::platform)
        .value("pcie_info", xrt::info::device::pcie_info)
        .value("host", xrt::info::device::host)
        .value("dynamic_regions", xrt::info::device::dynamic_regions)
        .value("vmr", xrt::info::device::vmr);

    py::enum_<xrt::message::level>(m, "xrt_msg_level", "XRT log msgs level")
        .value("emergency", xrt::message::level::emergency)
        .value("alert", xrt::message::level::alert)
        .value("critical", xrt::message::level::critical)
        .value("error", xrt::message::level::error)
        .value("warning", xrt::message::level::warning)
        .value("notice", xrt::message::level::notice)
        .value("info", xrt::message::level::info)
        .value("debug", xrt::message::level::debug)
        .export_values();

/*
 * Global Functions
 */
    m.def("enumerate_devices", &xrt::system::enumerate_devices, "Enumerate devices in system");
    m.def("log_message", &xrt::message::log, "Dispatch formatted log message");

 /*
 *
 * XRT:: UUID (needed since UUID classes passed outside of objects)
 *
 */
    py::class_<xrt::uuid>(m, "uuid", "XRT UUID object to identify a compiled xclbin binary")
        .def(py::init<char *>())
        .def("to_string", &xrt::uuid::to_string, "Convert XRT UUID object to string");

/*
*  xrt::hw_context
*/

    py::class_<xrt::hw_context>(m, "hw_context", "A hardware context associates an xclbin with hardware resources.")
        .def(py::init<>())
        .def(py::init([](const xrt::device& d, const xrt::uuid& u) {
            return new xrt::hw_context(d, u);
        }));

/*
 *
 * xrt::device
 *
 */
    py::class_<xrt::device>(m, "device", "Abstraction of an acceleration device")
        .def(py::init<>())
        .def(py::init<unsigned int>())
        .def(py::init([] (const std::string& bfd) {
                          return new xrt::device(bfd);
                      }))
        .def("load_xclbin", [](xrt::device& d, const std::string& xclbin) {
                                return d.load_xclbin(xclbin);
                            }, "Load an xclbin given the path to the device")
        .def("load_xclbin", [](xrt::device& d, const xrt::xclbin& xclbin) {
                                return d.load_xclbin(xclbin);
                            }, "Load the xclbin to the device")
        .def("register_xclbin", [](xrt::device& d, const xrt::xclbin& xclbin) {
                                return d.register_xclbin(xclbin);
                            }, "Register an xclbin with the device")
        .def("get_xclbin_uuid", &xrt::device::get_xclbin_uuid, "Return the UUID object representing the xclbin loaded on the device")
        .def("get_info", [] (xrt::device& d, xrt::info::device key) {
                             /* Convert the value to string since we can have only one return type for get_info() */
                             switch (key) {
                             case xrt::info::device::bdf:
                                 return d.get_info<xrt::info::device::bdf>();
                             case xrt::info::device::interface_uuid:
                                 return d.get_info<xrt::info::device::interface_uuid>().to_string();
                             case xrt::info::device::kdma:
                                 return std::to_string(d.get_info<xrt::info::device::kdma>());
                             case xrt::info::device::max_clock_frequency_mhz:
                                 return std::to_string(d.get_info<xrt::info::device::max_clock_frequency_mhz>());
                             case xrt::info::device::m2m:
                                 return std::to_string(d.get_info<xrt::info::device::m2m>());
                             case xrt::info::device::name:
                                 return d.get_info<xrt::info::device::name>();
                             case xrt::info::device::nodma:
                                 return std::to_string(d.get_info<xrt::info::device::nodma>());
                             case xrt::info::device::offline:
                                 return std::to_string(d.get_info<xrt::info::device::offline>());
                             case xrt::info::device::electrical:
                                 return d.get_info<xrt::info::device::electrical>();
                             case xrt::info::device::thermal:
                                 return d.get_info<xrt::info::device::thermal>();
                             case xrt::info::device::mechanical:
                                 return d.get_info<xrt::info::device::mechanical>();
                             case xrt::info::device::memory:
                                 return d.get_info<xrt::info::device::memory>();
                             case xrt::info::device::platform:
                                 return d.get_info<xrt::info::device::platform>();
                             case xrt::info::device::pcie_info:
                                 return d.get_info<xrt::info::device::pcie_info>();
                             case xrt::info::device::host:
                                 return d.get_info<xrt::info::device::host>();
                             case xrt::info::device::dynamic_regions:
                                 return d.get_info<xrt::info::device::dynamic_regions>();
                             case xrt::info::device::vmr:
                                 return d.get_info<xrt::info::device::vmr>();
                             default:
                                 return std::string("NA");
                             }
                         }, "Obtain the device properties and sensor information");


/*
 *
 * xrt::run
 *
 */
    py::class_<xrt::run>(m, "run", "Represents one execution of a kernel")
        .def(py::init<>())
        .def(py::init<const xrt::kernel &>())
        .def("start", [](xrt::run& r){
                          r.start();
                      }, "Start one execution of a run")
        .def("set_arg", [](xrt::run& r, int i, xrt::bo& item){
                            r.set_arg(i, item);
                        }, "Set a specific kernel global argument for a run")
        .def("set_arg", [](xrt::run& r, int i, int& item){
                            r.set_arg<int&>(i, item);
                        }, "Set a specific kernel scalar argument for this run")
        .def("wait", ([](xrt::run& r)  {
                           return r.wait(0);
                      }), "Wait for the run to complete")
        .def("wait", ([](xrt::run& r, unsigned int timeout_ms)  {
                          return r.wait(timeout_ms);
                      }), "Wait for the specified milliseconds for the run to complete")
        .def("state", &xrt::run::state, "Check the current state of a run object")
        .def("add_callback", &xrt::run::add_callback, "Add a callback function for run state");

    py::class_<xrt::kernel> pyker(m, "kernel", "Represents a set of instances matching a specified name");

    py::enum_<xrt::kernel::cu_access_mode>(pyker, "cu_access_mode", "Compute unit access mode")
        .value("exclusive", xrt::kernel::cu_access_mode::exclusive)
        .value("shared", xrt::kernel::cu_access_mode::shared)
        .value("none", xrt::kernel::cu_access_mode::none)
        .export_values();


    pyker.def(py::init([](const xrt::device& d, const xrt::uuid& u, const std::string& n,
                          xrt::kernel::cu_access_mode m) {
                           return new xrt::kernel(d, u, n, m);
                       }))
  	    .def(py::init([](const xrt::device& d, const xrt::uuid& u, const std::string& n) {
                               return new xrt::kernel(d, u, n);
                       }))
        .def(py::init([](const xrt::hw_context& ctx, const std::string& n) {
                               return new xrt::kernel(ctx, n);
                       }))
        .def("__call__", [](xrt::kernel& k, py::args args) -> xrt::run {
                             int i = 0;
                             xrt::run r(k);

                             for (auto item : args) {
                                 try {
                                     r.set_arg(i, item.cast<xrt::bo>());
                                 }
                                 catch (std::exception e) {  }

                                 try {
                                     r.set_arg<int>(i, item.cast<int>());
                                 }
                                 catch (std::exception e) {  }

                                 i++;
                             }

                             r.start();
                             return r;
                         })
        .def("group_id", &xrt::kernel::group_id, "Get the memory bank group id of an kernel argument");


/*
 *
 * xrt::bo
 *
 */
    py::class_<xrt::bo> pybo(m, "bo", "Represents a buffer object");

    py::enum_<xrt::bo::flags>(pybo, "flags", "Buffer object creation flags")
        .value("normal", xrt::bo::flags::normal)
        .value("cacheable", xrt::bo::flags::cacheable)
        .value("device_only", xrt::bo::flags::device_only)
        .value("host_only", xrt::bo::flags::host_only)
        .value("p2p", xrt::bo::flags::p2p)
        .value("svm", xrt::bo::flags::svm)
        .export_values();

    pybo.def(py::init<xrt::device, size_t, xrt::bo::flags, xrt::memory_group>(), "Create a buffer object with specified properties")
        .def(py::init<xrt::bo, size_t, size_t>(), "Create a sub-buffer of an existing buffer object of specifed size and offset in the existing buffer")
        .def("write", ([](xrt::bo &b, py::buffer pyb, size_t seek)  {
                           py::buffer_info info = pyb.request();
                           b.write(info.ptr, info.itemsize * info.size , seek);
                       }), "Write the provided data into the buffer object starting at specified offset")
        .def("read", ([](xrt::bo &b, size_t size, size_t skip) {
                          py::array_t<char> result = py::array_t<char>(size);
                          py::buffer_info bufinfo = result.request();
                          b.read(bufinfo.ptr, size, skip);
                          return result;
                      }), "Read from the buffer object requested number of bytes starting from specified offset")
        .def("sync", ([](xrt::bo &b, xclBOSyncDirection dir, size_t size, size_t offset)  {
                          b.sync(dir, size, offset);
                      }), "Synchronize (DMA or cache flush/invalidation) the buffer in the requested direction")
        .def("sync", ([](xrt::bo& b, xclBOSyncDirection dir) {
                          b.sync(dir);
                      }), "Sync entire buffer content in specified direction.")
        .def("map", ([](xrt::bo &b)  {
                         return py::memoryview::from_memory(b.map(), b.size());
                     }), "Create a byte accessible memory view of the buffer object")
        .def("size", &xrt::bo::size, "Return the size of the buffer object")
        .def("address", &xrt::bo::address, "Return the device physical address of the buffer object");

/*
 *
 * xrt::xclbin::ip, xrt::xclbin::kernel
 *
 */
    py::class_<xrt::xclbin> pyxclbin(m, "xclbin", "Represents an xclbin and provides APIs to access meta data");
    py::class_<xrt::xclbin::ip> pyxclbinip(pyxclbin, "xclbinip");
    py::bind_vector<std::vector<xrt::xclbin::ip>>(m, "xclbinip_vector");
    py::class_<xrt::xclbin::kernel> pyxclbinkernel(pyxclbin, "xclbinkernel", "Represents a kernel in an xclbin");
    py::bind_vector<std::vector<xrt::xclbin::kernel>>(m, "xclbinkernel_vector");
    py::class_<xrt::xclbin::mem> pyxclbinmem(pyxclbin, "xclbinmem", "Represents a physical device memory bank");
    py::bind_vector<std::vector<xrt::xclbin::mem>>(m, "xclbinmem_vector");


    pyxclbinip.def(py::init<>())
        .def("get_name", &xrt::xclbin::ip::get_name);

    pyxclbinkernel.def(py::init<>())
        .def("get_name", &xrt::xclbin::kernel::get_name, "Get kernel name")
        .def("get_num_args", &xrt::xclbin::kernel::get_num_args, "Number of arguments");

    pyxclbinmem.def(py::init<>())
        .def("get_tag", &xrt::xclbin::mem::get_tag, "Get tag name")
        .def("get_base_address", &xrt::xclbin::mem::get_base_address, "Get the base address of the memory bank")
        .def("get_size_kb", &xrt::xclbin::mem::get_size_kb, "Get the size of the memory in KB")
        .def("get_used", &xrt::xclbin::mem::get_used, "Get used status of the memory")
        .def("get_index", &xrt::xclbin::mem::get_index, "Get the index of the memory");
/*
 *
 * xrt::xclbin
 *
 */

    pyxclbin.def(py::init<>())
        .def(py::init([](const std::string &filename) {
                          return new xrt::xclbin(filename);
                      }))
        .def(py::init([](const axlf* top) {
                          return new xrt::xclbin(top);
                      }))
        .def("get_kernels", &xrt::xclbin::get_kernels, "Get list of kernels from xclbin")
        .def("get_xsa_name", &xrt::xclbin::get_xsa_name, "Get Xilinx Support Archive (XSA) name of xclbin")
        .def("get_uuid", &xrt::xclbin::get_uuid, "Get the uuid of the xclbin")
        .def("get_mems", &xrt::xclbin::get_mems, "Get list of memory objects")
        .def("get_axlf", &xrt::xclbin::get_axlf, "Get the axlf data of the xclbin");

/*
 *
 *  xrt::elf, xrt::aie::program
 *
 */

    py::class_<xrt::elf> pyelf(m, "elf", "ELF representation of compiled AIE binary");

    py::class_<xrt::aie::program> pyprogram(m, "program",
                                            "Represents a compiled program to be executed on the AIE. "
                                            "The program is an ELF file with sections and data specific to the AIE.");

    pyelf
        .def(py::init([](const std::string& fnm) {
            return new xrt::elf(fnm); }))
        .def(py::init([](const void *data, size_t size) {
            return new xrt::elf(data, size); }));

    pyprogram
        .def(py::init([](xrt::elf& xe) {
            return new xrt::aie::program(xe); }))
        .def("get_partition_size", &xrt::aie::program::get_partition_size,
             "Required partition size to run the program");
}
