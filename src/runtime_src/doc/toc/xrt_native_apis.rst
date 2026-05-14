.. _xrt_native_apis.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
   comment:: Copyright (C) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.

XRT Native APIs
===============

XRT exposes host-side APIs in C++ and Python.

Native XRT host code must link against the **xrt_coreutil** library.
C++ examples in this guide assume a compiler with **ISO C++17** or newer (for example ``-std=c++17``).

Example ``g++`` invocation:

.. code-block:: shell

    g++ -g -std=c++17 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o host.exe host.cpp -lxrt_coreutil -pthread


For general host code development, C++-based APIs are recommended, hence this document only describes the C++-based API interfaces. The full Doxygen generated C and C++ API documentation can be found in :doc:`xrt_native.main`.


The C++ Class objects used for the APIs are the following:

+----------------------+---------------------+------------------------------------------------+
| Core Object          |   C++ Class         |  Header Files                                  |
+======================+=====================+================================================+
|   Device             | ``xrt::device``     |  ``#include <xrt/xrt_device.h>``               |
+----------------------+---------------------+------------------------------------------------+
|   Buffer             | ``xrt::bo``         |  ``#include <xrt/xrt_bo.h>``                   |
+----------------------+---------------------+------------------------------------------------+
|   Kernel             | ``xrt::kernel``     |  ``#include <xrt/xrt_kernel.h>``               |
+----------------------+---------------------+------------------------------------------------+
|   Run                | ``xrt::run``        |  ``#include <xrt/xrt_kernel.h>``               |
+----------------------+---------------------+------------------------------------------------+
|   Run-list           | ``xrt::runlist``    |  ``#include <xrt/experimental/xrt_kernel.h>``  |
+----------------------+---------------------+------------------------------------------------+
|   Context            | ``xrt::hw_context`` |  ``#include <xrt/xrt_hw_context.h>``           |
+----------------------+---------------------+------------------------------------------------+
|   Xclbin             | ``xrt::xclbin``     |  ``#include <xrt/experimental/xrt_xclbin.h>``  |
+----------------------+---------------------+------------------------------------------------+
|   Control code (ELF) | ``xrt::elf``        |  ``#include <xrt/experimental/xrt_elf.h>``     |
+----------------------+---------------------+------------------------------------------------+
| User-managed Kernel  | ``xrt::ip``         |  ``#include <xrt/experimental/xrt_ip.h>``      |
+----------------------+---------------------+------------------------------------------------+
|   AIE Graph          | ``xrt::graph``      |  ``#include <xrt/xrt_aie.h>``                  |
|                      |                     |                                                |
|                      |                     |  ``#include <xrt/xrt_graph.h>``                |
+----------------------+---------------------+------------------------------------------------+

The majority of core data structures are defined in the header files under ``$XILINX_XRT/include/xrt/``. Newer features such as ``xrt::ip``, ``xrt::runlist``, ``xrt::elf``, and related types live under ``$XILINX_XRT/include/xrt/experimental/``. APIs in that experimental area are subject to breaking changes.

The common host code flow using the above data structures is as follows:

- Open AMD **Device** and load a kernel defined either in **ELF**, **XCLBIN** or combination of both.
- Create **Buffer** objects to hold data for kernel inputs and outputs
- If required use the Buffer class member functions for the data transfer between host and device (before and after the kernel execution).
- Use **Kernel** and **Run** objects to offload and manage the compute-intensive tasks running on FPGA.
- Release the **Buffer** object and close the **Device**.


Below we will walk through the common API usage to accomplish the above tasks.

Device and Context (NPU Flow)
-----------------------------

Device and Context classes provide fundamental infrastructure-related interfaces. The primary objectives of the device- and context-related APIs are:

- Open a device and create a context on the device
- Load a compiled kernel binary (or an elf) onto the device


The simplest code to load an elf is as below:

.. code:: c++
      :number-lines: 10

           unsigned int dev_index = 0;
           auto device = xrt::device(dev_index);
           xrt::elf elf{"config.elf"};
           auto hwctx = xrt::hw_context(device, elf);


The above code block shows:

- The ``xrt::device`` class's constructor is used to open the device (enumerated as 0)
- The ``xrt::elf`` class's constructor is used to load a compiled binary into host memory from the filesystem ("config.elf")
- The ``xrt::hw_context`` class's constructor is used to load the compiled binary on the device

The class constructor ``xrt::device::device(const std::string& bdf)`` also supports opening a device object from a PCIe BDF passed as a string.

.. code:: c++
      :number-lines: 10

           auto device = xrt::device("0000:03:00.1");


The ``xrt::device::get_info()`` is a useful member function to obtain necessary information about a device. Some of the information such as Name, BDF can be used to select a specific device to load an XCLBIN

.. code:: c++
      :number-lines: 10

           std::cout << "device name:     " << device.get_info<xrt::info::device::name>() << "\n";
           std::cout << "device bdf:      " << device.get_info<xrt::info::device::bdf>() << "\n";


