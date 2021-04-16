.. _opencl_extension.rst:

Xilinx OpenCL extension
***********************

Please follow the general OpenCL guidelines to compile host applications that uses XRT OpenCL API with Xilinx OpenCL extensions. 
Xilinx OpenCL extension doesn't require additional compiler features. Normally C99 or C++11 would be good.

All the OpenCL extensions are described in the file ``src/include/1_2/CL/cl_ext_xilinx.h``


OpenCL Buffer extension by ``CL_MEM_EXT_PTR_XILINX``
=====================================================

XRT OpenCL implementation provides a mechanism using a structure ``cl_mem_ext_ptr_t`` to specify the special buffer and/or buffer location on the device. Ensure to use ``CL_MEM_EXT_PTR_XILINX`` flag when using this mechanism. Some usecases are as below:

Specify a special buffer such as P2P buffer, Host only buffer, etc.
-------------------------------------------------------------------

An example of a P2P Buffer specification

.. code:: c++

    cl_mem_ext_ptr_t p2p_buf_ext = {0};
    p2p_buf_ext.flags = XCL_MEM_EXT_P2P_BUFFER;

    cl_mem p2p_buf = clCreateBuffer(context, CL_MEM_READ_ONLY |CL_MEM_EXT_PTR_XILINX, buffersize, &p2p_buf_ext, &err);

An example of a host only buffer specification

.. code:: c++

      cl_mem_ext_ptr_t host_buffer_ext = {0};
      host_buffer_ext.flags = XCL_MEM_EXT_HOST_ONLY;

      cl::Buffer host_buffer (context,CL_MEM_READ_ONLY |CL_MEM_EXT_PTR_XILINX, size, &host_buffer_ext);

Specify regular buffer location on the device memory banks
----------------------------------------------------------

Optionally, for regular buffer, the buffer location can be specified using ``CL_MEM_EXT_PTR_XILINX`` and ``cl_mem_ext_ptr_t``. These can be done by any of these three methods

     1. Specify a Buffer location by DDR bank name (legacy)
     2. Specify a Buffer location by memory bank index
     3. Specify a Buffer location by kernel name and argument index


1. Specify a Buffer location by DDR bank name (legacy)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Here is an example of specifying explicit name of the bank, this works for only DDR banks. The supported flags are ``XCL_MEM_DDR_BANK0``, ``XCL_MEM_DDR_BANK1``, ``XCL_MEM_DDR_BANK2``, and ``XCL_MEM_DDR_BANK3``.

.. code:: c++

      cl_mem_ext_ptr_t ext = {0};
      ext.banks = XCL_MEM_DDR_BANK0;
      cl_int error;
      clCreateBuffer(context,CL_MEM_EXT_PTR_XILINX,size,&ext,&error);


- **Note**: Explicit bank specification is not required in most of the host code development. The XRT can obtain the bank location for the buffer if the buffer is used for setting the kernel arguments right after the buffer creation, i.e. before any enqueue operation on the buffer.


2. Specify a Buffer location by memory bank index
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This approach works for all types of memory banks as it accept index of the memory. The memory index can be obtained by inspecting .xclbin.info file or by xbutil query.

.. code:: c++

    // Check the memory index from xclbin.info file  
    ext.banks = 2 | XCL_MEM_TOPOLOGY;
    cl_int error;
    clCreateBuffer(context,CL_MEM_EXT_PTR_XILINX,size,&ext,&error)

- **Note**: Explicit bank specification is not required in most of the host code development. The XRT can obtain the bank location for the buffer if the buffer is used for setting the kernel arguments right after the buffer creation, i.e. before any enqueue operation on the buffer.


3. Specify a Buffer location by kernel name and argument index
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In relative specification style, the memory bank is detected from the kernel arguments, for this purpose the kernel handle and argument index is used as below

.. code:: c++

       ext.argidx = idx;
       ext.kernel = kernel;
       cl_int error;
       clCreateBuffer(context,CL_MEM_EXT_PTR_XILINX,size,&ext,&error);



- **Note**: This style of relative bank specification is not required in most of the host code development. The XRT can obtain the bank location for the buffer if the buffer is used for setting the kernel arguments right after the buffer creation, i.e. before any enqueue operation on the buffer.


