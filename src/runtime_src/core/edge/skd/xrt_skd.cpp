/*
 * Copyright (C) 2022, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

#include "xrt_skd.h"

using ms_t = std::chrono::microseconds;
using Clock = std::chrono::high_resolution_clock;

// For use-case where map and unmap of buffers for each PS kernel call comes into critical path,
// uncomment below line to enable mapping entire DDR reserved space for faster buffer access
// #define SKD_MAP_BIG_BO

namespace xrt {

  using severity_level = xrt_core::message::severity_level;
  
  /**
   * skd() - Constructor from uuid and soft kernel section
   *
   * @param kernel metadata buffer handle
   *
   * @param soft kernel image buffer handle
   *
   * @param soft kernel name
   *
   * @param soft kernel CU index
   *
   */
  skd::skd(const xclDeviceHandle handle, const int sk_meta_bohdl, const int sk_bohdl, const std::string kname, const uint32_t cu_index,
	   unsigned char *uuid_in, const int parent_mem_bo_in, const uint64_t mem_start_paddr_in, const uint64_t mem_size_in)
    : m_sk_name(kname), m_parent_devhdl(handle), m_cu_idx(cu_index), m_sk_bo(sk_bohdl), m_sk_meta_bo(sk_meta_bohdl),
      m_xclbin_uuid(uuid_in), m_parent_bo_handle(parent_mem_bo_in), m_mem_start_paddr(mem_start_paddr_in), m_mem_size(mem_size_in),
      m_sk_path(std::string(SOFT_KERNEL_FILE_PATH)+m_xclbin_uuid.to_string()+"/"+kname)
  {
  }

  XCL_DRIVER_DLLESPEC
  int
  skd::init() {
    // Create soft kernel file from sk_bo
    int ret = create_softkernelfile(m_parent_devhdl, m_sk_bo);
    if(ret)
      return ret;

    // Open and load the soft kernel.
    m_sk_handle = dlopen(m_sk_path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (m_sk_handle == nullptr) {
      const std::string errstr = dlerror();
      std::string msg = std::string("Dynamic Link error: ") + errstr + std::string("Cannot open ") + m_sk_path.string();
      xrt_core::message::send(severity_level::error, "SKD", msg);
      return -ELIBACC; // Return ELIBACC - Can not access a needed shared library
    }

    // Extract arguments from sk_meta_bohdl
    xclBOProperties prop = {};
    ret = xclGetBOProperties(m_parent_devhdl, m_sk_meta_bo, &prop);
    if (ret) {
      std::string msg("Cannot get metadata BO info");
      xrt_core::message::send(severity_level::error, "SKD", msg);
      xclFreeBO(m_parent_devhdl, m_sk_meta_bo);
      return ret;
    }

    auto buf = reinterpret_cast<char *>(xclMapBO(m_parent_devhdl, m_sk_meta_bo, false));
    if (buf == MAP_FAILED) {
      std::string msg("Cannot map metadata BO!");
      xrt_core::message::send(severity_level::error, "SKD", msg);
      xclFreeBO(m_parent_devhdl, m_sk_meta_bo);
      return -errno;
    }
    m_kernel_args = xrt_core::xclbin::get_kernel_arguments(buf, prop.size, m_sk_name);
    m_return_offset = get_return_offset(m_kernel_args);
    std::string msg = std::string("Return offset = ") + std::to_string(m_return_offset);
    xrt_core::message::send(severity_level::debug, "SKD", msg);
    msg = std::string("Num args = ")  + std::to_string(m_kernel_args.size());
    xrt_core::message::send(severity_level::debug, "SKD", msg);
    munmap(buf, prop.size);

    // new device handle for the current instance
    m_devhdl = xclOpen(0, nullptr, XCL_QUIET);
    if (m_devhdl == nullptr) {
	const std::string errMsg = "Cannot open XCL device handle";
	xrt_core::message::send(severity_level::error, "SKD", errMsg);
    }
    m_xrtdhdl = xrtDeviceOpenFromXcl(m_devhdl);
    if (m_devhdl == nullptr) {
	const std::string errMsg = "Cannot open XRT device handle";
	xrt_core::message::send(severity_level::error, "SKD", errMsg);
    }

    // Map entire PS reserve memory space
#ifdef SKD_MAP_BIG_BO
    m_mem_start_vaddr = xclMapBO(m_parent_devhdl,m_parent_bo_handle,true);
    if (m_mem_start_vaddr == MAP_FAILED) {
      std::string msg("Cannot map PS kernel Mem BO!");
      xrt_core::message::send(severity_level::error, "SKD", msg);
      return -EINVAL;
    }
    msg = std::string("host_mem_size=") + m_mem_size
      + ", host_mem_paddr=" + m_mem_start_paddr
      + ", host_mem_vaddr=" + std::to_string(&m_mem_start_vaddr));
    xrt_core::message::send(severity_level::debug, "SKD", msg);
#endif
    
    // Check for soft kernel init function
    kernel_init_t kernel_init;
    std::string sk_init(std::string(m_sk_name) + std::string("_init"));

    kernel_init = reinterpret_cast<kernel_init_t>(dlsym(m_sk_handle, sk_init.c_str()));
    if (kernel_init) {
	ret = xrtDeviceLoadXclbinUUID(m_xrtdhdl, m_xclbin_uuid.get());
	if (ret) {
	    msg = std::string("Cannot load xclbin from UUID!");
	    xrt_core::message::send(severity_level::error, "SKD", msg);
	    return ret;
	}
	m_xrtHandle = kernel_init(m_devhdl, m_xclbin_uuid.get());
	if(m_xrtHandle) {
	    m_pass_xrtHandles = true;
	    msg = std::string("kernel init function found! Will pass xrtHandles to soft kernel");
	    xrt_core::message::send(severity_level::debug, "SKD", msg);
	} else {
	    msg = std::string("kernel init function did not return valid xrtHandles!");
	    xrt_core::message::send(severity_level::error, "SKD", msg);
	    return -EINVAL;
	}
    }

    // Open main soft kernel
    m_kernel = dlsym(m_sk_handle, m_sk_name.c_str());
    if (m_kernel == nullptr) {
      const std::string errstr = dlerror();
      msg = std::string("Dynamic Link error: ") + errstr
	+ std::string("Cannot find kernel %s\n") + m_sk_name;
      xrt_core::message::send(severity_level::error, "SKD", msg);
      return -ELIBACC;
    }

    // Soft kernel command bohandle init
    ret = create_softkernel(&m_cmd_boh);
    if (ret) {
      msg = std::string("Cannot create soft kernel.");
      xrt_core::message::send(severity_level::error, "SKD", msg);
      return ret;
    }

    msg = m_sk_name + std::to_string(m_cu_idx) + std::string(" start running, cmd_boh = ") + std::to_string(m_cmd_boh);
    xrt_core::message::send(severity_level::info, "SKD", msg);

    m_args_from_host = reinterpret_cast<unsigned *>(xclMapBO(m_devhdl, m_cmd_boh, true));
    if (m_args_from_host == MAP_FAILED) {
      msg = std::string("Failed to map soft kernel args for ") + m_sk_name + std::to_string(m_cu_idx);
      xrt_core::message::send(severity_level::error, "SKD", msg);
      dlclose(m_sk_handle);
      return -EINVAL;
    }

    // Prep FFI type for all kernel arguments
    for(const auto &i : m_kernel_args) {
      m_ffi_args.emplace_back(convert_to_ffitype(i));
    }

    // Expect PS kernels to return POSIX return code 
    if(ffi_prep_cif(&m_cif,FFI_DEFAULT_ABI, m_kernel_args.size(), &ffi_type_uint32, m_ffi_args.data()) != FFI_OK) {
      const std::string errMsg = "Cannot prep FFI arguments!";
      xrt_core::message::send(severity_level::error, "SKD", errMsg);
      return -EINVAL;
    }

    msg = std::string("Finish soft kernel %s init\n") + m_sk_name;
    xrt_core::message::send(severity_level::debug, "SKD", msg);
    return 0;
  }

  XCL_DRIVER_DLLESPEC
  void
  skd::run() {
    ffi_arg kernel_return = 0;
    int ret = 0;
    std::vector<void*> ffi_arg_values(m_kernel_args.size());
    // Buffer Objects
    std::vector<size_t> bo_offsets(m_kernel_args.size());
    std::vector<void*> bos(m_kernel_args.size());
    std::vector<size_t> bo_size(m_kernel_args.size());
    std::vector<int> bo_handles(m_kernel_args.size());
    std::vector<int> bo_list;
    Clock::time_point start;
    Clock::time_point end;
    Clock::time_point cmd_start;
    Clock::time_point cmd_end;

    while (true) {
      ret = wait_next_cmd();
      cmd_start = Clock::now();
      if(cmd_end < cmd_start) {
	  std::string msg = std::string("PS Kernel Command interval = ") + std::to_string((std::chrono::duration_cast<ms_t>(cmd_start - cmd_end)).count());
	  xrt_core::message::send(severity_level::info, "SKD", msg);
      }

      if (ret && (signal==SIGTERM)) {
	  // We are told to exit the soft kernel loop
	  std::string msg = std::string("Exit soft kernel %s\n") + m_sk_name;
	  xrt_core::message::send(severity_level::info, "SKD", msg);
	  break;
      }

      // Reg file indicates the kernel should not be running.
      if (!(m_args_from_host[0] & 0x1))
	continue; //AP_START bit is not set; New Cmd is not available

      // FFI PS Kernel implementation
      // Map buffers used by kernel
      for(int i=0;i<m_kernel_args.size();i++) {
	if((m_kernel_args[i].index == xrt_core::xclbin::kernel_argument::no_index) && (m_kernel_args[i].hosttype.compare("xrtHandles*")==0)) {
	  ffi_arg_values[i] = &m_xrtHandle;
	} else if(m_kernel_args[i].type == xrt_core::xclbin::kernel_argument::argtype::global) {
	  auto buf_addr_ptr = reinterpret_cast<uint64_t *>(&m_args_from_host[(m_kernel_args[i].offset + PS_KERNEL_REG_OFFSET) / 4]);
	  auto buf_addr = reinterpret_cast<uint64_t>(*buf_addr_ptr);
	  auto buf_size_ptr = reinterpret_cast<uint64_t *>(&m_args_from_host[(m_kernel_args[i].offset + PS_KERNEL_REG_OFFSET) / 4 + 2]);
	  auto buf_size = reinterpret_cast<uint64_t>(*buf_size_ptr);

#ifdef SKD_MAP_BIG_BO
	  bo_offsets[i] = buf_addr - mem_start_paddr;
	  bos[i] = mem_start_vaddr + bo_offsets[i];
#else
	  bo_size[i] = buf_size;
	  bo_handles[i] = xclGetHostBO(m_devhdl, buf_addr, buf_size);
	  bos[i] = xclMapBO(m_devhdl, bo_handles[i], true);
	  bo_list.emplace_back(i);
#endif
	  std::stringstream strm;
	  strm << bos[i];
	  std::string msg = std::string("BO Virtual Address = ") + strm.str();
	  xrt_core::message::send(severity_level::debug, "SKD", msg);
	  ffi_arg_values[i] = &bos[i];
	} else {
	  ffi_arg_values[i] = &m_args_from_host[(m_kernel_args[i].offset + PS_KERNEL_REG_OFFSET) / 4];
	}
      }

      start = Clock::now();
      ffi_call(&m_cif,FFI_FN(m_kernel), &kernel_return, ffi_arg_values.data());
      end = Clock::now();
      m_args_from_host[m_return_offset] = static_cast<uint32_t>(kernel_return);  // FFI return type is define as ffi_type_uint32

      std::string msg = std::string("PS Kernel duration = ") + std::to_string((std::chrono::duration_cast<ms_t>(end - start)).count());
      xrt_core::message::send(severity_level::info, "SKD", msg);

#ifdef SKD_MAP_BIG_BO
#else
      // Unmap Buffers
      for(auto i:bo_list) {
	munmap(bos[i],bo_size[i]);
	xclFreeBO(m_devhdl,bo_handles[i]);
      }
      bo_list.clear();
#endif

      cmd_end = Clock::now();
      msg = std::string("PS Kernel Command duration = ") + std::to_string((std::chrono::duration_cast<ms_t>(cmd_end - cmd_start)).count())
	  + std::string(", Preproc = ") + std::to_string((std::chrono::duration_cast<ms_t>(start - cmd_start)).count())
	  + std::string(", Postproc = ") + std::to_string((std::chrono::duration_cast<ms_t>(cmd_end - end)).count());
      xrt_core::message::send(severity_level::info, "SKD", msg);
    }
  }

  skd::~skd() {
    int ret = 0;

    // Check if SCU is still in running state
    // If it is, that means it has crashed
    if((m_args_from_host[0] & 0x1) == 1) {
      report_crash();  // Function to report crash to kernel - not implemented yet in kernel space
    }
#ifdef SKD_MAP_BIG_BO
    // Unmap mem BO
    ret = xclUnmapBO(m_parent_devhdl, m_parent_bo_handle, m_mem_start_vaddr);
    if (ret) {
	std::string msg = std::string("Cannot munmap mem BO ") + std::to_string(m_parent_bo_handle)
	    + std::string(", at ") +  std::to_string(m_mem_start_vaddr);
	xrt_core::message::send(severity_level::error, "SKD", msg);
    }
#endif
    // Unmap command BO
    if(m_cmd_boh >= 0) {
	xclBOProperties prop = {};
	ret = xclGetBOProperties(m_devhdl, m_cmd_boh, &prop);
	if (ret) {
	    std::string msg = std::string("Cannot get BO property of ") + std::to_string(m_cmd_boh);
	    xrt_core::message::send(severity_level::error, "SKD", msg);
	}
	ret = xclUnmapBO(m_devhdl, m_cmd_boh, m_args_from_host);
	if (ret) {
	    std::string msg = std::string("Cannot munmap BO ") + std::to_string(m_cmd_boh);
	    xrt_core::message::send(severity_level::error, "SKD", msg);
	}
    }

    // Call soft kernel fini if available
    kernel_fini_t kernel_fini;
    std::string sk_fini(std::string(m_sk_name) + std::string("_fini"));
    kernel_fini = reinterpret_cast<kernel_fini_t>(dlsym(m_sk_handle, sk_fini.c_str()));
    if (kernel_fini) {
      ret = kernel_fini(m_xrtHandle);
    }

    dlclose(m_sk_handle);
    ret = delete_softkernelfile();
    if (ret) {
	std::string msg = std::string("Cannot remove soft kernel file ") +  m_sk_path.string();
	xrt_core::message::send(severity_level::error, "SKD", msg);
    }
    xclClose(m_devhdl);
    report_fini();
  }

  int skd::create_softkernel(int *boh) {
    return xclSKCreate(m_devhdl, boh, m_cu_idx);
  }
  int skd::wait_next_cmd() {
    return xclSKReport(m_devhdl, m_cu_idx, XRT_SCU_STATE_DONE);
  }
  void skd::set_signal(int sig) {
    signal = sig;
  }

  /*
   * This function create a soft kernel file.
   */
  int skd::create_softkernelfile(xclDeviceHandle handle, int bohdl)
  {
      xclBOProperties prop = {};
      int ret = xclGetBOProperties(handle, bohdl, &prop);
      if (ret) {
	  const std::string msg = "Unable to get BO properties!";
	  xrt_core::message::send(severity_level::error, "SKD", msg);
      }

      auto buf = reinterpret_cast<char *>(xclMapBO(handle, bohdl, false));
      if(buf == MAP_FAILED) {
	  const std::string msg = "Cannot map soft kernel BO!";
	  xrt_core::message::send(severity_level::error, "SKD", msg);
	  return -errno;
      }

      const std::filesystem::path path(SOFT_KERNEL_FILE_PATH+m_xclbin_uuid.to_string()+"/");
      xrt_core::message::send(severity_level::info, "SKD", path.string());

      if(!std::filesystem::exists(path))
	  std::filesystem::create_directories(path);

      // Check if file is the same
      if(!std::filesystem::exists(m_sk_path)) {
	  std::ofstream fptr(m_sk_path, std::ios::out | std::ios::binary);
	  if (!fptr.is_open()) {
	      std::string msg = std::string("Cannot create file: ") + m_sk_path.string();
	      xrt_core::message::send(severity_level::error, "SKD", msg);
	      xclUnmapBO(handle, bohdl, buf);
	      return -EPERM;
	  }
	  
	  // copy the soft kernel to file
	  fptr.write(buf, prop.size);
	  if (fptr.fail()) {
	      std::string msg = std::string("Fail to write to file ") + m_sk_path.string();
	      xrt_core::message::send(severity_level::error, "SKD", msg);
	      fptr.close();
	      xclUnmapBO(handle, bohdl, buf);
	      return -EIO;
	  }
	  std::string msg = std::string("File created at %s\n") + m_sk_path.string();
	  xrt_core::message::send(severity_level::info, "SKD", msg);
	  
	  fptr.close();
	  xclUnmapBO(handle, bohdl, buf);
      }

      return 0;
  }

  /*
   * This function delete the soft kernel file.
   */
  int skd::delete_softkernelfile()
  {
      if(std::filesystem::exists(m_sk_path))
	  return(remove(m_sk_path.c_str()));
      else
	  return 0;
  }

  // Convert argument to ffi_type 
  ffi_type* skd::convert_to_ffitype(xrt_core::xclbin::kernel_argument arg) {
    ffi_type* return_type;
    // Mapping for FFI types
    static const std::map<std::pair<std::string, int>, ffi_type*> typeTable = {
      { {"uint", 1 }, &ffi_type_uint8 },
      { {"uint8_t", 1 }, &ffi_type_uint8 },
      { {"int", 1 }, &ffi_type_sint8 },
      { {"int8_t", 1 }, &ffi_type_sint8 },
      { {"char", 1 }, &ffi_type_sint8 },
      { {"unsigned char", 1 }, &ffi_type_uint8 },
      { {"uint", 2 }, &ffi_type_uint16 },
      { {"uint16_t", 2 }, &ffi_type_uint16 },
      { {"int", 2 }, &ffi_type_sint16 },
      { {"int16_t", 2 }, &ffi_type_sint16 },
      { {"uint", 4 }, &ffi_type_uint32 },
      { {"uint32_t", 4 }, &ffi_type_uint32 },
      { {"int", 4 }, &ffi_type_sint32 },
      { {"int32_t", 4 }, &ffi_type_sint32 },
      { {"uint", 8 }, &ffi_type_uint64 },
      { {"uint64_t", 8 }, &ffi_type_uint64 },
      { {"int", 8 }, &ffi_type_sint64 },
      { {"int64_t", 8 }, &ffi_type_sint64 },
      { {"float", 4 }, &ffi_type_float },
      { {"float", 8 }, &ffi_type_double },
      { {"double", 8 }, &ffi_type_double }
    };

    if((arg.index == xrt_core::xclbin::kernel_argument::no_index) ||  // Argument is xrtHandles
       (arg.type == xrt_core::xclbin::kernel_argument::argtype::global)) {  // Argument is a buffer pointer
      return_type = &ffi_type_pointer;
      return(return_type);
    } else {
      auto result = typeTable.find({ arg.hosttype, arg.size});
      return_type = result->second;
      return(return_type);
    }
  }

  // Respond to SCU subdevice PS kernel initialization is done
  void skd::report_ready() {
    xclSKReport(m_devhdl,m_cu_idx,XRT_SCU_STATE_READY);
  }

  void skd::report_fini() {
    xclSKReport(m_devhdl,m_cu_idx,XRT_SCU_STATE_FINI);
  }

  void skd::report_crash() {
    xclSKReport(m_devhdl,m_cu_idx,XRT_SCU_STATE_CRASH);
  }

  int skd::get_return_offset(std::vector<xrt_core::xclbin::kernel_argument> args) {
    int return_offset = 1;

    // Calculate offset to write return code into
    // If the last argument is a global which means there will be 64-bit address and 64-bit size for total of 16 bytes
    // Else the last argument size will be either 4-bytes or 8 bytes since arguments are 32-bit aligned
    if(args.back().type == xrt_core::xclbin::kernel_argument::argtype::global)
      return_offset = (args.back().offset+PS_KERNEL_REG_OFFSET+16)/4;
    else
      return_offset = (args.back().offset+PS_KERNEL_REG_OFFSET+((args.back().size>4)?8:4))/4;

    return return_offset;
  }

}
