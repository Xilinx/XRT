.. _xrt_native_apis.rst:

XRT Native APIs
===============

From 2020.2 release XRT provides a new XRT API set in C, C++, and Python flavor. 

To use the native XRT APIs, the host application must link with the **xrt_coreutil** library. 
Compiling host code with XRT native C++ API requires C++ standard with -std=c++14 or newer. 
On GCC version older than 4.9.0, please use -std=c++1y instead because -std=c++14 is introduced to GCC from 4.9.0.

Example g++ command

.. code-block:: shell

    g++ -g -std=c++14 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o host.exe host.cpp -lxrt_coreutil -pthread


The XRT native API supports both C and C++ flavor of APIs. For general host code development C++ based APIs are recommended. For this reason this document only describes the C++ based API interface. The full C and C++ API documentation (generated based on The Doxygen) can be found in `<./xrt_native.main.rst>`_.


The C++ Class objects used for the APIs are 

+----------------------+-------------------+
|                      |   C++ Class       |  
+======================+===================+
|   Device             | ``xrt::device``   |  
+----------------------+-------------------+
|   XCLBIN             | ``xrt::xclbin``   |  
+----------------------+-------------------+
|   Buffer             | ``xrt::bo``       |  
+----------------------+-------------------+
|   Kernel             | ``xrt::kernel``   |  
+----------------------+-------------------+
|   Run                | ``xrt::run``      |  
+----------------------+-------------------+
| User-managed Kernel  | ``xrt::ip``       |  
+----------------------+-------------------+
|   Graph              | ``xrt::graph``    |  
+----------------------+-------------------+

All the core data structures are defined inside in the header files at ``$XILINX_XRT/include/xrt/`` directory. In the user host code, it is sufficient to include ``"xrt/xrt_kernel.h"`` and ``"xrt/xrt_aie.h"`` (when using Graph APIs) to access all the APIs related to these data structure.

.. code:: c
      :number-lines: 5
           
           #include "xrt/xrt_kernel.h"
           #include "xrt/xrt_aie.h"


The common host code flow using the above data structures is as below
   
- Open Xilinx **Device** and Load the **XCLBIN**
- Create **Buffer** objects to transfer data to kernel inputs and outputs
- Use the Buffer class member functions for the data transfer between host and device (before and after the kernel execution).
- Use **Kernel** and **Run** objects to offload and manage the compute-intensive tasks running on FPGA. 
       
      
Below we will walk through the common API usage to accomplish the above tasks. 

Device and XCLBIN
-----------------

Device and XCLBIN class provide fundamental infrastructure-related interfaces. The primary objective of the device and XCLBIN related APIs are
 
- Open a Device
- Load compiled kernel binary (or XCLBIN) onto the device 


The simplest code to load a XCLBIN as below  

.. code:: c++
      :number-lines: 10
           
           unsigned int dev_index = 0;
           auto device = xrt::device(dev_index);
           auto xclbin_uuid = device.load_xclbin("kernel.xclbin");

       
The above code block shows

- The ``xrt::device`` class's constructor is used to open the device (enumerated as 0)
- The member function ``xrt::device::load_xclbin`` is used to load the XCLBIN from the filename. 
- The member function ``xrt::device::load_xclbin`` returns the XCLBIN UUID, which is required to open the kernel (refer the Kernel Section). 

The class constructor ``xrt::device::device(const std::string& bdf)`` also supports opening a device object from a Pcie BDF passed as a string.

.. code:: c++
      :number-lines: 10
           
           auto device = xrt::device("0000:03:00.1");


The ``xrt::device::get_info()`` is a useful member function to obtain necessary information about a device. Some of the information such as Name, BDF can be used to select a specific device to load an XCLBIN

.. code:: c++
      :number-lines: 10
      
           std::cout << "device name:     " << device.get_info<xrt::info::device::name>() << "\n";
           std::cout << "device bdf:      " << device.get_info<xrt::info::device::bdf>() << "\n";

Buffers
-------

Buffers are primarily used to transfer the data between the host and the device. The Buffer related APIs are discussed in the following three subsections

1. Buffer allocation and deallocation
2. Data transfer using Buffers
3. Miscellaneous other Buffer APIs



1. Buffer allocation and deallocation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The C++ inteface for buffers as below
   
