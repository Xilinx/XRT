.. _hm.rst:

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

Irrespective of the Hugepages settings, ``xbutil host_mem`` command must be used to reserve the host memory for the kernel. This has to be done upfront before the XCLBIN download. In the example below, ``xbutil host_mem`` command is used to reserve 1G, 4G, and 16G host memory respectively for 3 cards.

.. code-block:: bash

  xbutil host_mem -d 0 --enable --size 1G
  xbutil host_mem -d 1 --enable --size 4G
  xbutil host_mem -d 2 --enable --size 16G


Maximum Host memory supported by the platform
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For the platform supporting the host memory access feature, we can observe the following two data from the ``xbutil query`` output

     - **Max HOST_MEM**: The maximum host memory supported by the platform.
     - **HOST_MEM size**: The host memory specified for this card (by ``xbutil host_mem``)

Assuming the platform supported maximum host memory is 16GB, the following output will be observed when the card is configured for 4GB host memory.

.. code-block:: bash

  shell>>xbutil host_mem --enable 4G
  xbutil host_mem done successfully
  shell>>xbutil query
  INFO: Found total 1 card(s), 1 are usable
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  System Configuration
  OS name:	Linux
  Release:	5.7.0+
  Version:	#1 SMP Thu Jun 11 16:19:41 PDT 2020
  Machine:	x86_64
  Model:	Super Server
  CPU cores:	16
  Memory:	15547 MB
  Glibc:	2.23
  Distribution:	Ubuntu 16.04.5 LTS
  Now:		Tue Sep 22 18:30:33 2020 GMT
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  XRT Information
  Version:	2.8.0
  Git Hash:	64ece8bdbd553e0538e99612f11d1926c609a54f
  Git Branch:	ssv3_0921
  Build Date:	2020-09-21 14:25:40
  XOCL:		2.6.0,cd2fcd72498afa91f2a6a60d4e3c1697291cd16e
  XCLMGMT:	2.6.0,cd2fcd72498afa91f2a6a60d4e3c1697291cd16e
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Shell                           FPGA                            IDCode
  xilinx_u250_gen3x16_xdma_shell_2_1                                0x4b57093
  Vendor          Device          SubDevice       SubVendor       SerNum
  0x10ee          0x5005          0x000e          0x10ee
  DDR size        DDR count       Clock0          Clock1          Clock2
  0 Byte          0               300             500             0
  PCIe            DMA chan(bidir) MIG Calibrated  P2P Enabled     OEM ID
  GEN 3x16        2               true            false           0x0(N/A)
  Interface UUID
  8e4e5c27e1d0742bd3d00d65c691a382
  Logic UUID
  abad927204cb200a2e88751e9d582807
  DNA                             CPU_AFFINITY    HOST_MEM size   Max HOST_MEM
                                  0-15            4 GB            16 GB
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


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

In XRT Native APIs the ``xrt::bo`` object should be created with the flag ``XCL_BO_FLAGS_HOST_ONLY`` as shown in the example below

.. code-block:: c++

    auto buffer_in  = xrt::bo(device, size,XCL_BO_FLAGS_HOST_ONLY,kernel.group_id(0)); 
    auto buffer_out = xrt::bo(device, size,XCL_BO_FLAGS_HOST_ONLY,kernel.group_id(1)); 

Also ensure to follow coding guideline as stated below

      - Let XRT allocate the buffer as shown in the above code examples. Do not create a buffer from an already created user-space memory. The host code should map the buffer object to the user-space for read/write operation.
      - Regular data transfer APIs (OpenCL: ``clEnqueueMigramemObjects``/``clEnqueueWriteBuffer``, XRT Native API: ``xrt::bo::sync()``) should be used. Though these API will not do any DMA operation, they are used for Cache Invalidate/Flush as the application works on the Cache memory.
