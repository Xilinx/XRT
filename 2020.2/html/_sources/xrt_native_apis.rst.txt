.. _xrt_native_apis.rst:

XRT Native APIs
===============

From 2020.2 release XRT provides a new XRT API set in C, C++, and Python flavor. This document introduces the usability of C and C++ APIs.

To use the native XRT APIs, the host application must link with the **xrt_coreutil** library.

Example g++ command

.. code-block:: shell

    g++ -g -std=c++14 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o host.exe host.cpp -lxrt_coreutil -pthread


The core data structures in C and C++ are as below

+---------------+---------------+-------------------+
|               |   C++ Class   |  C Type (Handle)  |
+===============+===============+===================+
|   Device      | xrt::device   |  xrtDeviceHandle  |
+---------------+---------------+-------------------+
|   XCLBIN      | xrt::xclbin   |  xrtXclbinHandle  |
+---------------+---------------+-------------------+
|   Buffer      | xrt::bo       |  xrtBufferHandle  |
+---------------+---------------+-------------------+
|   Kernel      | xrt::kernel   |  xrtKernelHandle  |
+---------------+---------------+-------------------+
|   Run         | xrt::run      |  xrtRunHandle     |
+---------------+---------------+-------------------+
|   Graph       | TBD           |  xrtGraphHandle   |
+---------------+---------------+-------------------+

All the core data structures are defined inside in the header files at ``$XILINX_XRT/include/experimental/`` directory. In the user host code, it is sufficient to include ``"experimental/xrt_kernel.h"`` and ``"experimental/xrt_aie.h"`` (when using Graph APIs) to access all the APIs related to these data structure.

.. code:: c
      :number-lines: 5
           
           #include "experimental/xrt_kernel.h"
           #include "experimental/xrt_aie.h"


The common host code flow using the above data structures is as below
   
      - Open Xilinx **Device** and Load the **XCLBIN**
      - Set up the **Buffers** that are used to transfer the data between the host and the device
      - Use the Buffer APIs for the data transfer between host and device (before and after the kernel execution).
      - Use **Kernel** and **Run** handle/objects to offload and manage the compute-intensive tasks running on FPGA. 
       
      
Below we will walk through the common API usage to accomplish the above tasks. 

Device and XCLBIN
-----------------

Device and XCLBIN class provide fundamental infrastructure-related interfaces. The primary objective of the device and XCLBIN related APIs are
 
    - Open a Device
    - Load compiled kernel binary (or XCLBIN) onto the device 


Example C API based code  

.. code:: c
      :number-lines: 10
           
           xrtDeviceHandle device = xrtDeviceOpen(0);
       
           xrtXclbinHandle xclbin = xrtXclbinAllocFilename("kernel.xclbin");
       
           xrtDeviceLoadXclbinHandle(device,xclbin);
           ..............
           ..............
           xrtDeviceClose(device);

       

The above code block shows
      
      - Opening the device (enumerated as 0) and get device handle ``xrtDeviceHandle`` (line 10)
          
          - Device indices are enumerated as 0,1,2 and can be observed by ``xbutil scan``
          
          .. code::
               
               >>xbutil scan
               INFO: Found total 2 card(s), 2 are usable
               .............
               [0] 0000:b3:00.1 xilinx_u250_gen3x16_base_1 user(inst=129)
               [1] 0000:65:00.1 xilinx_u50_gen3x16_base_1 user(inst=128)

      - Opening the XCLBIN from the filename and get an XCLBIN handle ``xrtXclbinHandle`` (line 12)
      - Loading the XCLBIN onto the Device by using the XCLBIN handle by API ``xrtDeviceLoadXclbinHandle`` (line 14)
      - Closing the device handle at the end of the application (line 19)
      

**C++**: The equivalent C++ API based code

.. code:: c++
      :number-lines: 10
           
           unsigned int dev_index = 0;
           auto device = xrt::device(dev_index);
           auto xclbin_uuid = device.load_xclbin("kernel.xclbin");
       
The above code block shows

    - The ``xrt::device`` class's constructor is used to open the device
    - The member function ``xrt::device::load_xclbin`` is used to load the XCLBIN from the filename. 
    - The member function ``xrt::device::load_xclbin`` returns the XCLBIN UUID, which is required to open the kernel (refer the Kernel Section). 


Buffers
-------

Buffers are primarily used to transfer the data between the host and the device. The Buffer related APIs are discussed in the following three subsections

       1. Buffer allocation and deallocation
       2. Data transfer using Buffers
       3. Miscellaneous other Buffer APIs



1. Buffer allocation and deallocation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XRT APIs provides API for
   
      - ``xrtBOAlloc``: Allocates a buffer object 4K aligned, the API must be called with appropriate flags. 
      - ``xrtBOAllocUserPtr``: Allocates a buffer object using pointer provided by the user. The user pointer must be aligned to 4K boundary. 
      - ``xrtBOFree``: Deallocates the allocated buffer. 