The class constructor ``xrt::bo`` is mainly used to allocates a buffer object 4K align. By default a regular buffer is created (optionally the user can creates other types of buffer by providing a flag). 

.. code:: c++
      :number-lines: 15
           
           auto bank_grp_idx_0 = kernel.group_id(0);
           auto bank_grp_idx_1 = kernel.group_id(1);
    
           auto input_buffer = xrt::bo(device, buffer_size_in_bytes,bank_grp_idx_0);
           auto output_buffer = xrt::bo(device, buffer_size_in_bytes, bank_grp_idx_1);

In the above code ``xrt::bo`` buffer objects are created using the class's constructor. Please note the following 

- As no special flags are used a regular buffer will be created.  
- The second argument specifies the buffer size. 
- The third argument should be used to specify enumerated memory bank index (to specify the memory location) where the buffer should be allocated. In this example, ``xrt::kernel::group_id()`` member function is used to pass the memory bank index where the corresponding kernel arguments are (in the above example argument 0 and 1) connected. Instead of using ``xrt::kernel::group_id()`` member function, the direct memory bank index (as observed in ``xbutil examine --report memory``) can be used as well. 
  
  
Creating special Buffers
************************

The ``xrt::bo()`` constructors accepts multiple other buffer flags those are described using ``enum class`` argument with the following enumerator values

- ``xrt::bo::flags::normal``: Default, Regular Buffer
- ``xrt::bo::flags::device_only``: Device only Buffer (meant to be used only by the kernel).
- ``xrt::bo::flags::host_only``: Host Only Buffer (buffer resides in the host memory directly transferred to/from the kernel)
- ``xrt::bo::flags::p2p``: P2P Buffer, buffer for NVMe transfer  
- ``xrt::bo::flags::cacheable``: Cacheable buffer can be used when host CPU frequently accessing the buffer (applicable for embedded platform).

The below example shows creating a P2P buffer on a device memory bank connected to the argument 3 of the kernel. 

.. code:: c++
      :number-lines: 15
           
           auto p2p_buffer = xrt::bo(device, buffer_size_in_byte,xrt::bo::flags::p2p, kernel.group_id(3));

  
Creating Buffers from the user pointer
**************************************

The ``xrt::bo()`` constructor can also be called using pointer provided by the user. The user pointer must be aligned to 4K boundary.

.. code:: c++
      :number-lines: 15
           
           // Host Memory pointer aligned to 4K boundary
           int *host_ptr;
           posix_memalign(&host_ptr,4096,MAX_LENGTH*sizeof(int)); 
 
           // Sample example filling the allocated host memory       
           for(int i=0; i<MAX_LENGTH; i++) {
           host_ptr[i] = i;  // whatever 
           }
           
           auto mybuf = xrt::bo (device, host_ptr, MAX_LENGTH*sizeof(int), kernel.group_id(3)); 


2. Data transfer using Buffers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XRT Buffer API library provides a rich set of APIs helping the data transfers between the host and the device, between the buffers, etc. We will discuss the following data transfer style

I. Data transfer between host and device by Buffer read/write API
II. Data transfer between host and device by Buffer map API
III. Data transfer between buffers by copy API


I. Data transfer between host and device by Buffer read/write API
*****************************************************************

To transfer the data from the host to the device, the user first needs to update the host-side buffer backing pointer followed by a DMA transfer to the device. 

   
The ``xrt::bo`` class has following member functions for the same functionality

1. ``xrt::bo::write()``
2. ``xrt::bo::sync()`` with flag ``XCL_BO_SYNC_BO_TO_DEVICE``

To transfer the data from the device to the host, the steps are reverse, the user first needs to do a DMA transfer from the device followed by the reading data from the host-side buffer backing pointer. 


The corresponding ``xrt::bo`` class's member functions are

1. ``xrt::bo::sync()`` with flag ``XCL_BO_SYNC_BO_FROM_DEVICE``
2. ``xrt::bo::read()``


Code example of transferring data from the host to the device

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

Note the C++ ``xrt::bo::sync``, ``xrt::bo::write``, ``xrt::bo::read`` etc has overloaded version that can be used for paritial buffer sync/read/write by specifying the size and the offset. For the above code example, the full buffer size and offset=0 are assumed as default arguments. 


