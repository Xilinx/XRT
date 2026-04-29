/* SPDX-License-Identifier: Apache-2.0 */

/**
 * Pybind11 module for XRT C++ APIs
 *
 * Copyright (C) 2019-2026 Xilinx, Inc
 *
 * Authors: graham.schelle@xilinx.com
 *          sonal.santan@xilinx.com
 *          thomthehound@gmail.com
 */

// XRT includes
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"
#include "xrt/experimental/xrt_module.h"
#include "xrt/experimental/xrt_message.h"
#include "xrt/experimental/xrt_system.h"
#include "xrt/experimental/xrt_xclbin.h"
#include "xrt/experimental/xrt_elf.h"
#include "xrt/experimental/xrt_aie.h"
#include "xrt/experimental/xrt_ext.h"
#include "xrt/experimental/xrt_kernel.h"

// Pybind11 includes
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/stl_bind.h>

// C++11 includes
#include <stdexcept>
#include <string>

namespace py = pybind11;

//-----------------------------------------------------------------------------
// Deprecated Python compatibility shim for device.load_xclbin().
// Schedule for removal after downstream users have migrated to hw_context.

namespace {

class xrt_device_handle_guard
{
  xrtDeviceHandle handle = nullptr;

public:
  explicit
  xrt_device_handle_guard(const xrt::device& device)
    : handle(xrtDeviceOpenFromXcl(static_cast<xclDeviceHandle>(device)))
  {
    if (!handle)
      throw std::runtime_error("Failed to create temporary XRT device handle for load_xclbin compatibility shim");
  }

  xrt_device_handle_guard(const xrt_device_handle_guard&) = delete;
  xrt_device_handle_guard& operator=(const xrt_device_handle_guard&) = delete;
  xrt_device_handle_guard(xrt_device_handle_guard&&) = delete;
  xrt_device_handle_guard& operator=(xrt_device_handle_guard&&) = delete;

  ~xrt_device_handle_guard()
  {
    if (handle)
      xrtDeviceClose(handle);
  }

  xrtDeviceHandle
  get() const
  {
    return handle;
  }
};

void
warn_deprecated_load_xclbin()
{
  if (PyErr_WarnEx(PyExc_DeprecationWarning,
                   "pyxrt.device.load_xclbin() is deprecated; use pyxrt.hw_context(device, xclbin) or pyxrt.hw_context(device, xclbin_path) instead",
                   1) == -1)
    throw py::error_already_set();
}

void
warn_deprecated_kernel_device_constructor()
{
  if (PyErr_WarnEx(PyExc_DeprecationWarning,
                   "kernel(device, uuid, name[, mode]) is deprecated; use kernel(hw_context, name) instead.",
                   1) == -1)
    throw py::error_already_set();
}

xrt::kernel
kernel_device_compat(const xrt::device& device, const xrt::uuid& xclbin_id,
                     const std::string& name, xrt::kernel::cu_access_mode mode)
{
  // Convert kernel cu_access_mode to hw_context access_mode
  auto ctx_mode = (mode == xrt::kernel::cu_access_mode::exclusive)
      ? xrt::hw_context::access_mode::exclusive
      : xrt::hw_context::access_mode::shared;

  xrt::hw_context ctx(device, xclbin_id, ctx_mode);
  return {ctx, name};
}

void
bind_kernel_device_compat(py::class_<xrt::kernel>& pyker)  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
{
  pyker
    .def(py::init([](const xrt::device& d, const xrt::uuid& u, const std::string& n,
                     xrt::kernel::cu_access_mode m) {
        warn_deprecated_kernel_device_constructor();
        py::gil_scoped_release release;
        return new xrt::kernel(kernel_device_compat(d, u, n, m));
      }),
      py::arg("device"), py::arg("uuid"), py::arg("name"), py::arg("mode"),
      "Deprecated compatibility shim. Use kernel(hw_context, name) for new code")
    .def(py::init([](const xrt::device& d, const xrt::uuid& u, const std::string& n) {
        warn_deprecated_kernel_device_constructor();
        py::gil_scoped_release release;
        return new xrt::kernel(kernel_device_compat(d, u, n, xrt::kernel::cu_access_mode::shared));
      }),
      py::arg("device"), py::arg("uuid"), py::arg("name"),
      "Deprecated compatibility shim. Use kernel(hw_context, name) for new code");
}

xrt::uuid
load_xclbin_compat(const xrt::device& device, const xrt::xclbin& xclbin)
{
  xrt_device_handle_guard dhdl(device);

  if (xrtDeviceLoadXclbin(dhdl.get(), xclbin.get_axlf()) != 0)
    throw std::runtime_error("xrtDeviceLoadXclbin failed in load_xclbin compatibility shim");

  return xclbin.get_uuid();
}

void
bind_load_xclbin_compat(py::class_<xrt::device>& pydevice)
{
  pydevice
    .def("load_xclbin", [](const xrt::device& d, const std::string& xclbin_path) {
        warn_deprecated_load_xclbin();
        xrt::xclbin xclbin{xclbin_path};
        py::gil_scoped_release release;
        return load_xclbin_compat(d, xclbin);
      },
      py::arg("xclbin_path"),
      "Deprecated compatibility shim. Load an xclbin and return its UUID; use hw_context(device, xclbin_path) for new code")
    .def("load_xclbin", [](const xrt::device& d, const xrt::xclbin& xclbin) {
        warn_deprecated_load_xclbin();
        py::gil_scoped_release release;
        return load_xclbin_compat(d, xclbin);
      },
      py::arg("xclbin"),
      "Deprecated compatibility shim. Load an xclbin and return its UUID; use hw_context(device, xclbin) for new code");
}

} // namespace