The class constructor ``xrt::elf(const void *data, size_t size)`` also supports creating an elf object from compiled data already in memory.

.. code:: c++
      :number-lines: 10

           void *myctrlcode = mycompiler_out();
           xrt::elf elf(myctrlcode, 0x10000);


Device and XCLBIN (Classic FPGA Flow)
-------------------------------------

Device and XCLBIN classes provide fundamental infrastructure-related interfaces. The primary objectives of the device- and XCLBIN-related APIs are

- Open a device
- Load a compiled kernel binary (or XCLBIN) onto the device


The simplest code to load an XCLBIN is as below:

.. code:: c++
      :number-lines: 10

           unsigned int dev_index = 0;
           auto device = xrt::device(dev_index);
           auto xclbin_uuid = device.load_xclbin("kernel.xclbin");


The above code block shows:

- The ``xrt::device`` class's constructor is used to open the device (enumerated as 0)
- The member function ``xrt::device::load_xclbin`` is used to load the XCLBIN from the filename.
- The member function ``xrt::device::load_xclbin`` returns the XCLBIN UUID, which is required to open the kernel (see the Kernel section).

The class constructor ``xrt::device::device(const std::string& bdf)`` also supports opening a device object from a PCIe BDF passed as a string.

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

Buffers are primarily used to store the input/output data for use by the device. The buffer-related APIs are discussed in the following three subsections:

1. Buffer allocation and deallocation
2. Data transfer using Buffers
3. Miscellaneous other Buffer APIs



1. Buffer allocation and deallocation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The C++ interface for buffers is illustrated below.

The class constructor ``xrt::bo`` is mainly used to allocate a buffer object 4K aligned. By default, a regular buffer is created (optionally the user can create other types of buffers by providing a flag).

.. code:: c++
      :number-lines: 15

           auto bank_grp_arg0 = kernel.group_id(0); // Memory bank index for kernel argument 0
           auto bank_grp_arg1 = kernel.group_id(1); // Memory bank index for kernel argument 1

           auto input_buffer = xrt::bo(device, buffer_size_in_bytes,bank_grp_arg0);
           auto output_buffer = xrt::bo(device, buffer_size_in_bytes, bank_grp_arg1);

In the above code ``xrt::bo`` buffer objects are created using the class constructor. Please note the following:

- As no special flags are used a regular buffer will be created. Regular buffer is most common type of buffer that has a host backing pointer allocated by user space in heap memory and a device buffer allocated in the specified memory bank.
- The second argument specifies the buffer size.
- The third argument is used to specify the enumerated memory bank index (to specify the buffer location) where the buffer should be allocated. There are two ways to specify the memory bank index

 - Through kernel arguments: In the above example, the ``xrt::kernel::group_id()`` member function is used to pass the memory bank index. This member function accepts a kernel argument index and detects the corresponding memory bank index by inspecting the XCLBIN.
 - Passing a memory bank index: The ``xrt::kernel::group_id()`` overload also accepts the memory bank index directly (as reported by ``xrt-smi examine --report memory``).


Creating special Buffers
************************

The ``xrt::bo()`` constructors accept additional buffer flags via an ``enum class`` argument. The main enumerator values are:

- ``xrt::bo::flags::normal``: Regular buffer (default)
- ``xrt::bo::flags::device_only``: Device only buffer (meant to be used only by the kernel, there is no host backing pointer).
- ``xrt::bo::flags::host_only``: Host only buffer (buffer resides in the host memory directly transferred to/from the kernel)
- ``xrt::bo::flags::p2p``: P2P buffer, A special type of device-only buffer capable of peer-to-peer transfer
- ``xrt::bo::flags::cacheable``: Use a cacheable buffer when the host CPU accesses the buffer frequently (typical on edge platforms).

.. note::

   Buffer flags are specific to the host and device. Not all the flags are honored on all systems.

The below example shows creating a P2P buffer on a device memory bank connected to argument 3 of the kernel.

.. code:: c++
      :number-lines: 15

           auto p2p_buffer = xrt::bo(device, buffer_size_in_bytes, xrt::bo::flags::p2p, kernel.group_id(3));


Creating Buffers from the user pointer
**************************************

The ``xrt::bo()`` constructor can also be called using a pointer provided by the user. The user pointer must be aligned to 4K boundary.

.. code:: c++
      :number-lines: 15

           // Host Memory pointer aligned to 4K boundary
           int *host_ptr;
           posix_memalign(&host_ptr,4096,MAX_LENGTH*sizeof(int));

           // Simple example: fill the allocated host memory
           for(int i=0; i<MAX_LENGTH; i++) {
             host_ptr[i] = i;  // whatever
           }

           auto mybuf = xrt::bo (device, host_ptr, MAX_LENGTH*sizeof(int), kernel.group_id(3));