II. Data transfer between host and device by Buffer map API
***********************************************************

The API ``xrt::bo::map()`` allows mapping the host-side buffer backing pointer to a user pointer. The host code can subsequently exercise the user pointer for the data reads and writes. However, after writing to the mapped pointer (or before reading from the mapped pointer) the API ``xrt::bo::sync()`` should be used with direction flag for the DMA operation. 

Code example of transferring data from the host to the device by this approach

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

XRT provides ``xrt::bo::copy()`` API for deep copy between the two buffer objects if the platform supports a deep-copy (for detail refer M2M feature described in :ref:`m2m.rst`). If deep copy is not supported by the platform the data transfer happens by shallow copy (the data transfer happens via host). 

.. code:: c++
      :number-lines: 25
           
           
           dst_buffer.copy(src_buffer, copy_size_in_bytes);

The API ``xrt::bo::copy()`` also has overloaded version to provide a different offset than 0 for both the source and the destination buffer. 

3. Miscellaneous other Buffer APIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section describes a few other specific use-cases using buffers. 

DMA-BUF API
***********

XRT provides Buffer export and import APIs primarily used for sharing buffer across devices (P2P application) and processes. 

- ``xrt::bo::export_buffer()``: Export the buffer to an exported buffer handle
- ``xrt::bo()`` constructor : Allocate a BO imported from exported buffer handle


Consider the situation of exporting buffer from device 1 to device 2. 

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

The ``xrt::bo`` class constructor can also be used to allocate a sub-buffer from a parent buffer by specifying a start offset and the size. 

In the example below a sub-buffer is created from a parent buffer of size 4 bytes staring from its offset 0 

.. code:: c++ 
      :number-lines: 18
           
           size_t sub_buffer_size = 4; 
           size_t sub_buffer_offset = 0; 

           auto sub_buffer = xrt::bo(parent_buffer, sub_buffer_size, sub_buffer_offset);


Buffer information
******************

XRT provides few other API Class member functions to obtain information related to the buffer. 

- The member function ``xrt::bo::size()``: Size of the buffer
- The member function ``xrt::bo::address()`` : Physical address of the buffer



Kernel and Run
--------------

To execute a kernel on a device, first a kernel class object has to be created from currently loaded xclbin.  The kernel object can used to execute the kernel function on the hardware instance (Compute Unit or CU) of the kernel.  

A Run object represents an execution of the kernel. Upon finishing the kernel execution, the Run object can be reused to invoke the same kernel function if desired. 

The following topics are discussed below

- Obtaining kernel object from XCLBIN
- Getting the bank group index of a kernel argument
- Reading and write CU mapped registers
- Execution of kernel and dealing with the associated run
- Other kernel execution related API
       

Obtaining kernel object from XCLBIN
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The kernel object is created from the device, XCLBIN UUID and the kernel name using ``xrt::kernel()`` constructor as shown below

.. code:: c++
      :number-lines: 35
          
           auto xclbin_uuid = device.load_xclbin("kernel.xclbin");
           auto krnl = xrt::kernel(device, xclbin_uuid, name); 

**Note**: For the kernel with more than 1 CU, a kernel object can represent all the CUs having identical interface connectivity. If all the CUs of the kernel are not having identical connectivity, the specific CU name(s) should be used to obtain a kernel object to represent the subset of CUs with identical connectivity. Otherwise XRT will do this selection internally to select a group of CUs and discard the rest of the CUs (discarded CUs are not used during the execution of a kernel).  

As an example, assume a kernel name is foo having 3 CUs foo_1, foo_2, foo_3. The CUs foo_1 and foo_2 are connected to DDR bank 0, but the CU foo_3 is connected to DDR bank 1. 

- Opening kernel object for foo_1 and foo_2 (as they have identical interface connection)
       
.. code:: c
      :number-lines: 35
                  
           krnl_obj_1_2 = xrt::kernel(device, xclbin_uuid, "foo:{foo_1,foo_2}");     
   
- Opening kernel object for foo_3
          
.. code:: c
      :number-lines: 35
                  
           krnl_obj_3 = xrt::kernel(device, xclbin_uuid, "foo:{foo_3}");     

      
Exclusive access of the kernel's CU
***********************************
  
