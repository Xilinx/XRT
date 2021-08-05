/* SPDX-License-Identifier: Apache-2.0 */

/**
 * Pybind11 module for XRT C++ APIs
 *
 * Copyright (C) 2019-2021 Xilinx, Inc
 *
 * Authors: graham.schelle@xilinx.com
 *          sonal.santan@xilinx.com
 */


// XRT includes
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_graph.h"
#include "experimental/xrt_xclbin.h"

// Pybind11 includes
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/stl_bind.h>

// C++11 includes
#include <mutex>
#include <thread>
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

py::enum_<xclBOSyncDirection>(m, "xclBOSyncDirection")
    .value("XCL_BO_SYNC_BO_TO_DEVICE", xclBOSyncDirection::XCL_BO_SYNC_BO_TO_DEVICE)
    .value("XCL_BO_SYNC_BO_FROM_DEVICE", xclBOSyncDirection::XCL_BO_SYNC_BO_FROM_DEVICE)
    .value("XCL_BO_SYNC_BO_GMIO_TO_AIE", xclBOSyncDirection::XCL_BO_SYNC_BO_GMIO_TO_AIE)
    .value("XCL_BO_SYNC_BO_AIE_TO_GMIO", xclBOSyncDirection::XCL_BO_SYNC_BO_AIE_TO_GMIO);

py::enum_<ert_cmd_state>(m, "ert_cmd_state")
    .value("ERT_CMD_STATE_NEW", ert_cmd_state::ERT_CMD_STATE_NEW)
    .value("ERT_CMD_STATE_QUEUED", ert_cmd_state::ERT_CMD_STATE_QUEUED)
    .value("ERT_CMD_STATE_COMPLETED", ert_cmd_state::ERT_CMD_STATE_COMPLETED)
    .value("ERT_CMD_STATE_ERROR", ert_cmd_state::ERT_CMD_STATE_ERROR)
    .value("ERT_CMD_STATE_ABORT", ert_cmd_state::ERT_CMD_STATE_ABORT)
    .value("ERT_CMD_STATE_SUBMITTED", ert_cmd_state::ERT_CMD_STATE_SUBMITTED)
    .value("ERT_CMD_STATE_TIMEOUT", ert_cmd_state::ERT_CMD_STATE_TIMEOUT)
    .value("ERT_CMD_STATE_NORESPONSE", ert_cmd_state::ERT_CMD_STATE_NORESPONSE)
    .value("ERT_CMD_STATE_SKERROR", ert_cmd_state::ERT_CMD_STATE_SKERROR)
    .value("ERT_CMD_STATE_SKCRASHED", ert_cmd_state::ERT_CMD_STATE_SKCRASHED)
    .value("ERT_CMD_STATE_MAX", ert_cmd_state::ERT_CMD_STATE_MAX);

py::enum_<xrt::info::device>(m, "xrt_info_device")
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
    .value("dynamic_regions", xrt::info::device::dynamic_regions);
/*
 *
 * XRT:: UUID (needed since UUID classes passed outside of objects)
 *
 */
py::class_<xrt::uuid>(m, "uuid")
    .def(py::init<char *>())
    .def("to_string", &xrt::uuid::to_string);


/*
 *
 * xrt::device
 *
 */
py::class_<xrt::device>(m, "device")
    .def(py::init<>())
    .def(py::init<unsigned int>())
    .def(py::init([] (const std::string& bfd) {
                      return new xrt::device(bfd);
                  }))
    .def("load_xclbin", [](xrt::device& d, const std::string& xclbin) {
                            return d.load_xclbin(xclbin);
                        })
    .def("load_xclbin", [](xrt::device& d, const xrt::xclbin& xclbin) {
                            return d.load_xclbin(xclbin);
                        })
    .def("get_xclbin_uuid", &xrt::device::get_xclbin_uuid)
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
                         default:
                             return std::string("NA");
                         }
                     });


/*
 *
 * xrt::kernel
 *
 */
py::class_<xrt::run>(m, "run")
    .def(py::init<>())
    .def(py::init<const xrt::kernel &>())
    .def("start", [](xrt::run& r){
                       r.start();
                    })
    .def("set_arg", [](xrt::run& r, int i, xrt::bo& item){
                        r.set_arg(i, item);
                    })
    .def("set_arg", [](xrt::run& r, int i, int& item){
                        r.set_arg<int&>(i, item);
                    })
    .def("wait", ([](xrt::run& r, unsigned int timeout_ms)  {
                      return r.wait(timeout_ms);
                  }))
    .def("state", &xrt::run::state)
    .def("add_callback", &xrt::run::add_callback);

py::class_<xrt::kernel> pyker(m, "kernel");

py::enum_<xrt::kernel::cu_access_mode>(pyker, "cu_access_mode")
    .value("exclusive", xrt::kernel::cu_access_mode::exclusive)
    .value("shared", xrt::kernel::cu_access_mode::shared)
    .value("none", xrt::kernel::cu_access_mode::none)
    .export_values();