.. code:: c
      :number-lines: 15
           
           xrtMemoryGroup bank_grp_idx_0 = xrtKernelArgGroupId(kernel, 0);
           xrtMemoryGroup bank_grp_idx_1 = xrtKernelArgGroupId(kernel, 1);

           xrtBufferHandle input_buffer = xrtBOAlloc(device, buffer_size_in_bytes, XRT_BO_FLAGS_NONE, bank_grp_idx_0);
           xrtBufferHandle output_buffer = xrtBOAlloc(device, buffer_size_in_bytes, XRT_BO_FLAGS_NONE, bank_grp_idx_1);

           ....
           ....
           xrtBOFree(input_buffer);
           xrtBOFree(output_buffer);

   
The above code block shows 

    - Buffer allocation API ``xrtBOAlloc`` at lines 15,16
    - Buffer deallocation API ``xrtBOFree`` at lines 23,24 
    
The various arguments of the API ``xrtBOAlloc`` are

    - Argument 1: The device on which the buffer should be allocated 
    - Argument 2: The size (in bytes) of the buffer
    - Argument 3: ``xrtBufferFlags``: Used to specify the buffer type, most commonly used types are
       
        - ``XRT_BO_FLAGS_NONE``: Regular Buffer
        - ``XRT_BO_FLAGS_DEV_ONLY``: Device only Buffer (meant to be used only by the kernel). 
        - ``XRT_BO_FLAGS_HOST_ONLY``: Host Only Buffer (buffers reside in the host memory directly transferred to/from the kernel)
        - ``XRT_BO_FLAGS_P2P``: P2P Buffer, buffer for NVMe transfer
        - ``XRT_BO_FLAGS_CACHEABLE``: Cacheable buffer can be used when host CPU frequently accessing the buffer (applicable for embedded platform). 
        
    - Argument 4:  ``xrtMemoryGroup``: Enumerated Memory Bank to specify the location on the device where the buffer should be allocated. The ``xrtMemoryGroup`` is obtained by the API ``xrtKernelArgGroupId`` as shown in line 15 (for more details of this API refer to the Kernel section).   
    

**C++**: The equivalent C++ API based code

.. code:: c++
      :number-lines: 15
           
           auto bank_grp_idx_0 = kernel.group_id(0);
           auto bank_grp_idx_1 = kernel.group_id(1);
    
           auto input_buffer = xrt::bo(device, buffer_size_in_bytes,bank_grp_idx_0);
           auto output_buffer = xrt::bo(device, buffer_size_in_bytes, bank_grp_idx_1);

In the above code ``xrt::bo`` buffer objects are created using the class's constructor. Note the buffer flag is not used as constructor by default created regular buffer. Nonetheless, the available buffer flags for ``xrt::bo`` are described using ``enum class`` argument with the following enumerator values

        - ``xrt::bo::flags::normal``: Default, Regular Buffer
        - ``xrt::bo::flags::device_only``: Device only Buffer (meant to be used only by the kernel).
        - ``xrt::bo::flags::host_only``: Host Only Buffer (buffer resides in the host memory directly transferred to/from the kernel)
        - ``xrt::bo::flags::p2p``: P2P Buffer, buffer for NVMe transfer  
        - ``xrt::bo::flags::cacheable``: Cacheable buffer can be used when host CPU frequently accessing the buffer (applicable for embedded platform).



2. Data transfer using Buffers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XRT Buffer API library provides a rich set of APIs helping the data transfers between the host and the device, between the buffers, etc. We will discuss the following data transfer style

        I. Data transfer between host and device by Buffer read/write API
        II. Data transfer between host and device by Buffer map API
        III. Data transfer between buffers by copy API


I. Data transfer between host and device by Buffer read/write API
*****************************************************************

To transfer the data from the host to the device, the user first needs to update the host-side buffer backing pointer followed by a DMA transfer to the device. 

The following C APIs are used for the above tasks

    1. ``xrtBOWrite``  
    2. ``xrtBOSync`` with flag ``XCL_BO_SYNC_BO_TO_DEVICE``
    
In C++, ``xrt::bo`` class has following member functions for the same functionality

    1. ``xrt::bo::write``
    2. ``xrt::bo::sync`` with flag ``XCL_BO_SYNC_BO_TO_DEVICE``

To transfer the data from the device to the host, the steps are reverse, the user first needs to do a DMA transfer from the device followed by the reading data from the host-side buffer backing pointer. 

The following C APIs are used for the above tasks

     1. ``xrtBOSync`` with flag ``XCL_BO_SYNC_BO_FROM_DEVICE``
     2. ``xrtBORead``

In C++ the corresponding ``xrt::bo`` class's member functions are

    1. ``xrt::bo::sync`` with flag ``XCL_BO_SYNC_BO_FROM_DEVICE``
    2. ``xrt::bo::read``


Code example of transferring data from the host to the device

.. code:: c
      :number-lines: 20
           
           xrtBufferHandle input_buffer = xrtBOAlloc(device, buffer_size_in_bytes, XRT_BO_FLAGS_NONE, bank_grp_idx_0);

           // Prepare the input data
           int buff_data[data_size];
           for (int i=0; i<data_size; ++i) {
               buff_data[i] = i;
           }
    
           xrtBOWrite(input_buffer,buff_data,data_size*sizeof(int),0);
           xrtSyncBO(input_buffer,XCL_BO_SYNC_BO_TO_DEVICE, data_size*sizeof(int),0);
    