2. Data transfer using Buffers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XRT Buffer API library provides a rich set of APIs helping the data transfers between the host memory and the device attached memory, or between the buffers, etc.

.. note::

   Shared Virtual Memory (SVM) systems like Ryzen with integrated NPU or Versal with integrated NPU configured with Linux Contiguous Memory Allocator does **not** require explicit data transfers using buffers.

We will discuss the following data transfer styles:

I. Data transfer between host and device by Buffer read/write API
II. Data transfer between host and device by Buffer map API
III. Data transfer between buffers by copy API


I. Data transfer between host and device by buffer read/write API
*****************************************************************

To transfer the data from the host to the device, the user first needs to update the host-side buffer backing pointer followed by a DMA transfer to the device.


The ``xrt::bo`` class has following member functions for the same functionality

1. ``xrt::bo::write()``
2. ``xrt::bo::sync()`` with flag ``XCL_BO_SYNC_BO_TO_DEVICE``

To transfer data from the device to the host, reverse the steps: perform a DMA transfer from the device, then read data from the host-side buffer backing store.


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

Note that the C++ ``xrt::bo::sync``, ``xrt::bo::write``, ``xrt::bo::read``, and related overloads support partial buffer sync, read, and write by passing size and offset. In the example above, the full buffer size and offset ``0`` are used implicitly.

Also note that if the buffer is created through the user-pointer, the ``xrt::bo::write`` or ``xrt::bo::read`` is not required before or after the ``xrt::bo::sync`` call.

For the device only buffers (created with ``xrt::bo::flags::device_only`` flag) the ``xrt::bo::sync()`` operation is not required, only ``xrt::bo::write()`` (or ``xrt::bo::read()``) is sufficient for DMA operation. As for the device only buffer there is no host backing storage, the ``xrt::bo::write()`` (or ``xrt::bo::read()``) directly performs DMA operation to (or from) the device memory.

Below is the example for creation of device only buffers.

.. code:: c++
      :number-lines: 18

           xrt::bo::flags device_flags = xrt::bo::flags::device_only;
           auto device_only_buffer = xrt::bo(device, size_in_bytes, device_flags, bank_grp_arg0);

For device-only buffers (no host backing store), ``xrt::bo::read()`` and ``xrt::bo::write()`` perform DMA directly to or from device memory.

- ``xrt::bo::write(const void* src, size_t size, size_t seek)``: Copies data from src to device buffer directly.
- ``xrt::bo::read(void* dst, size_t size, size_t skip)``: Copies data from device buffer to dst.

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

XRT provides ``xrt::bo::copy()`` for a deep copy between two buffer objects when the platform supports memory-to-memory (M2M) DMA between memory banks. If deep copy is not supported, the transfer falls back to a shallow copy via the host.

.. code:: c++
      :number-lines: 25


           dst_buffer.copy(src_buffer, copy_size_in_bytes);

The API ``xrt::bo::copy()`` also has overloaded versions to provide a different offset than 0 for both the source and the destination buffer.

3. Miscellaneous other Buffer APIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section describes a few other specific use-cases using buffers.

DMA-BUF API
***********

XRT provides buffer export and import APIs primarily used for sharing buffers across devices (P2P) and processes. The buffer handle obtained from ``xrt::bo::export_buffer()`` is essentially a file descriptor, hence sending across the processes requires a suitable IPC mechanism (example, UDS or Unix Domain Socket) to translate the file descriptor of one process into another process.

- ``xrt::bo::export_buffer()``: Export the buffer to an exported buffer handle
- ``xrt::bo()`` constructor: Allocate a BO imported from exported buffer handle


Consider the situation of exporting buffer from device 1 to device 2 (inside same host process).

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

In the example below a sub-buffer is created from a parent buffer of size 4 bytes starting from its offset 0

.. code:: c++
      :number-lines: 18

           size_t sub_buffer_size = 4;
           size_t sub_buffer_offset = 0;

           auto sub_buffer = xrt::bo(parent_buffer, sub_buffer_size, sub_buffer_offset);


Buffer information
******************

XRT provides a few other class member functions to obtain buffer-related information.

- The member function ``xrt::bo::size()``: Size of the buffer
- The member function ``xrt::bo::address()`` : Physical address of the buffer



Kernel and Run
--------------

To execute a kernel on a device, a kernel class ``xrt::kernel`` object has to be created from currently loaded xclbin or from the context object.  The kernel object can be used to execute the kernel function on the hardware instance (Compute Unit or CU) of the kernel.

A Run object ``xrt::run`` represents an execution of the kernel. Upon finishing the kernel execution, the Run object can be reused to invoke the same kernel function if desired.

The following topics are discussed below

- Obtaining kernel object from XCLBIN for the classic FPGA flow
- Obtaining kernel object from context for the NPU flow
- Getting the bank group index of a kernel argument
- Execution of kernel and dealing with the associated run
- Other kernel related API


