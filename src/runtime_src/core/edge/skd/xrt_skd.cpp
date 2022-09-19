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

#ifdef __GNUC__
# pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

namespace xrt {

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
  skd::skd(xclDeviceHandle handle, int sk_meta_bohdl, int sk_bohdl, char *kname, uint32_t cu_index, unsigned char *uuid) {
    strcpy(sk_name,kname);
    parent_devHdl = handle;
    cu_idx = cu_index;
    sk_bo = sk_bohdl;
    sk_meta_bo = sk_meta_bohdl;
    memcpy(xclbin_uuid,uuid,sizeof(xclbin_uuid));

    // Set sk_path according to sk_name
    snprintf(sk_path, XRT_MAX_PATH_LENGTH, "%s%s%d", SOFT_KERNEL_FILE_PATH,
	     sk_name,cu_idx);
  }

  XCL_DRIVER_DLLESPEC
  int
  skd::init() {
    void *buf = NULL;
    xclBOProperties prop;
    int ret = 0;

    // Create soft kernel file from sk_bo
    if(createSoftKernelFile(parent_devHdl, sk_bo) != 0)
      return -1;

    /* Open and load the soft kernel. */
    sk_handle = dlopen(sk_path, RTLD_LAZY | RTLD_GLOBAL);
    char *errstr = dlerror();
    if(errstr != NULL) {
      syslog(LOG_ERR, "Dynamic Link error: %s\n", errstr);
      return -1;
    }
    if (!sk_handle) {
      syslog(LOG_ERR, "Cannot open %s\n", sk_path);
      return -1;
    }

    // Extract arguments from sk_meta_bohdl
    if (xclGetBOProperties(parent_devHdl, sk_meta_bo, &prop)) {
      syslog(LOG_ERR, "Cannot get metadata BO info.\n");
      xclFreeBO(parent_devHdl, sk_meta_bo);
      return -1;
    }

    buf = xclMapBO(parent_devHdl, sk_meta_bo, false);
    if (!buf) {
      syslog(LOG_ERR, "Cannot map metadata BO.\n");
      xclFreeBO(parent_devHdl, sk_meta_bo);
      return -1;
    }
    args = xrt_core::xclbin::get_kernel_arguments((char *)buf,prop.size,sk_name);
    num_args = args.size();
    // Calculate offset to write return code into
    // If the last argument is a global which means there will be 64-bit address and 64-bit size for total of 16 bytes
    // Else the last argument size will be either 4-bytes or 8 bytes since arguments are 32-bit aligned
    if(args[num_args-1].type == xrt_core::xclbin::kernel_argument::argtype::global)
      return_offset = (args[num_args-1].offset+PS_KERNEL_REG_OFFSET+16)/4;
    else
      return_offset = (args[num_args-1].offset+PS_KERNEL_REG_OFFSET+((args[num_args-1].size>4)?8:4))/4;
    syslog(LOG_INFO,"Return offset = %d\n",return_offset);
    syslog(LOG_INFO,"Num args = %d\n",num_args);
    munmap(buf, prop.size);

    // new device handle for the current instance
    devHdl = xclOpen(0, NULL, XCL_QUIET);
    xrtdHdl = xrtDeviceOpenFromXcl(devHdl);

    // Check for soft kernel init function
    kernel_init_t kernel_init;
    std::string initExtension = "_init";
    char sk_init[PS_KERNEL_NAME_LENGTH+initExtension.size()];

    snprintf(sk_init,PS_KERNEL_NAME_LENGTH+initExtension.size(),"%s%s",sk_name,initExtension.c_str());
    kernel_init = (kernel_init_t)dlsym(sk_handle, sk_init);
    if (!kernel_init) {
      syslog(LOG_INFO, "kernel init function %s not found\n", sk_init);
    } else {
      int ret = 0;
      ret = xrtDeviceLoadXclbinUUID(xrtdHdl,reinterpret_cast<unsigned char*>(xclbin_uuid));
      if(ret) {
	syslog(LOG_ERR, "Cannot load xclbin from UUID!\n");
	return -1;
      } else {
	syslog(LOG_INFO, "Finished loading xclbin from UUID.\n");
      }
      xrtHandle = kernel_init(devHdl,reinterpret_cast<unsigned char*>(xclbin_uuid));
      if(xrtHandle) {
	pass_xrtHandles = true;
	syslog(LOG_INFO, "kernel init function found! Will pass xrtHandles to soft kernel\n");
      } else {
	syslog(LOG_ERR, "kernel init function did not return valid xrtHandles!\n");
	return -1;
      }
    }
    ffi_args = new ffi_type*[num_args];

    // Open main soft kernel
    kernel = dlsym(sk_handle, sk_name);
    errstr = dlerror();
    if(errstr != NULL) {
      syslog(LOG_ERR, "Dynamic Link error: %s\n", errstr);
      return -1;
    }
    if (!kernel) {
      syslog(LOG_ERR, "Cannot find kernel %s\n", sk_name);
      return -1;
    }

    // Soft kernel command bohandle init
    ret = createSoftKernel(&cmd_boh);
    if (ret) {
      syslog(LOG_ERR, "Cannot create soft kernel.");
      return -1;
    }

    syslog(LOG_INFO, "%s_%d start running, cmd_boh = %d\n", sk_name, cu_idx, cmd_boh);

    args_from_host = (unsigned *)xclMapBO(devHdl, cmd_boh, true);;
    if (args_from_host == MAP_FAILED) {
      syslog(LOG_ERR, "Failed to map soft kernel args for %s_%d", sk_name, cu_idx);
      dlclose(sk_handle);
      return -1;
    }

    // Prep FFI type for all kernel arguments
    for(int i=0;i<num_args;i++) {
      ffi_args[i] = convert_to_ffitype(args[i]);
    }

    if(ffi_prep_cif(&cif,FFI_DEFAULT_ABI, num_args, &ffi_type_uint32,ffi_args) != FFI_OK) {
      syslog(LOG_ERR, "Cannot prep FFI arguments!");
      return -1;
    }

    syslog(LOG_INFO,"Finish soft kernel %s init\n",sk_name);
    return 0;
  }