**C++**: The equivalent C++ API based code


.. code:: c++
      :number-lines: 20    
           
           auto input_buffer = xrt::bo(device, buffer_size_in_bytes, bank_grp_idx_0);
           // Prepare the input data
           int buff_data[data_size];
           for (auto i=0; i<data_size; ++i) {
               buff_data[i] = i;
           }
    
           input_buffer.write(buff_data);
           input_buffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);

Note the C++ ``xrt::bo::sync``, ``xrt::bo::write``, ``xrt::bo::read`` etc has overloaded version that can be used for paritial buffer sync/read/write by specifying the size and the offset. For the above code example, the full buffer size and 0 offset are used as default arguments. 


II. Data transfer between host and device by Buffer map API
***********************************************************

The API ``xrtBOMap`` (C++: ``xrt::bo::map``) allows mapping the host-side buffer backing pointer to a user pointer. The host code can subsequently exercise the user pointer for the data reads and writes. However, after writing to the mapped pointer (or before reading from the mapped pointer) the API ``xrtBOSync`` (C++: ``xrt::bo::sync``) should be used with direction flag for the DMA operation. 

Code example of transferring data from the host to the device by this approach

.. code:: c
      :number-lines: 20
           
           xrtBufferHandle input_buffer = xrtBOAlloc(device, buffer_size_in_bytes, XRT_BO_FLAGS_NONE, bank_grp_idx_0);
           int* input_buffer_mapped = (int*)xrtBOMap(input_buffer);

           for (int i=0;i<data_size;++i) {
               input_buffer_mappped[i] = i;
           }

           xrtBOSync(input_buffer, XCL_BO_SYNC_BO_TO_DEVICE, buffer_size_in_bytes, 0);
    
**C++**: The equivalent C++ API based code

.. code:: c++
      :number-lines: 20
           
           auto input_buffer = xrt::bo(device, buffer_size_in_bytes, bank_grp_idx_0);
           auto input_buffer_mapped = input_buffer.map<int*>();

           for (auto i=0;i<data_size;++i) {
               input_buffer_mapped[i] = i;
           }

           input_buffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);


III. Data transfer between the buffers by copy API
**************************************************

XRT provides ``xrtBOCopy`` (C++: ``xrt::bo::copy``) API for deep copy between the two buffer objects if the platform supports a deep-copy (for detail refer M2M feature described in :ref:`m2m.rst`). If deep copy is not supported by the platform the data transfer happens by shallow copy (the data transfer happens via host). 

API Example in C, all arguments are self-explanatory

.. code:: c
      :number-lines: 25
           
           size_t dst_buffer_offset = 0;
           size_t src_buffer_offset = 0;
           xrtBOCopy(dst_buffer, src_buffer, size_of_copy, dst_buffer_offset, src_buffer_offset);


**C++**: The equivalent C++ API based code

.. code:: c++
      :number-lines: 25
           
           
           dst_buffer.copy(src_buffer, copy_size_in_bytes);

The API ``xrt::bo::copy`` also has overloaded version to provide a different offset than 0 for both the source and the destination buffer. 

3. Miscellaneous other Buffer APIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section describes a few other specific use-cases using buffers. 

DMA-BUF API
***********

XRT provides Buffer export and import APIs primarily used for sharing buffer across devices (P2P application) and processes. 

   - ``xrtBOExport`` (C++: ``xrt::bo::export_buffer``): Export the buffer to an exported buffer handle
   - ``xrtBOImport`` (C++: ``xrt::bo`` constructor) : Allocate a BO imported from exported buffer handle


Consider the situation of exporting buffer from device 1 to device 2. 

.. code:: c
      :number-lines: 18
           
           xclBufferExportHandle buffer_exported = xrtBOExport(buffer_device_1);
           xrtBufferHandle buffer_device_2 = xrtBOImport(device_2, buffer_exported);

In the above example

       - The buffer buffer_device_1 is a buffer allocated on device 1
       - buffer_device_1 is exported to an ``xclBufferExportHandle`` by API ``xrtBOExport``
       - The exported buffer of type ``xclBufferExportHandle`` is imported to device 2 by API ``xrtBOImport``


**C++**: The equivalent C++ API based code

.. code:: c++
      :number-lines: 18
      
           auto buffer_exported = buffer_device_1.export_buffer();
           auto buffer_device_2 = xrt::bo(device_2, buffer_exported);

In the above example

       - The buffer buffer_device_1 is a buffer allocated on device 1
       - buffer_device_1 is exported by the member function ``xrt::bo::export_buffer``
       - The new buffer buffer_device_2 is imported for device_2 by the constructor ``xrt::bo``


 
Sub-buffer support
******************

The API ``xrtBOSubAlloc`` (C++: supported by an ``xrt::bo`` class constructor) allocates a sub-buffer from a parent buffer by specifying a start offset and the size. 

In the example below a sub-buffer is created from a parent buffer of size 4 bytes staring from its offset 0 