pyker.def(py::init([](const xrt::device& d, const xrt::uuid& u, const std::string& n,
                      xrt::kernel::cu_access_mode m) {
                       return new xrt::kernel(d, u, n, m);
                   }))
    .def("__call__", [](xrt::kernel& k, py::args args) -> xrt::run {
                         int i = 0;
                         xrt::run r(k);

                         for (auto item : args) {
                             try
                             { r.set_arg(i, item.cast<xrt::bo>()); }
                             catch (std::exception e) {  }

                             try
                             { r.set_arg<int>(i, item.cast<int>()); }
                             catch (std::exception e) {  }

                             i++;
                         }

                         r.start();
                         return r;
                     })
    .def("group_id", &xrt::kernel::group_id)
    .def("write_register", &xrt::kernel::write_register)
    .def("read_register", &xrt::kernel::read_register);


/*
 *
 * xrt::bo
 *
 */
py::class_<xrt::bo> pybo(m, "bo");

py::enum_<xrt::bo::flags>(pybo, "flags")
    .value("normal", xrt::bo::flags::normal)
    .value("cacheable", xrt::bo::flags::cacheable)
    .value("device_only", xrt::bo::flags::device_only)
    .value("host_only", xrt::bo::flags::host_only)
    .value("p2p", xrt::bo::flags::p2p)
    .value("svm", xrt::bo::flags::svm)
    .export_values();

pybo.def(py::init<xrt::device, size_t, xrt::bo::flags, xrt::memory_group>())
    .def(py::init<xrt::bo, size_t, size_t>())
    .def("write", ([](xrt::bo &b, py::buffer pyb, size_t seek)  {
                       py::buffer_info info = pyb.request();
                       b.write(info.ptr, info.itemsize * info.size , seek);
                   }))
    .def("read", ([](xrt::bo &b, size_t size, size_t skip) {
                      py::array_t<char> result = py::array_t<char>(size);
                      py::buffer_info bufinfo = result.request();
                      b.read(bufinfo.ptr, size, skip);
                      return result;
                  }))
    .def("sync", ([](xrt::bo &b, xclBOSyncDirection dir, size_t size, size_t offset)  {
                      b.sync(dir, size, offset);
                  }))
    .def("map", ([](xrt::bo &b)  {
                     return py::memoryview::from_memory(b.map(), b.size());
                  }))
    .def("size", &xrt::bo::size)
    .def("address", &xrt::bo::address)
    ;

/*
 *
 * xrt::xclbin::ip xrt::xclbin::kernel
 *
 */
py::class_<xrt::xclbin> pyxclbin(m, "xclbin");
py::class_<xrt::xclbin::ip> pyxclbinip(pyxclbin, "xclbinip");
py::bind_vector<std::vector<xrt::xclbin::ip>>(m, "xclbinip_vector");
py::class_<xrt::xclbin::kernel> pyxclbinkernel(pyxclbin, "xclbinkernel");
py::bind_vector<std::vector<xrt::xclbin::kernel>>(m, "xclbinkernel_vector");
py::class_<xrt::xclbin::mem> pyxclbinmem(pyxclbin, "xclbinmem");
py::bind_vector<std::vector<xrt::xclbin::mem>>(m, "xclbinmem_vector");


pyxclbinip.def(py::init<>())
    .def("get_name", &xrt::xclbin::ip::get_name);

pyxclbinkernel.def(py::init<>())
    .def("get_name", &xrt::xclbin::kernel::get_name);

pyxclbinmem.def(py::init<>())
    .def("get_tag", &xrt::xclbin::mem::get_tag)
    .def("get_base_address", &xrt::xclbin::mem::get_base_address)
    .def("get_size_kb", &xrt::xclbin::mem::get_size_kb)
    .def("get_used", &xrt::xclbin::mem::get_used)
    .def("get_index", &xrt::xclbin::mem::get_index);
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
    .def("get_ips", &xrt::xclbin::get_ips)
    .def("get_kernels", &xrt::xclbin::get_kernels)
    .def("get_xsa_name", &xrt::xclbin::get_xsa_name)
    .def("get_uuid", &xrt::xclbin::get_uuid)
    .def("get_mems", &xrt::xclbin::get_mems);


#if !defined(__x86_64__)
py::enum_<xrt::graph::access_mode>(m, "xrt_graph_access_mode")
    .value("exclusive", xrt::graph::access_mode::exclusive)
    .value("primary", xrt::graph::access_mode::primary)
    .value("shared", xrt::graph::access_mode::shared);


/*
 *
 * xrt::graph
 *
 */
py::class_<xrt::graph>(m, "graph")
    .def(py::init([] (const xrt::device& device, const xrt::uuid& xclbin_id, const std::string& name,
                      xrt::graph::access_mode am = xrt::graph::access_mode::primary) {
                      return new xrt::graph(device, xclbin_id, name, am);
                  }))
    .def("reset", &xrt::graph::reset)
    .def("get_timestamp", &xrt::graph::get_timestamp)
    .def("run", &xrt::graph::run)
    .def("wait", ([](xrt::graph &g, uint64_t cycles)  {
                      g.wait(cycles);
                  }))
    .def("wait", ([](xrt::graph &g, std::chrono::milliseconds timeout_ms)  {
                      g.wait(timeout_ms);
                  }))
    .def("suspend", &xrt::graph::suspend)
    .def("resume", &xrt::graph::resume)
    .def("end", &xrt::graph::end);
#endif
}