// End shim.
//-----------------------------------------------------------------------------

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

    py::class_<xrt::hw_context> pyhwctx(
        m,
        "hw_context",
        "Hardware context for associating an xclbin or ELF configuration with device resources.");

    py::enum_<xrt::hw_context::access_mode>(pyhwctx, "access_mode", "Hardware context access mode")
        .value("exclusive", xrt::hw_context::access_mode::exclusive)
        .value("shared", xrt::hw_context::access_mode::shared)
        .export_values();

    pyhwctx.def(py::init<>(), "Create an empty hardware context handle.")
        .def(py::init([](const xrt::device& d, const xrt::uuid& u) {
            py::gil_scoped_release release;
            return new xrt::hw_context(d, u);
        }),
        py::arg("device"), py::arg("uuid"),
        "Create a hardware context for a previously registered xclbin UUID.")
        .def(py::init([](const xrt::device& d, const xrt::uuid& u, xrt::hw_context::access_mode mode) {
            py::gil_scoped_release release;
            return new xrt::hw_context(d, u, mode);
        }),
        py::arg("device"), py::arg("uuid"), py::arg("mode"),
        "Create a hardware context for a registered xclbin UUID with shared or exclusive access.")
        .def(py::init([](const xrt::device& d, const xrt::uuid& u, const xrt::hw_context::cfg_param_type& cfg_param) {
            py::gil_scoped_release release;
            return new xrt::hw_context(d, u, cfg_param);
        }),
        py::arg("device"), py::arg("uuid"), py::arg("cfg_param"),
        "Create a hardware context for a registered xclbin UUID with configuration parameters.")
        .def(py::init([](xrt::device& d, const xrt::xclbin& xclbin) {
            py::gil_scoped_release release;
            return new xrt::hw_context(d, d.register_xclbin(xclbin));
        }),
        py::arg("device"), py::arg("xclbin"),
        "Register an xclbin with the device and create a hardware context for it.")
        .def(py::init([](xrt::device& d, const xrt::xclbin& xclbin, xrt::hw_context::access_mode mode) {
            py::gil_scoped_release release;
            return new xrt::hw_context(d, d.register_xclbin(xclbin), mode);
        }),
        py::arg("device"), py::arg("xclbin"), py::arg("mode"),
        "Register an xclbin with the device and create a hardware context with the requested access mode.")
        .def(py::init([](xrt::device& d, const xrt::xclbin& xclbin, const xrt::hw_context::cfg_param_type& cfg_param) {
            py::gil_scoped_release release;
            return new xrt::hw_context(d, d.register_xclbin(xclbin), cfg_param);
        }),
        py::arg("device"), py::arg("xclbin"), py::arg("cfg_param"),
        "Register an xclbin with the device and create a hardware context with configuration parameters.")
        .def(py::init([](xrt::device& d, const std::string& xclbin_path) {
            py::gil_scoped_release release;
            xrt::xclbin xclbin{xclbin_path};
            return new xrt::hw_context(d, d.register_xclbin(xclbin));
        }),
        py::arg("device"), py::arg("xclbin_path"),
        "Load xclbin metadata from a path, register it with the device, and create a hardware context.")
        .def(py::init([](xrt::device& d, const std::string& xclbin_path, xrt::hw_context::access_mode mode) {
            py::gil_scoped_release release;
            xrt::xclbin xclbin{xclbin_path};
            return new xrt::hw_context(d, d.register_xclbin(xclbin), mode);
        }),
        py::arg("device"), py::arg("xclbin_path"), py::arg("mode"),
        "Load xclbin metadata from a path, register it with the device, and create a hardware context with the requested access mode.")
        .def(py::init([](xrt::device& d, const std::string& xclbin_path, const xrt::hw_context::cfg_param_type& cfg_param) {
            py::gil_scoped_release release;
            xrt::xclbin xclbin{xclbin_path};
            return new xrt::hw_context(d, d.register_xclbin(xclbin), cfg_param);
        }),
        py::arg("device"), py::arg("xclbin_path"), py::arg("cfg_param"),
        "Load xclbin metadata from a path, register it with the device, and create a hardware context with configuration parameters.")
        .def(py::init([](const xrt::device& device, const xrt::hw_context::cfg_param_type& cfg_param,
                         xrt::hw_context::access_mode mode) {
            py::gil_scoped_release release;
            return new xrt::hw_context(device, cfg_param, mode);
        }),
        py::arg("device"), py::arg("cfg_param"), py::arg("mode"),
        "Create a staged hardware context placeholder with configuration parameters and access mode. Use add_config() later to attach an ELF configuration.")
        .def(py::init([](const xrt::device& device, const xrt::elf& elf) {
            py::gil_scoped_release release;
            return new xrt::hw_context(device, elf);
        }),
        py::arg("device"), py::arg("elf"),
        "Create a hardware context from an ELF configuration object.")
        .def(py::init([](const xrt::device& device, const xrt::elf& elf,
                         const xrt::hw_context::cfg_param_type& cfg_param,
                         xrt::hw_context::access_mode mode) {
            py::gil_scoped_release release;
            return new xrt::hw_context(device, elf, cfg_param, mode);
        }),
        py::arg("device"), py::arg("elf"), py::arg("cfg_param"), py::arg("mode"),
        "Create a hardware context from an ELF configuration object with configuration parameters and access mode.")
        .def("add_config", [](xrt::hw_context& ctx, const xrt::elf& elf) {
            py::gil_scoped_release release;
            ctx.add_config(elf);
        }, py::arg("elf"),
        "Add an ELF configuration object to the hardware context.")
        .def("get_device", &xrt::hw_context::get_device,
             "Get the device from which the hardware context was created.")
        .def("get_xclbin_uuid", &xrt::hw_context::get_xclbin_uuid,
             "Get the UUID of the xclbin associated with the hardware context.")
        .def("get_xclbin", &xrt::hw_context::get_xclbin,
             "Get the xclbin associated with the hardware context.")
        .def("get_mode", &xrt::hw_context::get_mode,
             "Get the access mode of the hardware context.");