.. code:: c
      :number-lines: 18
           
           xrtBufferHandle parent_buffer; 
           xrtBufferHandle sub_buffer; 
     
           size_t sub_buffer_size = 4; 
           size_t sub_buffer_offset = 0; 
     
           sub_buffer = xrtBOSubAlloc(parent_buffer, sub_buffer_size, sub_buffer_offset);


**C++**: The equivalent C++ API based code

In C++ a sub-buffer is created by using the xrt::bo class's constructor using the parent buffer, size, and offset as parameters. 

.. code:: c++ 
      :number-lines: 18
           
           size_t sub_buffer_size = 4; 
           size_t sub_buffer_offset = 0; 

           auto sub_buffer = xrt::bo(parent_buffer, sub_buffer_size, sub_buffer_offset);


Buffer information
******************

XRT provides few other APIs to obtain information related to the buffer. 

   - ``xrtBOSize`` (C++: member function ``xrt::bo::size``): Size of the buffer
   - ``xrtBOAddr`` (C++: member function ``xrt::bo::address``) : Physical address of the buffer



Kernel and Run
--------------

The XRT kernel APIs support creating of kernel handle (or object in C++) from currently loaded xclbin.  The kernel handle is used to execute the kernel function on the hardware instance (Compute Unit or CU) of the kernel.  

A Run handle/object represents an execution of the kernel. Upon finishing the kernel execution, the Run handle/object can be reused to invoke the same kernel function if desired. 

The following topics are discussed below

       - Obtaining kernel handle/object from XCLBIN
       - Getting the bank group index of a kernel argument
       - Reading and write CU mapped registers
       - Execution of kernel and dealing with the associated run
       - Other kernel execution related API
       

Obtaining kernel handle/object from XCLBIN
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The kernel handle (or object) is created from the device, XCLBIN UUID and the kernel name. 

.. code:: c
      :number-lines: 35
           
           xuid_t xclbin_uuid;
           xrtXclbinGetUUID(xclbin,xclbin_uuid);

           xrtKernelHandle kernel = xrtPLKernelOpen(device, xclbin_uuid, "kernel_name");
           ....
           ....
           xrtKernelClose(kernel);


In the above code example
 
      - The UUID of the XCLBIN is retrieved by the API ``xrtXclbinGetUUID`` 
      - The kernel is created by the API ``xrtPLKernelOpen``
      - The kernel is closed by the API ``xrtKernelClose``

**Note**: For the kernel with more than 1 CU, a kernel handle (or object) should represent all the CUs having identical interface connectivity. If all the CUs of the kernel are not having identical connectivity, the specific CU name(s) should be used to obtain a kernel handle (or object) to represent the subset of CUs with identical connectivity. Otherwise XRT will do this selection internally to select a group of CUs and discard the rest of the CUs (discarded CUs are not used during the execution of a kernel).  

As an example, assume a kernel name is foo having 3 CUs foo_1, foo_2, foo_3. The CUs foo_1 and foo_2 are connected to DDR bank 0, but the CU foo_3 is connected to DDR bank 1. 

       - Opening kernel handle for foo_1 and foo_2 (as they have identical interface connection)
       
         .. code:: c
               :number-lines: 35
                  
                    cu_group_1 = xrtPLKernelOpen(device, xclbin_uuid, "foo:{foo_1,foo_2}");     
   
       - Opening kernel handle for foo_3
          
         .. code:: c
               :number-lines: 35
                  
                    cu_group_2 = xrtPLKernelOpen(device, xclbin_uuid, "foo:{foo_3}");     



**C++**: In C++, ``xrt::kernel`` object can be created from the constructor of ``xrt::kernel`` class. 

.. code:: c++
      :number-lines: 35
          
           auto xclbin_uuid = device.load_xclbin("kernel.xclbin");
           auto krnl = xrt::kernel(device, xclbin_uuid, name); 
      
Exclusive access of the kernel's CU
***********************************
  
The API ``xrtPLKernelOpen`` opens a kernel's CU in a shared mode so that the CU can be shared with the other processes. In some cases, it is required to open the CU in exclusive mode (for example, when it is required to read/write CU mapped register). Exclusive CU opening fails if the CU is already opened in either shared or exclusive access. 

.. code:: c
      :number-lines: 39
     
           xrtKernelHandle kernel = xrtPLKernelOpenExclusive(device, xclbin_uuid, "name");

**C++**: In C++, ``xrt::kernel`` constructor can be called with an additional ``enum class`` argument to access the kernel in exclusive mode. The enumerator values are: 

     - ``xrt::kernel::cu_access_mode::shared`` (default ``xrt::kernel`` constructor argument)
     - ``xrt::kernel::cu_access_mode::exclusive`` 

.. code:: c++
      :number-lines: 39
       
           auto krnl = xrt::kernel(device, xclbin_uuid, name, xrt::kernel::cu_access_mode::exclusive); 

   

Getting bank group index of the kernel argument
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We have seen in the Buffer creation section that it is required to provide the buffer location during the buffer creation. XRT provides an API ``xrtKernelArgGroupId`` (C++: ``xrt::kernel::group_id``) that returns the bank index (ID) of a specific argument of the kernel. This ID is used as the last argument of ``xclAllocBO`` (in C++ with ``xrt::bo`` constructor) API to create the buffer on the same memory bank. 


