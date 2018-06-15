Debug Methodoloy
----------------

Debugging failures on board runs can be a daunting task which often requires *tribal knowledge* to be productive. This document attempts to document the tricks of the trade to help reduce debug cycles for all users.

Tools of the Trade
~~~~~~~~~~~~~~~~~~

1. ``dmesg``
   Capture Linux kernel and driver log
2. ``strace``
   Capture trace of system calls made by a SDAccel application
3. ``xbsak``
   Query status of FPGA device
4. ``gdb``
   Capture stack trace of an XRT application
5. ``xclbinsplit``
   Unpack an xclbin
6. XRT Log
   Run failing application with HAL logging enabled in ``sdaccel.ini`` ::
   [Runtime]
     hal_log="fail.log"

Common Reasons For Failures
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Incorrect Memory Topology Usage
...............................

5.0+ DSAs use sparse connectivity between acceleration kernels and memory controllers (MIGs). This means that a kernel port can only read/write from/to a specific MIG. This connectivity is frozen at xclbin generation time. The host application needs to ensure that it uses the correct memory banks for buffer allocation using cl_mem_ext_ptr_t for OpenCL applications. For XRT native applications the bank is specified in flags to xclAllocBO() and xclAllocUserPtr().

If an applicaiton is producing incorrect results it is important to review the host code to ensure that host application and xclbin agree on memory topology. One way to validate this at runtime is to enable HAL logging in sdaccel.ini and then carefully go through all buffer allocation requests.

Memory Read Without Write
.........................

Read-Without-Write in 5.0+ DSAs will cause MIG ECC error. This is typically and user error. For example if user expects a kernel to write 4KB of data in DDR but it produced only 1KB of data and now the user tries to transfer full 4KB of data to host. It can also happen if user supplied 1KB sized buffer to a kernel but the kernel tries to read 4KB of data. Note ECC errors only occur if since the last bitstream download i.e. MIG initialization no data has been written to a memory location but a read request is made for  that memory location. ECC errors stall the affected MIG. This can manifest in two different ways:

1. Kernel may hang or stall because it does not know how to handle this error while reading/writing to/from the affected MIG. ``xbsak query`` will show that the Kernel is stuck in BUSY state and not making progress.
2. AXI Firewall may trip if PCIe DMA request is made to the affected MIG as the DMA engine will be unable to complete request. AXI Firewall trips result in the Linux kernel driver killing all processes which have opened the device node. ``xbsak query`` will show if an AXI Firewall has indeed tripped including its timestamp.

Incorrect Frequency Scaling
...........................

Incorrect frequency scaling usually indicates a tooling or runtime bug.

HLS Generated Kernel Deadlocks
..............................

Multiple Kernel DDR Access Deadlocks
....................................

Compiler Bugs
.............

Platform Bugs
.............











Writing Good Bug Reports
~~~~~~~~~~~~~~~~~~~~~~~~

1. Output of ``dmesg``
2. Output of ``xbsak query``
3. Output of ``xbsak scan``
4. Application binaries: xclbin, host executable and code, any data files used by the application
5. XRT version
6. DSA name and version