DMA-BUF APIs
============

For some use-cases, for example p2p, multiprocess it may required to use buffer sharing. The XRT provides a couple of related APIs for import/export FD from the OpenCL buffer object.

   - ``xclGetMemObjectFd`` : To obtain FD from OpenCL memory object
   - ``xclGetMemObjectFromFd``: To obtain OpenCL memory object from FD

The example of API usage in the p2p context can be found in OpenCL example code in P2P documentation :ref:`p2p.rst`


Stream or QDMA Platform Related APIs
====================================

For description of Stream or QDMA related APIs are described in OpenCL Streaming documentation :ref:`streaming_data_support.rst`



Miscellaneous other APIs and Parameter extension
================================================

API to get Compute Units Information
------------------------------------

The API ``xclGetComputeUnitInfo`` is used to get information of Compute Unit. The API should be used together with specific flags to obtain the related information

   - ``XCL_COMPUTE_UNIT_NAME``
   - ``XCL_COMPUTE_UNIT_INDEX``
   - ``XCL_COMPUTE_UNIT_BASE_ADDRESS``
   - ``XCL_COMPUTE_UNIT_CONNECTIONS``

Example to get CU index and CU base address

.. code:: c++

      cl_uint cuidx;  // retrieve index of first cu in kernel
      xclGetComputeUnitInfo(kernel,0,XCL_COMPUTE_UNIT_INDEX,sizeof(cuidx),&cuidx,nullptr);

      size_t cuaddr;
      xclGetComputeUnitInfo(kernel,0,XCL_COMPUTE_UNIT_BASE_ADDRESS,sizeof(cuaddr),&cuaddr,nullptr);


Parameter extension of the API ``clGetKernelInfo``
--------------------------------------------------

These XRT specific parameters are provided for ``cl_kernel_info`` to be used with API ``clGetKernelInfo``.

  - ``CL_KERNEL_COMPUTE_UNIT_COUNT``: Can be used to get the number of CUs from the kernel handle/object
  - ``CL_KERNEL_INSTANCE_BASE_ADDRESS``: The base address of this kernel object

The below example is showing to get the number of Compute Unit information from the kernel object

.. code:: c++

      cl_uint numcus = 0;
      clGetKernelInfo(kernel,CL_KERNEL_COMPUTE_UNIT_COUNT,sizeof(cl_uint),&numcus,nullptr);


Parameter extension of the API ``clGetKernelArgInfo``
-----------------------------------------------------

This XRT specific parameter is provided for ``cl_kernel_arg_info`` to be used with API ``clGetKernelArgInfo``.

 - ``CL_KERNAL_ARG_OFFSET``: To get the argument offset for a specific argument.

Example shows below to get the offset for the argument 2 for the kernel.

.. code:: c++

      size_t foo_offset = 0;
      clGetKernelArgInfo(kernel, 2, CL_KERNEL_ARG_OFFSET, sizeof(foo_offset), &foo_offset, nullptr);


Parameter extension of the API ``clGetMemObjectInfo``
-----------------------------------------------------

This XRT specific parameter is provided for ``cl_mem_info`` to be used with API ``clGetMemObjectInfo``.

 - ``CL_MEM_BANK``: Memory bank index associated with the OpenCL Buffer

Example shows below to get the offset for the argument 2 for the kernel.

.. code:: c++

      int mem_bank_index = 0;
      clGetMemObjectInfo(buf, CL_MEM_BANK, sizeof(int), &mem_bank_index, nullptr);


Parameter extension of the API ``clGetDeviceInfo``
--------------------------------------------------

This XRT specific parameter is provided for ``cl_device_info`` to be used with API ``clGetDeviceInfo``.

  - ``CL_DEVICE_PCIE_BDF``: To obtain the Bus/Device/Function information of the Pcie based Device

Example shows below to get PCie BDF information from the OpenCL device

.. code:: c++

      char[20] bdf;
      clGetDeviceInfo(device, CL_DEVICE_PCIE_BDF, sizeof(bdf), &bdf, nullptr);