Let us review the example below where the buffer is allocated for the kernel's first (argument index 0) by using this API

.. code:: c
      :number-lines: 39
           
           xrtMemoryGroup idx_0 = xrtKernelArgGroupId(kernel, 0); // bank index of 0th argument
           xrtBufferHandle a = xrtBOAlloc(device, data_size*sizeof(int), XRT_BO_FLAGS_NONE, idx_0);


.. code:: c++
      :number-lines: 15
                       
           auto input_buffer = xrt::bo(device, buffer_size_in_bytes, kernel.group_id(0));



The API fails if the kernel bank index is ambiguous. For example, the kernel has multiple CU with different connectivity for that argument. In those cases, it is required to create a kernel object/handle with specific a CU (or group of CUs with identical connectivity). 


   
Reading and write CU mapped registers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To read and write from the AXI-Lite register space corresponding to a CU, the CU must be opened in exclusive mode (in shared mode, multiple processes can access the CU's address space, hence it is unsafe if they are trying to access/change registers at the same time leading to a potential race behavior). The required APIs for kernel register read and write are
  
    - ``xrtKernelReadRegister`` (C++: member function ``xrt::kernel::read_register``)
    - ``xrtKernelWriteRegiste`` (C++: member function ``xrt::kernel::write_register``)

.. code:: c
      :number-lines: 35
         
           int read_data; 
           int write_data = 7; 
              
           xrtKernelHandle kernel = xrtPLKernelOpenExclusive(device, xclbin_uuid, "foo:{foo_1}");
              
           xrtKernelReadRegister(kernel,READ_OFFSET,&read_data);
           xrtKernelWriteRegister(kernel,WRITE_OFFSET,write_data); 
              
           xrtKernelClose(kernel);


In the above code block

              - The CU named "foo_1" (name syntax: "kernel_name:{cu_name}") is opened exclusively.
              - The Register Read/Write operation is performed. 
              - Closed the kernel
              
**C++**: The equivalent C++ API example

.. code:: c
      :number-lines: 35
       
           int read_data; 
           int write_data = 7; 
              
           auto krnl = xrt::kernel(device, xclbin_uuid, "foo:{foo_1}", true); 

           read_data = kernel.read_register(READ_OFFSET);
           kernel.write_register(WRITE_OFFSET,write_data); 
              
              
Obtaining the argument offset
*****************************
              
The register read/write access APIs use the register offset as shown in the above examples. The user can get the register offset of a corresponding kernel argument from the ``v++`` generated ``.xclbin.info`` file and use with the register read/write APIs. 

.. code::
    
    --------------------------
    Instance:        foo_1
    Base Address: 0x1800000

    Argument:          a
    Register Offset:   0x10
    


However, XRT also provides APIs to obtain the register offset for CU arguments. In the below example C API ``xrtKernelArgOffset`` is used to obtain offset of third argument of the CU ``foo:foo_1``.  


.. code:: c
      :number-lines: 38

           // Assume foo has 3 arguments, a,b,c (arg 0, arg 1 and arg 2 respectively) 
           
           xrtKernelHandle kernel = xrtPLKernelOpenExclusive(device, xclbin_uuid, "foo:{foo_1}");
           uint32_t arg_c_offset = xrtKernelArgOffset(kernel, 2);
 

**C++**: The equivalent C++ API example

.. code:: c++
      :number-lines: 38

           // Assume foo has 3 arguments, a,b,c (arg 0, arg 1 and arg 2 respectively) 
           
           auto krnl = xrt::kernel(device, xclbin_uuid, "foo:{foo_1}", true); 
           auto offset = krnl.offset(2);

 
Executing the kernel
~~~~~~~~~~~~~~~~~~~~

Execution of the kernel is associated with a **Run** handle (or object). The kernel can be executed by the API ``xrtKernelRun`` (in C++ overloaded operator ``xrt::kernel::operator()``) that takes all the kernel arguments in order. The kernel execution API returns a run handle (or object) corresponding to the execution. 


.. code:: c
      :number-lines: 50
       
           // 1st kernel execution
           xrtRunHandle run = xrtKernelRun(kernel, buf_a, buf_b,  scalar_1); 
           xrtRunWait(run);
    
           // 2nd kernel execution with just changing 3rd argument
           xrtRunSetArg(run,2,scalar_2); // Arguments are specified starting from 0
           xrtRunStart(run);
           xrtRunWait(run);

           // Close the run handle
           xrtRunClose(run);

Note the following APIs regarding  the above example

   - The kernel is executed by ``xrtKernelRun`` API by specifying all its arguments to obtain a Run handle
   - The API ``xrtKernelRun`` is non-blocking. It returns as soon as it submits the job without waiting for the kernel's actual execution start.  
   - The host code uses ``xrtRunWait`` API to block the current thread and wait till the kernel execution is finished.       
   - After a run is finished, the same run handle can be reused to execute the kernel multiple times if desired. 
     
       - API ``xrtRunSetArg`` is used to set one or more arguments, in the example above only the last (3rd) argument is changed before the second execution
       - API ``xrtRunStart`` is used to execute the kernel using the run handle. 
   - API ``xrtRunClose`` is used to close the Run handle.  
 
   
**C++**: The equivalent C++ code

In C++ the ``xrt::kernel`` class provides **overloaded operator ()** to execute the kernel with a comma-separated list of arguments.  

.. code:: c++
      :number-lines: 50
   
           // 1st kernel execution
           auto run = kernel(buf_a, buf_b, scalar_1); 
           run.wait();
    
           // 2nd kernel execution with just changing 3rd argument
           run.set_arg(2,scalar_2); // Arguments are specified starting from 0 
           run.start();
           run.wait();

The above c++ code block is demonstrating 
  
  - The kernel execution using the ``xrt::kernel()`` operator with the list of arguments that returns a xrt::run object. This is an asynchronous API and returns after submitting the task.    
  - The member function ``xrt::run::wait`` is used to block the current thread until the current execution is finished. 
  - The member function ``xrt::run::set_arg`` is used to set one or more kernel argument(s) before the next execution. In the example above, only the last (3rd) argument is changed.  
  - The member function ``xrt::run::start`` is used to start the next kernel execution with new argument(s).   

Other kernel execution related APIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Run handle/object supports few other use-cases. 

**Obtaining the run handle/object before execution**: In the above example we have seen a run handle/object is obtained when the kernel is executed (kernel execution returns a run handle/object). However, a run handle can be obtained even before the kernel execution. The flow is as below

    - Open a Run handle (or object) by API ``xrtRunOpen`` (in C++ ``xrt::run`` constructor with a kernel argument). There is no kernel execution associated with this run handle/object yet
    - Set the kernel arguments associated for the next execution by ``xrtRunSetArg`` (in C++ member function ``xrt::run::set_arg``). 
    - Execute the kernel by ``xrtRunStart`` (in C++ member function ``xrt::run::start``).
    - Wait for the execution finish by ``xrtRunWait`` (C++: ``xrt::run::wait``). 

**Timeout while wait for kernel finish**: The API ``xrtRunWait`` blocks the current thread until the kernel execution finishes. However, a timeout supported API ``xrtRunWaitFor`` is also provided . The timeout number can be specified using a millisecond unit.

In C++, the timeout facility can be used by the same member function that takes a ``std::chrono::milliseconds`` to specify the timeout. 

**Asynchronous update of the kernel arguments**: The API ``xrtRunSetArg`` (C++: ``xrt::run::set_arg``) is synchronous to the kernel execution. This API can only be used when kernel is in the IDLE state and before the start of the next execution. An asynchronous version of this API (only for edge platform) ``xrtRunUpdateArg`` (in C++ member function ``xrt::run::update_arg``) is provided to change the kernel arguments asynchronous to the kernel execution. 

Graph
-----

In Versal ACAPs with AI Engines, the XRT Graph APIs can be used to dynamically load, monitor, and control the graphs executing on the AI Engine array. As of the 2020.2 release, XRT provides a set of C APIs for graph control. The C++ APIs are planned for a future release. Also, as of the 2020.2 release Graph APIs are only supported on the Edge platform.

A graph handle is of type ``xrtGraphHandle``. 

Graph Opening and Closing
~~~~~~~~~~~~~~~~~~~~~~~~~

The XRT graph APIs support the obtaining of graph handle from currently loaded xclbin. The required APIs for graph open and close are

         - ``xrtGraphOpen``: API provides the handle of the graph from the device, XCLBIN UUID, and the graph name. 
         - ``xrtGraphClose``: API to close the graph handle. 

.. code:: c
      :number-lines: 35
           
           xuid_t xclbin_uuid;
           xrtXclbinGetUUID(xclbin,xclbin_uuid);

           xrtGraphHandle graph = xrtGraphOpen(device, xclbin_uuid, "graph_name");
           ....
           ....
           xrtGraphClose(graph);


The graph handle obtained from ``xrtGraphOpen`` is used to execute the graph function on the AIE tiles.

Reset Functions
~~~~~~~~~~~~~~~

There are two reset functions are used:

   - API ``xrtAIEResetArray`` is used to reset the whole AIE array. 
   - API ``xrtGraphReset`` is used to reset a specified graph by disabling tiles and enabling tile reset. 


.. code:: c
      :number-lines: 45
           
           xrtDeviceHandle device_handle = xrtDeviceOpen(0);
           ...
           // AIE Array Reset
           xrtAIEResetArray(device_handle)
           
           xrtGraphHandle graph = xrtGraphOpen(device, xclbin_uuid, "graph_name");
           // Graph Reset
           xrtGraphReset(graphHandle);




Graph execution
~~~~~~~~~~~~~~~

XRT provides basic graph execution control APIs to initialize, run, wait, and terminate graphs for a specific number of iterations. Below we will review some of the common graph execution styles. 

Graph execution for a fixed number of iterations
************************************************

A graph can be executed for a fixed number of iterations followed by a "busy-wait" or a "time-out wait". 

**Busy Wait scheme**

The graph can be executed for a fixed number of iteration by ``xrtGraphRun`` API using an iteration argument. Subsequently, ``xrtGraphWait`` or ``xrtGraphEnd`` API should be used (with argument 0) to wait until graph execution is completed. 

Let's review the below example

- The graph is executed for 3 iterations by API ``xrtGraphRun`` with the number of iterations as an argument. 
- The API ``xrtGraphWait(graphHandle,0)`` is used to wait till the iteration is done. 

     - The API `xrtGraphWait` is used because the host code needs to execute the graph again. 
- The Graph is executed again for 5 iteration
- The API ``xrtGraphEnd(graphHandle,0)`` is used to wait till the iteration is done. 

    - After ``xrtGraphEnd`` the same graph should not be executed. 

.. code:: c
      :number-lines: 35
           
           // start from reset state
           xrtGraphReset(graphHandle);
           
           // run the graph for 3 iteration
           xrtGraphRun(graphHandle, 3);
           
           // Wait till the graph is done 
           xrtGraphWait(graphHandle,0);  // Use xrtGraphWait if you want to execute the graph again
           
           
           xrtGraphRun(graphHandle,5);
           xrtGraphEnd(graphHandle,0);  // Use xrtGraphEnd if you are done with the graph execution


**Timeout wait scheme**

As shown in the above example ``xrtGraphWait(graphHandle,0)`` performs a busy-wait and suspend the execution till the graph is not done. If desired a timeout version of the wait can be achieved by ``xrtGraphWaitDone`` which can be used to wait for some specified number of milliseconds, and if the graph is not done do something else in the meantime. An example is shown below

.. code:: c++
      :number-lines: 35
           
           // start from reset state
           xrtGraphReset(graphHandle);
           
           // run the graph for 100 iteration
           xrtGraphRun(graphHandle, 100);
           
            while (1) {
             auto rval  = xrtGraphWaitDone(graphHandle, 5); 
              std::cout << "Wait for graph done returns: " << rval << std::endl;
              if (rval == -ETIME)  {
                   std::cout << "Timeout, reenter......" << std::endl;
                   // Do something 
              }
              else  // Graph is done, quit the loop
                  break;
             }


Infinite Graph Execution
************************

The graph runs infinitely if ``xrtGraphRun`` is called with iteration argument -1. While a graph running infinitely the APIs ``xrtGraphWait``, ``xrtGraphSuspend`` and xrtGraphEnd can be used to suspend/end the graph operation after some number of AIE cycles. The API ``xrtGraphResume`` is used to execute the infinitely running graph again. 


.. code:: c
      :number-lines: 39
           
           // start from reset state
           xrtGraphReset(graphHandle);
           
           // run the graph infinitely
           xrtGraphRun(graphHandle, -1);
           
           xrtGraphWait(graphHandle,3000);  // Suspends the graph after 3000 AIE cycles from the previous start 
           
           
           xrtGraphResume(graphHandle); // Restart the suspended graph again to run forever
           
           xrtGraphSuspend(graphHandle); // Suspend the graph immediately
           
           xrtGraphResume(graphHandle); // Restart the suspended graph again to run forever
           
           xrtGraphEnd(graphHandle,5000);  // End the graph operation after 5000 AIE cycles from the previous start


In the example above

- The API ``xrtGraphRun(graphHandle, -1)`` is used to execute the graph infinitely
- The API ``xrtGraphWait(graphHandle,3000)`` suspends the graph after 3000 AIE cycles from the graph starts. 

       - If the graph was already run more than 3000 AIE cycles the graph is suspended immediately. 
- The API ``xrtGraphResume`` is used to restart the suspended graph
- The API ``xrtGraphSuspend`` is used to suspend the graph immediately
- The API ``xrtGraphEnd(graphHandle,5000)`` is  ending the graph after 5000 AIE cycles from the previous graph start. 
       
       - If the graph was already run more than 5000 AIE cycles the graph ends immediately.
       - Using ``xrtGraphEnd`` eliminates the capability of rerunning the Graph (without loading PDI and a graph reset again). 


Measuring AIE cycle consumed by the Graph
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The API ``xrtGraphTimeStamp`` can be used to determine AIE cycle consumed between a graph start and stop. 

Here in this example, the AIE cycle consumed by 3 iteration is calculated
 

.. code:: c++
      :number-lines: 35
           
           // start from reset state
           xrtGraphReset(graphHandle);
           
           uint64_t begin_t = xrtGraphTimeStamp(graphHandle);
           
           // run the graph for 3 iteration
           xrtGraphRun(graphHandle, 3);
           
           xrtGraphWait(graphHandle, 0); 
           
           uint64_t end_t = xrtGraphTimeStamp(graphHandle);
           
           std::cout<<"Number of AIE cycles consumed in the 3 iteration is: "<< end_t-begin_t; 
           

RTP (Runtime Parameter) control
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XRT provides the API to update and read the runtime parameters of the graph. 

- The API ``xrtGraphUpdateRTP`` to update the RTP 
- The API ``xrtGraphReadRTP`` to read the RTP. 

.. code:: c++
      :number-lines: 35

           ret = xrtGraphReset(graphHandle);
           if (ret) throw std::runtime_error("Unable to reset graph");

           ret = xrtGraphRun(graphHandle, 2);
           if (ret) throw std::runtime_error("Unable to run graph");

           float increment[1] = {1};
           const char *inVect = reinterpret_cast<const char *>(increment);
           xrtGraphUpdateRTP(graphHandle, "mm.mm0.in[2]", inVect, sizeof (float));
     
           // Do more things
           xrtGraphRun(graphHandle,16);
           xrtGraphWait(graphHandle,0);
     
           // Read RTP
           float increment_out[1] = {1};
           char *outVect = reinterpret_cast<char *>(increment_out);
           xrtGraphReadRTP(graphHandle, "mm.mm0.inout[0]", outVect, sizeof(float));
           std::cout<<"\n RTP value read<<increment_out[0]; 
 
In the above example, the API ``xrtGraphUpdateRTP`` and ``xrtGraphReadRTP`` are used to update and read the RTP values respectively. Note the API arguments 
   
      - The hierarchical name of the RTP port
      - Pointer to write or read the RTP variable
      - The size of the RTP value. 

DMA operation to and from Global Memory IO
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XRT provides API ``xrtAIESyncBO`` to synchronize the buffer contents between GMIO and AIE. The following code shows a sample example


.. code:: c++
      :number-lines: 35

           xrtDeviceHandle device_handle = xrtDeviceOpen(0);
       
           // Buffer from GM to AIE
           xrtBufferHandle in_bo_handle  = xrtBOAlloc(device_handle, SIZE * sizeof (float), 0, 0);
       
           // Buffer from AIE to GM
           xrtBufferHandle out_bo_handle  = xrtBOAlloc(device_handle, SIZE * sizeof (float), 0, 0);
       
           inp_bo_map = (float *)xrtBOMap(in_bo_handle);
           out_bo_map = (float *)xrtBOMap(out_bo_handle);

           // Prepare input data 
           std::copy(my_float_array,my_float_array+SIZE,inp_bo_map);


           xrtAIESyncBO(device_handle, in_bo_handle, "in_sink", XCL_BO_SYNC_BO_GMIO_TO_AIE, SIZE * sizeof(float),0); 

           xrtAIESyncBO(device_handle, out_bo_handle, "out_sink", XCL_BO_SYNC_BO_AIE_TO_GMIO, SIZE * sizeof(float), 0);
       
       
The above code shows

    - Input and output buffer (``in_bo_handle`` and ``out_bo_handle``) to the graph are created and mapped to the user space
    - The API ``xrtAIESyncBO`` is used for data transfer using the following arguments
    
          - Device and Buffer Handle
          - The name of the GMIO ports associated with the DMA transfer
          - The direction of the buffer transfer 
          
                   - GMIO to Graph: ``XCL_BO_SYNC_BO_GMIO_TO_AIE``
                   - Graph to GMIO: ``XCL_BO_SYNC_BO_AIE_TO_GMIO``
          - The size and the offset of the buffer
    
               
XRT Error API
-------------

In general, XRT APIs can encounter two types of errors:
 
       - Synchronous error: Error can be thrown by the API itself. These types of errors should be checked against all APIs (strongly recommended). 
       - Asynchronous error: Errors from the underneath driver, system, hardware, etc. 
       
XRT provides a couple of APIs to retrieve the asynchronous errors into the userspace host code. This helps to debug when something goes wrong.
 
       - ``xrtErrorGetLast`` - Gets the last error code and its timestamp of a given error class
       - ``xrtErrorGetString`` - Gets the description string of a given error code.

**NOTE**: The asynchronous error retrieving APIs are at an early stage of development and only supports AIE related asynchronous errors. Full support for all other asynchronous errors is planned in a future release. 

Example code

.. code:: c++
      :number-lines: 41

           rval = xrtGraphRun(graphHandle, runInteration);
           if (rval != 0) {                                                                   
               /* code to handle synchronous xrtGraphRun error */ 
               goto fail;                                             
           }      
 
           rval = xrtGraphWaitDone(graphHandle, timeout);
           if (rval == -ETIME) {
               /* wait Graph done timeout without further information */
               xrtErrorCode errCode;
               uint64_t timestamp;
 
               rval = xrtErrorGetLast(devHandle, XRT_ERROR_CLASS_AIE, &errCode, &timestamp);
               if (rval == 0) {
                   size_t len = 0;
                   if (xrtErrorGetString(devHandle, errCode, nullptr, 0, &len))
                       goto fail;
                   std::vector<char> buf(len);  // or C equivalent
                   if (xrtErrorGetString(devHandle, errCode, buf.data(), buf.size()))
                       goto fail;
                   /* code to deal with this specific error */
                   std::cout << buf.data() << std::endl;
               }
          }                  
          /* more code can be added here to check other error class */
        
       
The above code shows
     
     - As good practice synchronous error checking is done directly against all APIs (line 41,47,53,56,59)
     - After timeout occurs from ``xrtGraphWaitDone`` the API ``xrtErrorGetLast`` is called to retrieve asynchronous error code (line 53) 
     - Using the error code API ``xrtErrorGetString`` is called to get the length of the error string (line 56)
     - The API ``xrtErrorGetString`` called again for the second time to get the full error string (line 59)