Obtaining kernel object from XCLBIN (Classic FPGA flow)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The kernel object is created from the device, XCLBIN UUID and the kernel name using ``xrt::kernel()`` constructor as shown below:

.. code:: c++
      :number-lines: 35

           auto xclbin_uuid = device.load_xclbin("kernel.xclbin");
           auto krnl = xrt::kernel(device, xclbin_uuid, name);

.. note::

   A single kernel object (when created by a kernel name) can be used to execute multiple CUs as long as CUs are having identical interface connectivity. If all the CUs of the kernel are not having identical connectivity, XRT assigns a subset of CUs (one or more CUs with identical connectivity) to the created kernel object and discards the rest of the CUs (discarded CUs are not used during the execution of a kernel).  For this type of situation creating a kernel object using mangled CU names can be more useful.

As an example, assume a kernel name is foo having 3 CUs foo_1, foo_2, foo_3. The CUs foo_1 and foo_2 are connected to DDR bank 0, but the CU foo_3 is connected to DDR bank 1.

- Opening kernel object for foo_1 and foo_2 (as they have identical interface connection)

.. code:: c
      :number-lines: 35

           krnl_obj_1_2 = xrt::kernel(device, xclbin_uuid, "foo:{foo_1,foo_2}");

- Opening kernel object for foo_3

.. code:: c
      :number-lines: 35

           krnl_obj_3 = xrt::kernel(device, xclbin_uuid, "foo:{foo_3}");


Obtaining kernel object from context (NPU flow)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The kernel object is created from the context using the kernel name using ``xrt::ext::kernel()`` constructor as shown below

.. code:: c++
      :number-lines: 35

           auto hwctx = xrt::hw_context(dev, elfbin);
           auto krn = xrt::ext::kernel(hwctx, "DPU");


Getting bank group index of the kernel argument
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We have seen in the Buffer creation section that it is required to provide the buffer location during the buffer creation. The member function ``xrt::kernel::group_id()`` returns the memory bank index (or id) of a specific argument of the kernel. This id is passed as a parameter of ``xrt::bo()`` constructor to create the buffer on the same memory bank.


Let us review the example below where the buffer is allocated for the kernel's first (argument index 0) argument.

.. code:: c++
      :number-lines: 15

           auto input_buffer = xrt::bo(dev, buffer_size_in_bytes, krn.group_id(0));



If the kernel bank index is ambiguous then ``kernel.group_id()`` returns the last memory bank index in the list it maintains. This is the case when the kernel has multiple CU with different connectivity for that argument. For example, let's assume a kernel argument (argument 0) is connected to memory bank 0, 1, 2 (for 3 CUs), then ``kernel.group_id(0)`` will return the last index from the group {0,1,2}, i.e. 2. As a result the buffer is created on the memory bank 2, so the buffer cannot be used for the CU0 and CU1.

However, in the above situation, you can create three distinct ``xrt::kernel`` objects—one per CU—using the ``{kernel_name:{cu_name(s)}}`` form of the ``xrt::kernel`` constructor, and run each CU with its own kernel object.


Executing the kernel
~~~~~~~~~~~~~~~~~~~~

Execution of the kernel is associated with a **Run** object. The kernel can be executed by the ``xrt::kernel::operator()`` that takes all the kernel arguments in order. The kernel execution API returns a run object corresponding to the execution.

.. code:: c++
      :number-lines: 50

           // 1st kernel execution
           auto runt = krn(buf_a, buf_b, scalar_1);
           runt.wait();

           // 2nd kernel execution with just changing 3rd argument
           runt.set_arg(2, scalar_2); // Arguments are specified starting from 0
           runt.start();
           runt.wait();

           // 3rd kernel execution with explicit run object creation
           auto runs = xrt::run(krn);
           runs.set_arg(0, buf_a);
           runs.set_arg(1, buf_b);
           runs.set_arg(2, scalar_1);
           runs.start();
           runs.wait();

The ``xrt::kernel`` class provides **overloaded operator ()** to execute the kernel with a comma-separated list of arguments.

The C++ example above illustrates the following:

- Invoking the kernel with ``operator()`` and a comma-separated argument list returns an ``xrt::run`` object. The call is asynchronous and returns after the work is submitted.
- The member function ``xrt::run::wait()`` is used to block the current thread until the current execution is finished.
- The member function ``xrt::run::set_arg()`` is used to set one or more kernel argument(s) before the next execution. In the example above, only the last (3rd) argument is changed.
- The member function ``xrt::run::start()`` is used to start the next kernel execution with new argument(s).
- Optionally construct an ``xrt::run`` from the kernel, then use ``xrt::run::set_arg()`` for each argument, ``xrt::run::start()`` to launch, and ``xrt::run::wait()`` to block until completion.


Executing a kernel list
~~~~~~~~~~~~~~~~~~~~~~~

