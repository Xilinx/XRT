.. _sb.rst:

Slave-Bridge Support
====================

Slave Bridge IP is used by the kernel(s) to read and write data directly from the Host Memory. Unlike the XDMA data transfer, this data transfer mechanism does not utlize global memories (DDR, HBM, PLRAM ,etc) on the card. Slave-Bridge provides DMA bypass capability that is primarily used for data transfer on a No-DMA platform. 


Kernel Compilation
------------------

Use the following V++ configuration option to connect a kernel AXI-Master Port to Slave-Bridge IP. 

.. code-block:: 

   [connectivity]
   ## Syntax
   ##sp=<cu_name>.<axi_master_port>:HOST[0]
   sp=my_kernel_1.m_axi_gmem:HOST[0]
   
   
Host Server Setup
-----------------

To enable host memory access functionality the following settings are required from the host

Hugepage Requirement
~~~~~~~~~~~~~~~~~~~~

If the kernel requirement of the Host Memory is more than 1GB, XRT allocates multiple Hugepages from the host memory. These Hugepages are internally remapped (inside the FPGA shell) so that kernel can see a large contiguous bank like memory. 


**Steps required to enable Hugepages (Linux)**
   
   1. Modify grub setup by changing the following line of the file ``/etc/default/grub``
  
         `GRUB_CMDLINE_LINUX_DEFAULT="splash quiet noresume hugepagesz=1G hugepages=4"`
    
   2. shell>update-grub

   3. Reboot the server
  
   4. Verify the HugePage setting
   
   shell>hugeadm --pool-list

  .. code-block:: 

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

.. code-block:: 

  xbutil host_mem -d 0 --enable --size 1G
  xbutil host_mem -d 1 --enable --size 4G
  xbutil host_mem -d 2 --enable --size 16G


Maximum Host memory supported by the platform
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For the platform supporting Slave-Bridge we can observe the following two data from a ``xbutil query`` output 

     - **Max HOST_MEM**: The maximum host memory supported by the platform. 
     - **HOST_MEM size**: The host memory specified for this card (by ``xbutil host_mem``) 
     
Assuming the platform supported maximum host memory is 16GB, the following output will be observed when the card is configured for 4GB host memory. 

.. code-block::
  
  shell>>xbutil host_mem --enable 4G 
  xbutil host_mem done successfully

  shell>>xbutil query
  
  ...........    .............   ............  
  ...........    .............   ............             
                 HOST_MEM size   Max HOST_MEM  
                 4 GB            16 GB


OpenCL Host code Guideline
--------------------------

XRT OpenCL introduces a new buffer extension Flag ``XCL_MEM_EXT_HOST_ONLY`` that should be used to denote a Host-only buffer 

.. code-block:: c++
 
    cl_mem_ext_ptr_t host_buffer_ext;
    host_buffer_ext.flags = XCL_MEM_EXT_HOST_ONLY;
    host_buffer_ext.obj = NULL;
    host_buffer_ext.param = 0;
 
    cl::Buffer buffer_in (context,CL_MEM_READ_ONLY |CL_MEM_EXT_PTR_XILINX, size, &host_buffer_ext);
    cl::Buffer buffer_out(context,CL_MEM_WRITE_ONLY |CL_MEM_EXT_PTR_XILINX, size, &host_buffer_ext);
    
  
Follow coding guideline as dictated below
  
      - Use ``XCL_MEM_EXT_HOST_ONLY`` extension for Buffer declaration (as per the above example) 
      - Do not use ``CL_MEM_USE_HOST_PTR`` for creating a host-only buffer
      - Buffer should mapped to the user-space ``clEnqueueMapBuffer`` for Read/Write
      - Regular OpenCL data transfer APIs ``clEnqueueMigramemObjects``, ``clEnqueueWriteBuffer`` etc should be used. Though these API will not do any DMA operation, but they are used for Cache Invalidate/Flush as the application works on the Cache memory. 
      
      
