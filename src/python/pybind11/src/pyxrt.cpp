/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */


// XRT includes
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"

// Pybind11 includes
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

namespace py = pybind11;

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
    .value("XCL_BO_SYNC_BO_FROM_DEVICE", xclBOSyncDirection::XCL_BO_SYNC_BO_FROM_DEVICE);


/*
 *
 * XRT:: UUID (needed since UUID classes passed outside of objects)
 *
 */
py::class_<xrt::uuid>(m, "uuid")
   .def(py::init<char *>())
   .def("get",
      [](xrt::uuid & u){
        py::array_t<unsigned char> result = py::array_t<unsigned char>(16);
        py::buffer_info bufinfo = result.request();
        unsigned char* bufptr = (unsigned char*) bufinfo.ptr;
        memcpy(bufptr,u.get(),16);
        return result;
     }
    )
    .def("to_string", &xrt::uuid::to_string)
    ;


/*
 *
 * XRT::Device
 *
 */
py::class_<xrt::device>(m, "device")
    .def(py::init<>())
    .def(py::init<unsigned int>())
    .def("load_xclbin",
       [](xrt::device & d, const std::string& xclbin){
	     return d.load_xclbin(xclbin);
       }
     )
    .def("get_xclbin_uuid", &xrt::device::get_xclbin_uuid)
    ;


/*
 *
 * XRT::Kernel
 *
 */
py::class_<xrt::run>(m, "run")
   .def(py::init<>())
   .def(py::init<const xrt::kernel &>()) 
   .def("start", &xrt::run::start)
   .def("set_arg", [](xrt::run & r, int i, xrt::bo & item){
       r.set_arg(i, item);
     })
   .def("set_arg", [](xrt::run & r, int i, int & item){
       r.set_arg<int>(i, item);
     })  
   .def("wait", [](xrt::run & r) {
       r.wait();
     })
    .def("state", &xrt::run::state)
    .def("add_callback", &xrt::run::add_callback)
    ;

py::class_<xrt::kernel>(m, "kernel")
    .def(py::init([](xrt::device d, const py::array_t<unsigned char> u, const std::string & n, bool e){
        return new xrt::kernel(d, (const unsigned char*) u.request().ptr, n, e);
    }))
  .def("__call__", [](xrt::kernel & k, py::args args) -> xrt::run {    
	int i =0;
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
    .def("read_register", &xrt::kernel::read_register)
    ;


/*
 *
 * XRT:: BO
 *
 */
py::class_<xrt::bo>(m, "bo")
    .def(py::init<xrt::device,size_t,xrt::buffer_flags,xrt::memory_group>())
    .def("write", ([](xrt::bo &b, py::array_t<int> pyb, size_t seek)  {
	  py::buffer_info info = pyb.request();
	  int* pybptr = (int*) info.ptr;

	  b.write(info.ptr, info.itemsize * info.size , 0);
    }))
    .def("read", ([](xrt::bo &b, size_t size, size_t skip) {
	  int nitems = size/sizeof(int);
	  py::array_t<int> result = py::array_t<int>(nitems);
	  
	  py::buffer_info bufinfo = result.request();
	  int* bufptr = (int*) bufinfo.ptr;
	  b.read(bufptr, size, skip);
	  return result;
     }))
  .def("sync", ([](xrt::bo &b, xclBOSyncDirection dir, size_t size, size_t offset)  {	
	b.sync(dir, size, offset);
      }))
    ;
}