Multiple kernel runs of the same or different kernels can be chained in a runlist. The runlist can then be executed with a single submission, ``xrt::runlist::execute()``, and a single wait, ``xrt::runlist::wait()``, for the entire list. This enables efficient graph mode execution for ML models running on NPU.

.. code:: c++
      :number-lines: 50

           auto hwctx = xrt::hw_context(device, elf);
           auto krn1 = xrt::ext::kernel(hwctx, "DPU1");
           auto krn2 = xrt::ext::kernel(hwctx, "DPU2");
           auto krn3 = xrt::ext::kernel(hwctx, "DPU3");

           xrt::runlist rlist(hwctx);
           auto run1 = xrt::run(krn1);
           auto run2 = xrt::run(krn2);
           auto run3 = xrt::run(krn3);

           // Chain the kernels in the runlist
           run1.set_arg(0, buf_a);
           run1.set_arg(1, buf_b);
           run1.set_arg(2, buf_c);
           rlist.add(run1);
           run2.set_arg(0, buf_c);
           run2.set_arg(2, buf_d);
           rlist.add(run2);
           run3.set_arg(0, buf_g);
           run3.set_arg(1, buf_h);
           run3.set_arg(2, scalar_0);
           rlist.add(run3);
           // Now launch and wait
           rlist.execute();
           rlist.wait();


Waiting for completion of a run
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The member function ``xrt::run::wait()`` blocks the current thread until the kernel execution finishes. To specify a timeout supported API ``xrt::run::wait()`` also accepts a timeout in milliseconds. It returns ``ert_cmd_state`` so the caller can tell whether the kernel has finished.

.. code:: c++
      :number-lines: 15

           auto runt = krn(buf_a, buf_b, scalar_1);
           // wait for 100 ms
           while (1) {
               auto result = runt.wait(100);
               if ((result == ERT_CMD_STATE_RUNNING) ||
                   (result == ERT_CMD_STATE_SUBMITTED) ||
                   (result == ERT_CMD_STATE_QUEUED))
                   continue;
           }


The preferred way to wait for a kernel run is ``xrt::run::wait2()``. Like ``xrt::run::wait()``, it blocks until execution finishes, but it also throws if the run completes with an error status.

.. code:: c++
      :number-lines: 15

           auto runt = krn(buf_a, buf_b, scalar_1);
           // wait for 100 ms
           while (1) {
               try {
                   runt.wait2(100);
               } catch (std::exception &ex) {
                   std::cout << ex.what();
               }
           }


User Managed Kernel (Classic FPGA flow)
---------------------------------------

The ``xrt::kernel`` is used to execute the kernels with standard control interface through AXI-Lite control registers. These standard control interfaces are well defined and understood by XRT but transparent to the user. These XRT managed kernels should always be represented by ``xrt::kernel`` objects in the host code.

XRT also supports a custom control interface for a kernel. Kernels of this kind (user-managed kernels) must be driven by the host reading and writing the AXI-Lite registers that control them. To differentiate from the XRT managed kernel, class ``xrt::ip`` is used to specify a user-managed kernel inside the user host code.

Creating ``xrt::ip`` object from XCLBIN
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``xrt::ip`` object creation is very similar to creating a kernel.

.. code:: c++
      :number-lines: 35

           auto xclbin_uuid = device.load_xclbin("kernel.xclbin");
           auto ip = xrt::ip(device, xclbin_uuid, "ip_name");

An ip object can only be opened in exclusive mode. That means at a time, only one thread/process can access IP at the same time. This is required for a safety reason because multiple threads/processes reading/writing to the AXI-Lite registers at the same time potentially leads to a race situation.

Allocating buffers for the IP inputs/outputs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Similar to XRT managed kernel ``xrt::bo`` objects are used to create buffers for IP ports. However, the memory bank location must be specified explicitly by providing enumerated index of the memory bank.

Below is an example of creating two buffers. Note the last argument of ``xrt::bo`` is the enumerated index of the memory bank as seen by the XRT (in this example index 8 corresponds to the host-memory bank). The bank index can be obtained by ``xrt-smi examine --report memory`` command.

.. code:: c++
      :number-lines: 35

           auto buf_in_a = xrt::bo(device, DATA_SIZE, xrt::bo::flags::host_only, 8);
           auto buf_in_b = xrt::bo(device, DATA_SIZE, xrt::bo::flags::host_only, 8);


Reading and writing CU-mapped registers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To read and write from the AXI-Lite register space to a CU (specified by ``xrt::ip`` object in the host code), the required member functions from the ``xrt::ip`` class are

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


Graph (Versal AIE)
------------------

On Versal ACAPs with AI Engines (AIE and AIE2), the XRT graph class (``xrt::graph``) and its member functions can be used to dynamically load, monitor, and control graphs running on the AI Engine array.

