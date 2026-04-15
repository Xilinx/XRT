.. _hm.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.

Host Memory Access
==================

Some of the recent Alveo cards support direct host memory access by the kernel(s) to read/write data directly from/to the host memory. Unlike the XDMA data transfer, this data transfer mechanism does not utilize global memories (DDR, HBM, PLRAM, etc) on the card. This feature provides DMA bypass capability that is primarily used for data transfer on a No-DMA platform.


Kernel Compilation
------------------

Use the following V++ configuration option to configure kernel port's connectivity to allow drive data through the AXI bridge to the host memory.

.. code-block:: bash

   [connectivity]
   ## Syntax
   sp=my_kernel_1.m_axi_gmem:HOST[0]


Host Server Setup
-----------------

To enable host memory access functionality the following settings are required from the host

Hugepage Requirement
~~~~~~~~~~~~~~~~~~~~

If the kernel requirement of the host memory is more than 1GB, XRT allocates multiple Hugepages from the host memory. These hugepages are internally remapped (inside the FPGA shell) so that kernel can see a large contiguous bank-like memory.


**Steps required to enable Hugepages (Linux)**

   1. Modify grub setup by changing the following line of the file ``/etc/default/grub``

         `GRUB_CMDLINE_LINUX_DEFAULT="splash quiet noresume hugepagesz=1G hugepages=4"`

   2. shell>update-grub

   3. Reboot the server

   4. Verify the HugePage setting

.. code-block:: bash

   shell>hugeadm --pool-list

       Size         Minimum  Current  Maximum  Default
       2097152        0        0        0        *
       1073741824     4        4        4


The following table can be used to determine the number of Hugepages required based on the host memory requirement

+-------------------------+-----------------------------+
|  Host Mem Requirement   |      Huge Page Setting      |
+=========================+=============================+
|    M <= 1GB             | Not Required                |
+-------------------------+-----------------------------+
|   M>1GB and M<=2GB      | No of 1G HugePages = 2      |
+-------------------------+-----------------------------+
|   M>2GB and M<=4GB      | No of 1G HugePages = 4      |
+-------------------------+-----------------------------+
|   M>4GB and M<=8GB      | No of 1G HugePages = 8      |
+-------------------------+-----------------------------+
|   M>8GB and M<=16GB     | No of 1G HugePages = 16     |
+-------------------------+-----------------------------+

Enabling the Host Memory by XRT
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Irrespective of the Hugepages settings, ``xbutil configure --host-mem`` command must be used to reserve the host memory for the kernel. This has to be done upfront before the XCLBIN download. In the example below, ``sudo xbutil configure --host-mem -d <bdf>`` command is used to reserve 1G, 4G, and 16G host memory respectively for 3 cards.

.. code-block:: bash

  sudo xbutil configure --host-mem -d 0000:a6:00.1 --size 1G enable
  sudo xbutil configure --host-mem -d 0000:73:00.1 --size 4G enable
  sudo xbutil configure --host-mem -d 0000:17:00.1 --size 16G enable


Maximum Host memory supported by the platform
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For the platform supporting the host memory access feature, we can observe the following two data from the ``xbutil examine -r pcie-info -d <bdf>`` output

     - **Max Shared Host Memory**: The maximum host memory supported by the platform.
     - **Shared Host Memory**: The host memory specified for this card (by ``xbutil configure --host-mem``)

Assuming the platform supported maximum host memory is 16GB, the following output will be observed when the card is configured for 1GB host memory, no xclbin loaded.

.. code-block:: bash

  shell>>sudo xbutil configure --host-mem -d 0000:17:00.1 --size 1G enable

  Host-mem enabled successfully
  shell>>xbutil examine -r pcie-info -d 0000:17:00.1

  -----------------------------------------------
  1/1 [0000:a6:00.1] : xilinx_u250_gen3x16_xdma_shell_3_1
  -----------------------------------------------
  Pcie Info
    Vendor                 : 0x10ee
    Device                 : 0x5005
    Sub Device             : 0x000e
    Sub Vendor             : 0x10ee
    PCIe                   : Gen3x16
    DMA Thread Count       : 2
    CPU Affinity           : 16-31,48-63
    Shared Host Memory     : 1 GB
    Max Shared Host Memory : 0 Byte
    Enabled Host Memory    : 0 Byte

When you load an xclbin with the host mem support, the ``Max Shared Host Mem`` gets populated.

.. code-block:: bash

  shell>>xbutil examine -r pcie-info -d 0000:17:00.1

  -----------------------------------------------
  1/1 [0000:a6:00.1] : xilinx_u250_gen3x16_xdma_shell_3_1
  -----------------------------------------------
  Pcie Info
    Vendor                 : 0x10ee
    Device                 : 0x5005
    Sub Device             : 0x000e
    Sub Vendor             : 0x10ee
    PCIe                   : Gen3x16
    DMA Thread Count       : 2
    CPU Affinity           : 16-31,48-63
    Shared Host Memory     : 1 GB
    Max Shared Host Memory : 16 GB
    Enabled Host Memory    : 0 Byte

Finally, when you run an application which exercises HOST[0], ``Enabled Host Memory`` is populated.

.. code-block:: bash

  shell>>xbutil examine -r pcie-info -d 0000:17:00.1

  -----------------------------------------------
  1/1 [0000:a6:00.1] : xilinx_u250_gen3x16_xdma_shell_3_1
  -----------------------------------------------
  Pcie Info
    Vendor                 : 0x10ee
    Device                 : 0x5005
    Sub Device             : 0x000e
    Sub Vendor             : 0x10ee
    PCIe                   : Gen3x16
    DMA Thread Count       : 2
    CPU Affinity           : 16-31,48-63
    Shared Host Memory     : 1 GB
    Max Shared Host Memory : 16 GB
    Enabled Host Memory    : 1 GB

Host code Guideline
-------------------

XRT OpenCL introduces a new buffer extension Flag ``XCL_MEM_EXT_HOST_ONLY`` that should be used to denote a Host-only buffer

.. code-block:: c++

    cl_mem_ext_ptr_t host_buffer_ext;
    host_buffer_ext.flags = XCL_MEM_EXT_HOST_ONLY;
    host_buffer_ext.obj = NULL;
    host_buffer_ext.param = 0;

    cl::Buffer buffer_in (context,CL_MEM_READ_ONLY |CL_MEM_EXT_PTR_XILINX, size, &host_buffer_ext);
    cl::Buffer buffer_out(context,CL_MEM_WRITE_ONLY |CL_MEM_EXT_PTR_XILINX, size, &host_buffer_ext);

In XRT Native APIs the ``xrt::bo`` object should be created with the flag ``xrt::bo::flags::host_only`` as shown in the example below

.. code-block:: c++

    xrt::bo buffer_in (device, size, xrt::bo::flags::host_only, kernel.group_id(0)); 
    xrt::bo buffer_out(device, size, xrt::bo::flags::host_only, kernel.group_id(1)); 

Also ensure to follow coding guideline as stated below

      - Let XRT allocate the buffer as shown in the above code examples. Do not create a buffer from an already created user-space memory. The host code should map the buffer object to the user-space for read/write operation.
      - Regular data transfer APIs (OpenCL: ``clEnqueueMigramemObjects``/``clEnqueueWriteBuffer``, XRT Native API: ``xrt::bo::sync()``) should be used. Though these API will not do any DMA operation, they are used for Cache Invalidate/Flush as the application works on the Cache memory.
