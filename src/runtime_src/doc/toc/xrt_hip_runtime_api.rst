.. _xrt_hip_runtime_api.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
   comment:: Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.


XRT HIP Runtime APIs
====================

Starting from 2026.1 release, XRT natively supports HIP runtime APIs.

To use the native HIP APIs with XRT, the host application must link with the **xrt_hip** library.

Example g++ command

.. code-block:: shell

    g++ -g -std=c++17 -I/opt/rocm/include -D__HIP_PLATFORM_AMD__ -L$XILINX_XRT/lib -o host.exe host.cpp -lxrt_hip -pthread


.. list-table:: The following HIP APIs are natively supported by XRT
   :widths: 100
   :header-rows: 1

   * - HIP API
   * - ``hipDestroyExternalMemory``
   * - ``hipDeviceGet``
   * - ``hipDeviceGetAttribute``
   * - ``hipDeviceGetName``
   * - ``hipDeviceGetUuid``
   * - ``hipDrvGetErrorName``
   * - ``hipDrvGetErrorString``
   * - ``hipEventCreate``
   * - ``hipEventDestroy``
   * - ``hipEventElapsedTime``
   * - ``hipEventQuery``
   * - ``hipEventRecord``
   * - ``hipEventSynchronize``
   * - ``hipExtGetLastError``
   * - ``hipExternalMemoryGetMappedBuffer``
   * - ``hipFree``
   * - ``hipFuncSetAttribute``
   * - ``hipGetDeviceCount``
   * - ``hipGetDeviceProperties``
   * - ``hipGetDevicePropertiesR0600``
   * - ``hipGetErrorName``
   * - ``hipGetErrorString``
   * - ``hipGetLastError``
   * - ``hipGraphAddEmptyNode``
   * - ``hipGraphAddEventRecordNode``
   * - ``hipGraphAddEventWaitNode``
   * - ``hipGraphAddKernelNode``
   * - ``hipGraphAddMemcpyNode1D``
   * - ``hipGraphAddMemsetNode``
   * - ``hipGraphCreate``
   * - ``hipGraphDestroy``
   * - ``hipGraphExecDestroy``
   * - ``hipGraphInstantiate``
   * - ``hipGraphLaunch``
   * - ``hipHostFree``
   * - ``hipHostGetDevicePointer``
   * - ``hipHostMalloc``
   * - ``hipHostRegister``
   * - ``hipHostUnregister``
   * - ``hipImportExternalMemory``
   * - ``hipInit``
   * - ``hipIpcCloseMemHandle``
   * - ``hipIpcGetMemHandle``
   * - ``hipIpcOpenMemHandle``
   * - ``hipKernelNameRef``
   * - ``hipMalloc``
   * - ``hipMemcpy``
   * - ``hipMemcpyAsync``
   * - ``hipMemcpyHtoDAsync``
   * - ``hipMemPrefetchAsync``
   * - ``hipMemset``
   * - ``hipMemsetAsync``
   * - ``hipMemsetD16Async``
   * - ``hipMemsetD32Async``
   * - ``hipMemsetD8Async``
   * - ``hipModuleGetFunction``
   * - ``hipModuleLaunchKernel``
   * - ``hipModuleLoad``
   * - ``hipModuleLoadData``
   * - ``hipModuleLoadDataEx``
   * - ``hipModuleUnload``
   * - ``hipPeekAtLastError``
   * - ``hipSetDevice``
   * - ``hipStreamCreateWithFlags``
   * - ``hipStreamDestroy``
   * - ``hipStreamSynchronize``
   * - ``hipStreamWaitEvent``

For detailed description of the above HIP APIs please see `Using HIP runtime API <https://rocm.docs.amd.com/projects/HIP/en/latest/how-to/hip_runtime_api.html>`_.