**A note regarding device and buffer**: In AIE-based applications, device and buffer objects have additional behavior. Prefer ``xrt::aie::device`` and ``xrt::aie::buffer`` when you need those capabilities.

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

The member function ``xrt::graph::reset()`` is used to reset a specified graph by disabling tiles and enabling tile reset.


.. code:: c
      :number-lines: 45

           auto device = xrt::aie::device(0);

           // load XCLBIN
           ...

           auto graph = xrt::graph(device, xclbin_uuid, "graph_name");
           // Graph Reset
           graph.reset();


The member function ``xrt::aie::device::reset_array()`` resets the entire AIE array. After that call, the PDI is no longer loaded, so you must load a suitable AIE-only XCLBIN again (advanced flow only).



Graph execution
~~~~~~~~~~~~~~~

XRT provides basic graph execution control interfaces to initialize, run, wait, and terminate graphs for a specific number of iterations. Below we will review some of the common graph execution styles.

Graph execution for a fixed number of iterations
************************************************

A graph can be executed for a fixed number of iterations followed by a "busy-wait" or a "time-out wait".

**Busy Wait scheme**

The graph can be run for a fixed number of iterations using ``xrt::graph::run()`` with an iteration count. Then use ``xrt::graph::wait()`` or ``xrt::graph::end()`` (with argument ``0``) to block until that burst of execution completes.

Let's review the below example

- The graph runs for three iterations via ``xrt::graph::run()`` with the iteration count as an argument.
- ``xrt::graph::wait(0)`` blocks until that run completes.

  - Use ``xrt::graph::wait()`` when you intend to run the graph again afterward.
- The graph runs again for five iterations.
- ``xrt::graph::end(0)`` blocks until that run completes.

  - After ``xrt::graph::end()``, the same graph cannot be run again without reloading the design and resetting.

.. code:: c
      :number-lines: 35

           // start from reset state
           graph.reset();

           // run the graph for 3 iterations
           graph.run(3);

           // Wait until the graph is done
           graph.wait(0);  // Use graph::wait if you want to execute the graph again


           graph.run(5);
           graph.end(0);  // Use graph::end when finished with graph execution


**Timeout wait scheme**

As in the busy-wait example above, ``xrt::graph::wait(0)`` blocks the host thread until that graph run completes.

``xrt::graph`` provides **two** ``wait`` overloads:

- ``xrt::graph::wait(std::chrono::milliseconds timeout)`` — block until the graph reports **done**, or until **timeout** host-side milliseconds elapse.

- ``xrt::graph::wait(uint64_t cycles)`` — with **non-zero** ``cycles``, wait for that many **AIE cycles** since the last graph start, then **suspend** the graph (used with infinite execution; see :ref:`infinite-graph-execution`).

Example: poll until the graph completes, doing other host work between attempts.

.. code:: c++
      :number-lines: 35

           // start from reset state
           graph.reset();

           // run the graph for 100 iterations
           graph.run(100);

           while (true) {
             try {
               graph.wait(std::chrono::milliseconds(100));
               break;  // returned: graph completed
             }
             catch (const std::system_error& ex) {
               if (ex.code().value() == ETIME) {
                 std::cout << "Timeout, do other work and retry..." << std::endl;
                 // DO Something
               }
             }
           }


.. _infinite-graph-execution:

Infinite Graph Execution
************************

The graph runs indefinitely if ``xrt::graph::run()`` is called with an iteration count of ``0``. While the graph is running in that mode, ``xrt::graph::wait(uint64_t)``, ``xrt::graph::suspend()``, and ``xrt::graph::end(uint64_t)`` can suspend or end execution after a given number of **AIE cycles**. ``xrt::graph::resume()`` continues a suspended graph.


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
- ``xrt::graph::wait(3000)`` suspends the graph after 3000 AIE cycles from the start of the run.

  - If the graph has already executed more than 3000 cycles, it suspends immediately.
- The member function ``xrt::graph::resume()`` is used to restart the suspended graph
- The member function ``xrt::graph::suspend()`` is used to suspend the graph immediately
- ``xrt::graph::end(5000)`` ends the graph after 5000 AIE cycles from the previous start.

  - If the graph has already executed more than 5000 cycles, it ends immediately.
  - After ``xrt::graph::end()``, you cannot run the same graph again without reloading the PDI and performing a graph reset.


Measuring AIE cycle consumed by the Graph
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The member function ``xrt::graph::get_timestamp()`` can be used to determine AIE cycle consumed between a graph start and stop.

In this example, the AIE cycles consumed across three iterations are measured.


.. code:: c++
      :number-lines: 35

           // start from reset state
           graph.reset();

           uint64_t begin_t = graph.get_timestamp();

           // run the graph for 3 iterations
           graph.run(3);

           graph.wait(0);

           uint64_t end_t = graph.get_timestamp();

           std::cout << "Number of AIE cycles consumed in 3 iterations: " << (end_t - begin_t) << std::endl;