/*
 *
 * xrt::device
 *
 */

    py::class_<xrt::device> pydevice(m, "device", "Abstraction of an acceleration device");

    pydevice
        .def(py::init<>(), "Create an empty device object.")
        .def(py::init<unsigned int>(), py::arg("index"), "Open a device by index.")
        .def(py::init([] (const std::string& bfd) {
                          return new xrt::device(bfd);
                      }), py::arg("bdf"), "Open a device by BDF string.")
        .def("register_xclbin", [](xrt::device& d, const xrt::xclbin& xclbin) {
                                py::gil_scoped_release release;
                                return d.register_xclbin(xclbin);
                            }, py::arg("xclbin"),
                            "Register an xclbin with the device. Registration alone does not create a hardware context.");

    bind_load_xclbin_compat(pydevice);

    pydevice
        .def("get_xclbin_uuid", &xrt::device::get_xclbin_uuid, "Return the UUID of the xclbin currently associated with the device.")
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
        .def("wait2", [](xrt::run&r) { 
                            return r.wait2();
                    }, "Wait for the run to complete")
        .def("wait2", [](xrt::run&r, const std::chrono::milliseconds& timeout) {
                            return r.wait2(timeout);
                    }, "Wait for the specified milliseconds for the run to complete")
        .def("state", &xrt::run::state, "Return the current execution state of the run.")
        .def("add_callback", &xrt::run::add_callback, "Register a callback to be invoked when the run reaches the requested state.");

    py::class_<xrt::kernel> pyker(m, "kernel", "Represents a set of instances matching a specified name");

    py::enum_<xrt::kernel::cu_access_mode>(pyker, "cu_access_mode", "Compute unit access mode")
        .value("exclusive", xrt::kernel::cu_access_mode::exclusive)
        .value("shared", xrt::kernel::cu_access_mode::shared)
        .value("none", xrt::kernel::cu_access_mode::none)
        .export_values();

    bind_kernel_device_compat(pyker);

    pyker
        .def(py::init([](const xrt::hw_context& ctx, const std::string& n) {
                           py::gil_scoped_release release;
                           return new xrt::kernel(ctx, n);
                       }),
              py::arg("hw_context"), py::arg("name"),
              "Create kernel from hardware context and name (recommended)")
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
        .def("group_id", &xrt::kernel::group_id, "Get the memory-group identifier for a kernel argument.");

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

    pybo.def(py::init<xrt::device, size_t, xrt::bo::flags, xrt::memory_group>(),
             py::arg("device"), py::arg("size"), py::arg("flags"), py::arg("group"),
             "Create a buffer object on a device with the requested size, flags, and memory group.")
        .def(py::init<xrt::hw_context, size_t, xrt::bo::flags, xrt::memory_group>(),
             py::arg("hwctx"), py::arg("size"), py::arg("flags"), py::arg("group"),
             "Create a buffer object in a hardware context with the requested size, flags, and memory group.")
        .def(py::init<xrt::hw_context, size_t, xrt::memory_group>(),
             py::arg("hwctx"), py::arg("size"), py::arg("group"),
             "Create a buffer object in a hardware context using default flags.")
        .def(py::init<xrt::bo, size_t, size_t>(), "Create a sub-buffer view of an existing buffer object with the requested size and offset.")
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

    /*
    *
    * xrt::module
    * 
    */

    py::class_<xrt::module> pymodule(m, "module", "Executable hardware module created from an ELF image.");

    pymodule
        .def(py::init([](xrt::elf& xe) {
                return new xrt::module(xe);
        }), py::arg("elf"), "Create a hardware module from an ELF object.")
        .def("get_hw_context", &xrt::module::get_hw_context,
             "Get the hardware context associated with the module.");

    /*
    *
    * xrt::ext
    * 
    */

    py::module_ ext = m.def_submodule("ext", "Extended XRT functionality.");

    py::class_<xrt::ext::bo, xrt::bo> pyextbo(ext, "bo", "Extended buffer object with explicit sharing and access controls.");

    py::enum_<xrt::ext::bo::access_mode>(ext, "access_mode", "External buffer access mode")
        .value("none", xrt::ext::bo::access_mode::none)
        .value("read", xrt::ext::bo::access_mode::read)
        .value("write", xrt::ext::bo::access_mode::write)
        .value("read_write", xrt::ext::bo::access_mode::read_write)
        .value("local", xrt::ext::bo::access_mode::local)
        .value("shared", xrt::ext::bo::access_mode::shared)
        .value("process", xrt::ext::bo::access_mode::process)
        .value("hybrid", xrt::ext::bo::access_mode::hybrid)
        .export_values()
        .def("__or__", [](xrt::ext::bo::access_mode a, xrt::ext::bo::access_mode b) {
            return a | b;
        })
        .def("__and__", [](xrt::ext::bo::access_mode a, xrt::ext::bo::access_mode b) {
            return a & b;
        })
        .def("__ior__", [](xrt::ext::bo::access_mode &a, xrt::ext::bo::access_mode b) {
            a = a | b;
            return a;
        })
        .def("__iand__", [](xrt::ext::bo::access_mode &a, xrt::ext::bo::access_mode b) {
            a = a & b;
            return a;
        })
        ;
    
    pyextbo
        .def(py::init([](const xrt::device& device, void* userptr, size_t sz, xrt::ext::bo::access_mode access) {
            return new xrt::ext::bo(device, userptr, sz, access);
        }),
        py::arg("device"), py::arg("userptr"), py::arg("size"), py::arg("access"),
        "Create an extended buffer on a device from an existing user pointer with the requested access mode.")
        .def(py::init([](const xrt::device& device, void* userptr, size_t sz){
            return new xrt::ext::bo(device, userptr, sz);
        }),
        py::arg("device"), py::arg("userptr"), py::arg("size"),
        "Create an extended buffer on a device from an existing user pointer.")
        .def(py::init([](const xrt::device& device, size_t sz, xrt::ext::bo::access_mode access) {
            return new xrt::ext::bo(device, sz, access);
        }),
        py::arg("device"), py::arg("size"), py::arg("access"),
        "Create an extended buffer on a device with the requested access mode.")
        .def(py::init([](const xrt::device& device, size_t sz) {
            return new xrt::ext::bo(device, sz);
        }),
        py::arg("device"), py::arg("size"),
        "Create an extended buffer on a device with the default access mode.")
        .def(py::init([](const xrt::device& device, xrt::pid_type pid, xrt::bo::export_handle ehdl) {
            return new xrt::ext::bo(device, pid, ehdl);
        }),
        py::arg("device"), py::arg("pid"), py::arg("export_handle"),
        "Import an extended buffer from another process using an exported handle.")
        .def(py::init([](const xrt::hw_context& hwctx, size_t sz, xrt::ext::bo::access_mode access) {
            return new xrt::ext::bo(hwctx, sz, access);
        }),
        py::arg("hwctx"), py::arg("size"), py::arg("access"),
        "Create an extended buffer in a hardware context with the requested access mode.")
        .def(py::init([](const xrt::hw_context& hwctx, size_t sz) {
            return new xrt::ext::bo(hwctx, sz);
        }),
        py::arg("hwctx"), py::arg("size"),
        "Create an extended buffer in a hardware context with the default access mode.")
        .def(py::init([](const xrt::hw_context& hwctx, xrt::pid_type pid, xrt::bo::export_handle ehdl) {
            return new xrt::ext::bo(hwctx, pid, ehdl);
        }),
        py::arg("hwctx"), py::arg("pid"), py::arg("export_handle"),
        "Import an extended buffer into a hardware context from another process using an exported handle.");
    
    py::class_<xrt::ext::kernel, xrt::kernel> pyextkernel(ext, "kernel", "Extended kernel object for module-backed and shared workflows.");

    pyextkernel
        .def(py::init([](const xrt::hw_context& ctx, const xrt::module& mod, const std::string& name) {
            return new xrt::ext::kernel(ctx, mod, name);
        }),
        py::arg("hwctx"), py::arg("module"), py::arg("name"),
        "Create an extended kernel from a hardware context, a module, and a kernel name.")
        .def(py::init([](const xrt::hw_context& ctx, const std::string& name) {
            return new xrt::ext::kernel(ctx, name);
        }),
        py::arg("hwctx"), py::arg("name"),
        "Create an extended kernel from a hardware context and a kernel name.");

    /*
    *
    * xrt::runlist
    * 
    */

    py::class_<xrt::runlist> pyrunlist(m, "runlist", "Ordered collection of runs executed as a unit.");
    
    pyrunlist
        .def(py::init([](){
            return new xrt::runlist();
        }), "Create an empty runlist.")
        .def(py::init([](const xrt::hw_context& hwctx) {
            return new xrt::runlist(hwctx);
        }), py::arg("hwctx"), "Create a runlist associated with a hardware context.")
        .def("add", ([](xrt::runlist &r, const xrt::run& run) {
            r.add(run);
        }), py::arg("run"), "Add a run to the runlist")
        .def("execute", ([](xrt::runlist &r) {
            r.execute();
        }), "Execute all runs in the runlist")
        .def("wait", ([](xrt::runlist &r) {
            r.wait();
        }), "Wait for all runs in the runlist to complete")
        .def("wait", ([](xrt::runlist &r, const std::chrono::milliseconds& timeout) {
            return r.wait(timeout);
        }), py::arg("timeout"),
        "Wait for the specified timeout for the runlist to complete");
        
}