By default, ``xrt::kernel()`` opens a kernel's CU in a shared mode so that the CU can be shared with the other processes. In some cases, it is required to open the CU in exclusive mode (for example, when it is required to read/write CU mapped register). Exclusive CU opening fails if the CU is already opened in either shared or exclusive access. To open a CU in exclusive mode the ``xrt::kernel`` constructor can be called with an additional ``enum class`` argument. The enumerator values are: 

- ``xrt::kernel::cu_access_mode::shared`` (default ``xrt::kernel`` constructor argument)
- ``xrt::kernel::cu_access_mode::exclusive`` 

.. code:: c++
      :number-lines: 39
       
           auto krnl = xrt::kernel(device, xclbin_uuid, name, xrt::kernel::cu_access_mode::exclusive); 

   

Getting bank group index of the kernel argument
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We have seen in the Buffer creation section that it is required to provide the buffer location during the buffer creation. The member function ``xrt::kernel::group_id()`` returns the memory bank index (or id) of a specific argument of the kernel. This id is passed as a parameter of ``xrt::bo()`` constructor to create the buffer on the same memory bank. 


Let us review the example below where the buffer is allocated for the kernel's first (argument index 0) argument. 

.. code:: c++
      :number-lines: 15
                       
           auto input_buffer = xrt::bo(device, buffer_size_in_bytes, kernel.group_id(0));



If the kernel bank index is ambiguous then ``kernel.group_id()`` returns the last memory bank index in the list it maintains. This is the case when the kernel has multiple CU with different connectivity for that argument. For example, let's assume a kernel argument (argument 0) is connected to memory bank 0, 1, 2 (for 3 CUs), then kernel.group_id(0) will return the last index from the group {0,1,2}, i.e. 2. As a result the buffer is created on the memory bank 2, so the buffer cannot be used for the CU0 and CU1.  

However, if the kernel object is created for a specific CU (by using the ``{kernel_name:{cu_name(s)}}`` for xrt::kernel consturctor) then the user can use their desired CU as they create in the host code. 

 
Executing the kernel
~~~~~~~~~~~~~~~~~~~~

Execution of the kernel is associated with a **Run** handle (or object). The kernel can be executed by the ``xrt::kernel::operator()`` that takes all the kernel arguments in order. The kernel execution API returns a run object corresponding to the execution. 

.. code:: c++
      :number-lines: 50
   
           // 1st kernel execution
           auto run = kernel(buf_a, buf_b, scalar_1); 
           run.wait();
    
           // 2nd kernel execution with just changing 3rd argument
           run.set_arg(2,scalar_2); // Arguments are specified starting from 0 
           run.start();
           run.wait();


The ``xrt::kernel`` class provides **overloaded operator ()** to execute the kernel with a comma-separated list of arguments.  


The above c++ code block is demonstrating 
  
- The kernel execution using the ``xrt::kernel()`` operator with the list of arguments that returns a xrt::run object. This is an asynchronous API and returns after submitting the task.    
- The member function ``xrt::run::wait()`` is used to block the current thread until the current execution is finished. 
- The member function ``xrt::run::set_arg()`` is used to set one or more kernel argument(s) before the next execution. In the example above, only the last (3rd) argument is changed.  
- The member function ``xrt::run::start()`` is used to start the next kernel execution with new argument(s).   


Other kernel execution related APIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Obtaining the run object before execution**: In the above example we have seen a run object is obtained when the kernel is executed (kernel execution returns a run object). However, a run object can be obtained even before the kernel execution. The flow is as below