RTP (Runtime Parameter) control
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``xrt::graph`` class provides member functions to update and read runtime parameters (RTP) of the graph.

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
           std::cout << "\n RTP value read: " << increment_out << std::endl;

In the above example, the member function ``xrt::graph::update()`` and ``xrt::graph::read()`` are used to update and read the RTP values respectively. Note the function arguments

- The hierarchical name of the RTP port
- Variable to set/read the RTP

DMA operation to and from Global Memory IO
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The AIE buffer class ``xrt::aie::bo`` provides ``xrt::aie::bo::sync()`` to synchronize data between global memory and the AIE. The following example illustrates its use.


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


GMIOs and external buffers
---------------------------

XRT provides ``xrt::aie::buffer`` for GMIO and external-buffer endpoints. GMIOs and external buffers move data between global memory (for example DDR) and the AI Engine. They help manage data flow so large workloads can be staged without exhausting local tile memory.

Construction of ``xrt::aie::buffer`` succeeds only if a GMIO or external buffer with the given name exists in the loaded design.

The class overloads ``xrt::aie::buffer::sync(...)`` to move data between global memory and the AIE.

- ``xrt::aie::buffer::sync(xrt::bo bo, ...)`` synchronizes between an ``xrt::aie::buffer`` (GMIO or external buffer) and an ``xrt::bo`` in global memory.

- ``xrt::aie::buffer::sync(xrt::bo ping, xrt::bo pong, ...)`` attaches ping/pong ``xrt::bo`` buffers to an external buffer for parallel transfers.

The example below uses one input and one output GMIO or external buffer: data moves from the global buffer ``in_bo`` into ``gr.in1``.

.. code:: c++
        :number-lines: 1

           auto device = xrt::aie::device(0);
           auto uuid = device.load_xclbin("kernel.xclbin");

           // Create buffer in DDR / global memory and prepare input
           auto in_bo  = xrt::aie::bo (device, SIZE * sizeof (float), 0, 0);
           auto inp_bo_map = in_bo.map<float *>();
           std::copy(my_float_array,my_float_array+SIZE,inp_bo_map);

           // Create buffer in DDR / global memory for output
           auto out_bo  = xrt::aie::bo (device, SIZE * sizeof (float), 0, 0);
           auto out_bo_map = out_bo.map<float *>();

           // GMIO / external buffer for input — sync from in_bo
           auto in_buffer = xrt::aie::buffer(device, uuid, "gr.in1");
           in_buffer.sync(in_bo, XCL_BO_SYNC_BO_GMIO_TO_AIE, SIZE * sizeof(float),0);

           // Run graphs that use the output GMIO / external buffer

           // GMIO / external buffer for output — sync to out_bo
           auto out_buffer  = xrt::aie::buffer(device, uuid, "gr.out1");
           out_buffer.sync(out_bo, XCL_BO_SYNC_BO_AIE_TO_GMIO, SIZE * sizeof(float),0);

The class also overloads ``xrt::aie::buffer::async(...)`` to start an asynchronous transfer involving an ``xrt::bo``.

- ``xrt::aie::buffer::async(xrt::bo bo, ...)`` starts an asynchronous sync between an ``xrt::aie::buffer`` and global memory.

- ``xrt::aie::buffer::async(xrt::bo ping, xrt::bo pong, ...)`` starts an asynchronous sync using ping/pong ``xrt::bo`` objects.

Use ``xrt::aie::buffer::wait()`` to wait for the asynchronous operation to finish.

The example below is the same scenario as above, using ``async`` and ``wait`` instead of ``sync`` alone.

.. code:: c++
        :number-lines: 1

           auto device = xrt::aie::device(0);
           auto uuid = device.load_xclbin("kernel.xclbin");

           // Create buffer in DDR / global memory and prepare input
           auto in_bo  = xrt::aie::bo (device, SIZE * sizeof (float), 0, 0);
           auto inp_bo_map = in_bo.map<float *>();
           std::copy(my_float_array,my_float_array+SIZE,inp_bo_map);

           // Create buffer in DDR / global memory for output
           auto out_bo  = xrt::aie::bo (device, SIZE * sizeof (float), 0, 0);
           auto out_bo_map = out_bo.map<float *>();

           // GMIO / external buffer for input
           auto in_buffer = xrt::aie::buffer(device, uuid, "gr.in1");
           in_buffer.async(in_bo, XCL_BO_SYNC_BO_GMIO_TO_AIE, SIZE * sizeof(float),0);

           // Run graphs that use the output GMIO / external buffer

           // GMIO / external buffer for output
           auto out_buffer  = xrt::aie::buffer(device, uuid, "gr.out1");
           out_buffer.async(out_bo, XCL_BO_SYNC_BO_AIE_TO_GMIO, SIZE * sizeof(float),0);
           out_buffer.wait();

Ping-pong buffers
~~~~~~~~~~~~~~~~~