  XCL_DRIVER_DLLESPEC
  void
  skd::run() {
    int32_t kernel_return = 0;
    int ret = 0;
    void* ffi_arg_values[num_args];
    // Buffer Objects
    int boHandles[num_args];
    void* bos[num_args];
    uint64_t boSize[num_args];
    std::vector<int> bo_list;

    while (1) {
      ret = waitNextCmd();

      if (ret) {
	/* We are told to exit the soft kernel loop */
	syslog(LOG_INFO, "Exit soft kernel %s\n", sk_name);
	break;
      }

      syslog(LOG_DEBUG, "Got new kernel command!\n");

      /* Reg file indicates the kernel should not be running. */
      if (!(args_from_host[0] & 0x1))
	continue; //AP_START bit is not set; New Cmd is not available

      // FFI PS Kernel implementation
      // Map buffers used by kernel
      for(int i=0;i<num_args;i++) {
	if((args[i].index == xrt_core::xclbin::kernel_argument::no_index) && (args[i].hosttype.compare("xrtHandles*")==0)) {
	  ffi_arg_values[i] = &xrtHandle;
	} else if(args[i].type == xrt_core::xclbin::kernel_argument::argtype::global) {
	  uint64_t *buf_addr_ptr = (uint64_t*)(&args_from_host[(args[i].offset+PS_KERNEL_REG_OFFSET)/4]);
	  uint64_t buf_addr = reinterpret_cast<uint64_t>(*buf_addr_ptr);
	  uint64_t *buf_size_ptr = (uint64_t*)(&args_from_host[(args[i].offset+PS_KERNEL_REG_OFFSET)/4+2]);
	  uint64_t buf_size = reinterpret_cast<uint64_t>(*buf_size_ptr);
	  boSize[i] = buf_size;

	  boHandles[i] = xclGetHostBO(devHdl,buf_addr,buf_size);
	  bos[i] = xclMapBO(devHdl,boHandles[i],true);
	  ffi_arg_values[i] = &bos[i];
	  bo_list.emplace_back(i);
	} else {
	  ffi_arg_values[i] = &args_from_host[(args[i].offset+PS_KERNEL_REG_OFFSET)/4];
	}
      }

      ffi_call(&cif,FFI_FN(kernel), &kernel_return, ffi_arg_values);
      args_from_host[return_offset] = (uint32_t)kernel_return;

      // Unmap Buffers
      for(auto i:bo_list) {
	munmap(bos[i],boSize[i]);
	xclFreeBO(devHdl,boHandles[i]);
      }
      bo_list.clear();

    }

  }