- Open a Run object by the ``xrt::run`` constructor with a kernel argument). 
- Set the kernel arguments associated for the next execution by the member function ``xrt::run::set_arg()`. 
- Execute the kernel by the member function ``xrt::run::start()``.
- Wait for the execution finish by the member function ``xrt::run::wait()``. 

**Timeout while wait for kernel finish**: The member function ``xrt::run::wait()`` blocks the current thread until the kernel execution finishes. To specify a timeout supported API ``xrt::run::wait()`` also accepts a timeout in millisecond unit.

**Asynchronous update of the kernel arguments**: The member function ``xrt::run::set_arg()`` is synchronous to the kernel execution. This function can only be used when kernel is in the IDLE state and before the start of the next execution. An asynchronous version of this functionality (only for edge platform) is provided by the member function ``xrt::run::update_arg()`` to change the kernel arguments asynchronous to the kernel execution. 

User Managed Kernel
-------------------

The ``xrt::kernel`` is used to denote the kernels with standard control interface through AXI-Lite control registers. These standard control interfaces are well defined and understood by XRT. XRT manages the kernel execution transparent to the user. 

The XRT also supports custom control interface for a kernel. These type of kernels must be managed by the user by writing/reading from the AXI-Lite register space. To differentiate from the XRT managed kernel, class ``xrt::ip`` is used to denote a user-managed kernel. 

Creating IP object from XCLBIN
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The IP object creation is very similar to creating a kernel. 

.. code:: c++
      :number-lines: 35
          
           auto xclbin_uuid = device.load_xclbin("kernel.xclbin");
           auto ip = xrt::ip(device, xclbin_uuid, "ip_name");
           
An ip object can only be opened in exclusive mode. 

Allocating buffers for the IP inputs/outputs 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Simialr to XRT managed kernel ``xrt::bo`` objects are used to create buffers for IP ports. However, the memory bank location must be specified explicitly by providing enumerated index of the memory bank. 

Below is a example of creating two buffers. Note the last argument of ``xrt::bo`` is the enumated index of the memory bank as seen by the XRT. The bank index can be obtained by ``xbutil examine --report memory`` command.  

.. code:: c++
      :number-lines: 35
          
           auto buf_in_a = xrt::bo(device, DATA_SIZE,XCL_BO_FLAGS_HOST_ONLY,8);
           auto buf_in_b = xrt::bo(device, DATA_SIZE,XCL_BO_FLAGS_HOST_ONLY,8);


Reading and write CU mapped registers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To read and write from the AXI-Lite register space to a CU (``xrt::ip`` instance in the hardware), the required member functions from the ``xrt::ip`` class are
  
-  ``xrt::ip::read_register``
-  ``xrt::ip::write_register``

.. code:: c++
      :number-lines: 35
       
           int read_data; 
           int write_data = 7; 
              
           auto ip = xrt::ip(device, xclbin_uuid, "foo:{foo_1}"); 

           read_data = ip.read_register(READ_OFFSET);
           ip.write_register(WRITE_OFFSET,write_data); 

In the above code block

- The CU named "foo_1" (name syntax: "kernel_name:{cu_name}") is opened exclusively.
- The Register Read/Write operation is performed. 


Graph
-----

In Versal ACAPs with AI Engines, the XRT Graph class (``xrt::graph``) and its member functions can be used to dynamically load, monitor, and control the graphs executing on the AI Engine array. 

**A note regarding Device and Buffer**: In AIE based application, the device and buffer have some additional functionlities. For this reason the classes ``xrt::aie::device`` and ``xrt::aie::buffer`` are recommended to specify device and buffer objects. 

Graph Opening and Closing
~~~~~~~~~~~~~~~~~~~~~~~~~

The ``xrt::graph`` object can be opened using the uuid of the currently loaded XCLBIN file as shown below 

.. code:: c
      :number-lines: 35
           
           auto xclbin_uuid = device.load_xclbin("kernel.xclbin");
           auto graph = xrt::graph(device, xclbin_uuid, "graph_name");
           

The graph object can be used to execute the graph function on the AIE tiles.

Reset Functions
~~~~~~~~~~~~~~~

There are two reset functions are used:

- The member function ``xrt::aie::device::reset_array()`` is used to reset the whole AIE array. 
- The member function ``xrt::graph::reset()`` is used to reset a specified graph by disabling tiles and enabling tile reset. 


.. code:: c
      :number-lines: 45
           
           auto device = xrt::aie::device(0);
           ...
           // AIE Array Reset
           device.reset_array();
           
           auto graph = xrt::graph(device, xclbin_uuid, "graph_name");
           // Graph Reset
           graph.reset();




Graph execution
~~~~~~~~~~~~~~~

XRT provides basic graph execution control interfaces to initialize, run, wait, and terminate graphs for a specific number of iterations. Below we will review some of the common graph execution styles. 

Graph execution for a fixed number of iterations
************************************************

A graph can be executed for a fixed number of iterations followed by a "busy-wait" or a "time-out wait". 

**Busy Wait scheme**

The graph can be executed for a fixed number of iteration by ``xrt::graph::run()`` API using an iteration argument. Subsequently, ``xrt::graph::wait()`` or ``xrt::graph::end()`` API should be used (with argument 0) to wait until graph execution is completed. 

Let's review the below example

- The graph is executed for 3 iterations by API ``xrt::graph::run()`` with the number of iterations as an argument. 
- The API ``xrt::graph::wait(0)`` is used to wait till the iteration is done. 

     - The API `xrt::graph::wait()` is used because the host code needs to execute the graph again. 
- The Graph is executed again for 5 iteration
- The API ``xrt::graph::end(0)`` is used to wait till the iteration is done. 

    - After ``xrt::graph::end()`` the same graph can not be executed. 

.. code:: c
      :number-lines: 35
           
           // start from reset state
           graph.reset();
           
           // run the graph for 3 iteration
           graph.run(3);
           
           // Wait till the graph is done 
           graph.wait(0);  // Use graph::wait if you want to execute the graph again
           
           
           graph.run(5);
           graph.end(0);  // Use graph::end if you are done with the graph execution


**Timeout wait scheme**

As shown in the above example ``xrt::graph::wait(0)`` performs a busy-wait and suspend the execution till the graph is not done. If desired a timeout version of the wait can be achieved by ``xrt::graph::wait(std::chrono::milliseconds)`` which can be used to wait for some specified number of milliseconds, and if the graph is not done do something else in the meantime. An example is shown below

.. code:: c++
      :number-lines: 35
           
           // start from reset state
           graph.reset();
           
           // run the graph for 100 iteration
           graph.run(100);
           
            while (1) {
                          
              try {
                 graph.wait(5);
              }
              catch (const std::system_error& ex) {
            
                 if (ex.code().value() == ETIME) {          
                   
                    std::cout << "Timeout, reenter......" << std::endl;
                    // Do something
             
                 } 
             }
            
             

Infinite Graph Execution
************************

The graph runs infinitely if ``xrt::graph::run()`` is called with iteration argument 0. While a graph running infinitely the APIs ``xrt::graph::wait()``, ``xrt::graph::suspend()`` and ``xrt::graph::end()`` can be used to suspend/end the graph operation after some number of AIE cycles. The API ``xrt::graph::resume()`` is used to execute the infinitely running graph again. 


.. code:: c
      :number-lines: 39
           
           // start from reset state
           graph.reset();
           
           // run the graph infinitely
           graph.run(0);
           
           graph.wait(3000);  // Suspends the graph after 3000 AIE cycles from the previous start 
           
           
           graph.resume(); // Restart the suspended graph again to run forever
           
           graph.suspend(); // Suspend the graph immediately
           
           graph.resume(); // Restart the suspended graph again to run forever
           
           graph.end(5000);  // End the graph operation after 5000 AIE cycles from the previous start


In the example above

- The member function ``xrt::graph::run(0)`` is used to execute the graph infinitely
- The member function ``xrt::graph::wait(3000)`` suspends the graph after 3000 AIE cycles from the graph starts. 

       - If the graph was already run more than 3000 AIE cycles the graph is suspended immediately. 
- The member function ``xrt::graph::resume()`` is used to restart the suspended graph
- The member function ``xrt::graph::suspend()`` is used to suspend the graph immediately
- The member function ``xrt::graph::end(5000)`` is  ending the graph after 5000 AIE cycles from the previous graph start. 
       
       - If the graph was already run more than 5000 AIE cycles the graph ends immediately.
       - Using ``xrt::graph::end()`` eliminates the capability of rerunning the Graph (without loading PDI and a graph reset again). 


Measuring AIE cycle consumed by the Graph
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The member function ``xrt::graph::get_timestamp()`` can be used to determine AIE cycle consumed between a graph start and stop. 

Here in this example, the AIE cycle consumed by 3 iteration is calculated
 

.. code:: c++
      :number-lines: 35
           
           // start from reset state
           graph.reset();
           
           uint64_t begin_t = graph.get_timestamp();
           
           // run the graph for 3 iteration
           graph.run(3);
           
           graph.wait(0); 
           
           uint64_t end_t = graph.get_timestamp();
           
           std::cout<<"Number of AIE cycles consumed in the 3 iteration is: "<< end_t-begin_t; 
           

RTP (Runtime Parameter) control
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``xrt::graph`` class contains member function to update and read the runtime parameters of the graph. 

- The member function ``xrt::graph::update()`` to update the RTP 
- The member function ``xrt::graph::read()`` to read the RTP. 

.. code:: c++
      :number-lines: 35

           graph.reset();

           graph.run(2);

           float increment = 1.0;
           graph.update("mm.mm0.in[2]", increment);
     
           // Do more things
           graph.run(16);
           graph.wait(0);
     
           // Read RTP
           float increment_out;
           graph.read("mm.mm0.inout[0]", &increment_out);
           std::cout<<"\n RTP value read<<increment_out; 
 
In the above example, the member function ``xrt::graph::update()`` and ``xrt::graph::read()`` are used to update and read the RTP values respectively. Note the function arguments 
   
- The hierarchical name of the RTP port
- Variable to set/read the RTP

DMA operation to and from Global Memory IO
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The AIE buffer class ``xrt::aie::bo`` supports member function ``xrt::aie::bo::sync()`` that can be used to synchronize the buffer contents between global memory and AIE. The following code shows a sample example


.. code:: c++
      :number-lines: 35

           auto device = xrt::aie::device(0);
       
           // Buffer from global memory (GM) to AIE
           auto in_bo  = xrt::aie::bo (device, SIZE * sizeof (float), 0, 0);
       
           // Buffer from AIE to global memory (GM)
           auto out_bo  = xrt::aie::bo (device, SIZE * sizeof (float), 0, 0);
       
           auto inp_bo_map = in_bo.map<float *>(); 
           auto out_bo_map = out_bo.map<float *>();

           // Prepare input data 
           std::copy(my_float_array,my_float_array+SIZE,inp_bo_map);


           in_bo.sync("in_sink", XCL_BO_SYNC_BO_GMIO_TO_AIE, SIZE * sizeof(float),0); 

           out_bo.sync("out_sink", XCL_BO_SYNC_BO_AIE_TO_GMIO, SIZE * sizeof(float), 0);
       
       
The above code shows

- Input and output buffer (``in_bo`` and ``out_bo``) to the graph are created and mapped to the user space
- The member function ``xrt::aie::bo::sync`` is used for data transfer using the following arguments
    
          - The name of the GMIO ports associated with the DMA transfer
          - The direction of the buffer transfer 
          
                   - GMIO to Graph: ``XCL_BO_SYNC_BO_GMIO_TO_AIE``
                   - Graph to GMIO: ``XCL_BO_SYNC_BO_AIE_TO_GMIO``
          - The size and the offset of the buffer
    
               
XRT Error API
-------------

In general, XRT APIs can encounter two types of errors:
 
- Synchronous error: Error can be thrown by the API itself. The host code can catch these exception and take necessary steps. 
- Asynchronous error: Errors from the underneath driver, system, hardware, etc. 
       
XRT provides an ``xrt::error`` class and its member functions to retrieve the asynchronous errors into the userspace host code. This helps to debug when something goes wrong.
 
- Member function ``xrt::error::get_error_code()`` - Gets the last error code and its timestamp of a given error class
- Member function ``xrt::error::get_timestamp()`` - Gets the timestamp of the last error
- Member function ``xrt:error::to_string()`` - Gets the description string of a given error code.

**NOTE**: The asynchronous error retrieving APIs are at an early stage of development and only supports AIE related asynchronous errors. Full support for all other asynchronous errors is planned in a future release. 

Example code

.. code:: c++
      :number-lines: 41

           graph.run(runInteration);
           
           try {
              graph.wait(timeout);
           }
           catch (const std::system_error& ex) {
            
              if (ex.code().value() == ETIME) {          
                 xrt::error error(device, XRT_ERROR_CLASS_AIE);

                 auto errCode = error.get_error_code(); 
                 auto timestamp = error.get_timestamp();
                 auto err_str = error.to_string(); 
                  
                 /* code to deal with this specific error */
                 std::cout << err_str << std::endl;
              } else {
               /* Something else */
              }
           }
        
       
The above code shows
     
- After timeout occurs from ``xrt::graph::wait()`` the member functions ``xrt::error`` class are called to retrieve asynchronous error code and timestamp
- Member function ``xrt::error::to_string()`` is called to obtain the error string. 