The example below attaches ping-pong ``xrt::bo`` buffers in global memory to an external buffer (``gr.ext1``) for double-buffered input, then syncs the result to ``out_bo`` via ``gr.out1``.

.. code:: c++
        :number-lines: 1

           auto device = xrt::aie::device(0);
           auto uuid = device.load_xclbin("kernel.xclbin");

           // Host buffer and GMIO / external buffer for primary input
           auto in_bo  = xrt::aie::bo (device, SIZE * sizeof (float), 0, 0);
           auto in_bo_map = in_bo.map<float *>();
           std::copy(my_float_array,my_float_array+SIZE,in_bo_map);

           auto in_buffer = xrt::aie::buffer(device, uuid, "gr.in1");
           in_buffer.sync(in_bo, XCL_BO_SYNC_BO_GMIO_TO_AIE, SIZE * sizeof(float),0);

           // Output buffer in global memory
           auto out_bo  = xrt::aie::bo (device, SIZE * sizeof (float), 0, 0);
           auto out_bo_map = out_bo.map<float *>();

           // Ping-pong buffers for an external buffer port
           auto ext1_bo  = xrt::aie::bo (device, SIZE * sizeof (float), 0, 0);
           auto ext2_bo  = xrt::aie::bo (device, SIZE * sizeof (float), 0, 0);

           auto ping_pong_bo  = xrt::aie::buffer(device, uuid, "gr.ext1");
           ping_pong_bo.sync(ext1_bo, ext2_bo, XCL_BO_SYNC_BO_GMIO_TO_AIE, SIZE * sizeof(float),0);

           auto out_buffer  = xrt::aie::buffer(device, uuid, "gr.out1");
           out_buffer.sync(out_bo, XCL_BO_SYNC_BO_AIE_TO_GMIO, SIZE * sizeof(float),0);

XRT Error API
-------------

In general, XRT APIs can encounter two types of errors:

- **Synchronous errors:** The API may throw an exception that host code can catch and handle.
- **Asynchronous errors:** Failures reported later from the driver, system, or hardware.

XRT provides ``xrt::error`` and related member functions to surface asynchronous errors to user-space host code, which aids debugging.

- ``xrt::error::get_error_code()`` — underlying ``xrtErrorCode`` for the error object (constructed from the device and error class, or from an explicit code and timestamp)
- ``xrt::error::get_timestamp()`` — timestamp associated with that error
- ``xrt::error::to_string()`` — formatted description string for the error object

**Note:** Asynchronous error retrieval is still evolving and currently focuses on AIE-related asynchronous errors. Broader coverage is planned for a future release.

Example code

.. code:: c++
      :number-lines: 41

           graph.run(runIteration);

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



Profiling
---------

In Versal ACAPs with AI Engines, the XRT Profiling class (``xrt::aie::profiling``) and its member functions can be used to configure AI Engine hardware resources for performance profiling and event tracing.

Create Profiling Event
~~~~~~~~~~~~~~~~~~~~~~

The ``xrt::aie::profiling`` constructor creates a profiling object, as shown below.

.. code:: c
      :number-lines: 35

           auto event = xrt::aie::profiling(device);

Use the profiling object to start and stop counters and to read profiling statistics through the profiling APIs.

Start Profiling
~~~~~~~~~~~~~~~

The member function ``xrt::aie::profiling::start()`` is used to start performance counters in AI Engine as per the profiling option passed as an argument. This function configures the performance counters in the AI Engine and starts profiling.


.. code:: c
      :number-lines: 45

           auto graph = xrt::graph(device, xclbin_uuid, "graph_name");
           std::string port1_name = "...";  // PLIO/GMIO port per UG1079
           std::string port2_name = "...";  // PLIO/GMIO port per UG1079
           uint32_t value = 0;               // meaning depends on profiling_option
           event.start(
               xrt::aie::profiling::profiling_option::io_total_stream_running_to_idle_cycles,
               port1_name, port2_name, value);

           // run graph
           ...
           s2mm_run.wait();

Use the same ``xrt::aie::profiling`` object for ``read()`` and ``stop()`` after ``start()``; see ``xrt/xrt_aie.h`` and UG1079 for option and port semantics.

Read Profiling
~~~~~~~~~~~~~~

``xrt::aie::profiling::read()`` returns the current performance counter value for the profiling session on that object.

.. code:: c++
      :number-lines: 35

           uint64_t cycle_count = event.read();

Stop Profiling
~~~~~~~~~~~~~~

The ``xrt::aie::profiling::stop`` function stops the performance profiling associated with the profiling handle and releases the corresponding hardware resources.

.. code:: c
      :number-lines: 35

        event.stop();
        double throughput = output_size_in_bytes / (cycle_count *0.8 * 1e-3);
        // Every AIE cycle is 0.8ns in production board
        std::cout << "Throughput of the graph: " << throughput << " MB/s" << std::endl;