  skd::~skd() {
    // Call soft kernel fini if available
    kernel_fini_t kernel_fini;
    std::string finiExtension = "_fini";
    char sk_fini[PS_KERNEL_NAME_LENGTH+finiExtension.size()];
    int ret = 0;

    // Check if SCU is still in running state
    // If it is, that means it has crashed
    if((args_from_host[0] & 0x1) == 1) {
      report_crash();  // Function to report crash to kernel - not implemented yet in kernel space
    }
    // Unmap command BO
    if(cmd_boh >= 0) {
      xclBOProperties prop;
      if (xclGetBOProperties(devHdl, cmd_boh, &prop)) {
      }
      ret = munmap(args_from_host,prop.size);
      if (ret) {
	syslog(LOG_ERR, "Cannot munmap BO %d, at %p\n", cmd_boh, &args_from_host);
      }
    }

    snprintf(sk_fini,PS_KERNEL_NAME_LENGTH+finiExtension.size(),"%s%s",sk_name,finiExtension.c_str());
    kernel_fini = (kernel_fini_t)dlsym(sk_handle, sk_fini);
    if (!kernel_fini) {
      syslog(LOG_INFO, "kernel fini function %s not found\n", sk_fini);
    } else {
      ret = kernel_fini(xrtHandle);
    }

    dlclose(sk_handle);
    ret = deleteSoftKernelFile();
    if (ret) {
      syslog(LOG_ERR, "Cannot remove soft kernel file %s\n", sk_path);
    }
    xclClose(devHdl);
    xclClose(parent_devHdl);
    report_fini();
  }

  int skd::createSoftKernel(int *boh) {
    int ret = 0;
    ret = xclSKCreate(devHdl, boh, cu_idx);
    return ret;
  }
  int skd::waitNextCmd() {
    return xclSKReport(devHdl, cu_idx, XRT_SCU_STATE_DONE);
  }

  /*
   * This function create a soft kernel file.
   */
  int skd::createSoftKernelFile(xclDeviceHandle handle, int bohdl)
  {
    FILE *fptr = NULL;
    void *buf = NULL;
    char path[XRT_MAX_PATH_LENGTH];
    int len, i;

    xclBOProperties prop;
    if (xclGetBOProperties(handle, bohdl, &prop)) {
      syslog(LOG_ERR, "Cannot get SK .so BO info.\n");
      return -1;
    }

    buf = xclMapBO(handle, bohdl, false);
    if (!buf) {
      syslog(LOG_ERR, "Cannot map softkernel BO.\n");
      return -1;
    }

    snprintf(path, XRT_MAX_PATH_LENGTH, "%s", SOFT_KERNEL_FILE_PATH);

    /* If not exist, create the path one by one. */
    std::filesystem::create_directories(path);

    fptr = fopen(sk_path, "w+b");
    if (fptr == NULL) {
      syslog(LOG_ERR, "Cannot create file: %s\n", sk_path);
      munmap(buf, prop.size);
      return -1;
    }

    /* copy the soft kernel to file */
    if (fwrite(buf, prop.size, 1, fptr) != 1) {
      syslog(LOG_ERR, "Fail to write to file %s.\n", sk_path);
      fclose(fptr);
      munmap(buf, prop.size);
      return -1;
    }
    syslog(LOG_INFO,"File created at %s\n", sk_path);

    fclose(fptr);
    munmap(buf, prop.size);

    return 0;
  }

  /*
   * This function delete the soft kernel file.
   */
  int skd::deleteSoftKernelFile()
  {
    return(remove(sk_path));
  }

  /* Convert argument to ffi_type */
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
    xclSKReport(devHdl,cu_idx,XRT_SCU_STATE_READY);
  }

  void skd::report_fini() {
    xclSKReport(devHdl,cu_idx,XRT_SCU_STATE_FINI);
  }

  void skd::report_crash() {
    xclSKReport(devHdl,cu_idx,XRT_SCU_STATE_CRASH);
  }
}
