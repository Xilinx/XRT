XRT/Board Debug FAQ
-------------------

Debugging failures on board runs can be a daunting task which often requires *tribal knowledge* to be effective. This document attempts to document the tricks of the trade to help reduce debug cycles for all users. This is a living document and will be continuously updated.

Tools of the Trade
~~~~~~~~~~~~~~~~~~

``dmesg``
   Capture Linux kernel and driver log
``strace``
   Capture trace of system calls made by a SDAccel application
``xbsak``
   Query status of FPGA device
``gdb``
   Capture stack trace of an XRT application
``xclbinsplit``
   Unpack an xclbin
XRT Log
   Run failing application with HAL logging enabled in ``sdaccel.ini`` ::

     [Runtime]
     hal_log=myfail.log


Common Reasons For Failures
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Incorrect Memory Topology Usage
...............................

5.0+ DSAs use sparse connectivity between acceleration kernels and memory controllers (MIGs). This means that a kernel port can only read/write from/to a specific MIG. This connectivity is frozen at xclbin generation time in specified in mem_topology section. The host application needs to ensure that it uses the correct memory banks for buffer allocation using cl_mem_ext_ptr_t for OpenCL applications. For XRT native applications the bank is specified in flags to xclAllocBO() and xclAllocUserPtr().

If an application is producing incorrect results it is important to review the host code to ensure that host application and xclbin agree on memory topology. One way to validate this at runtime is to enable HAL logging in sdaccel.ini and then carefully go through all buffer allocation requests.

Memory Read Without Write
.........................

Read-Without-Write in 5.0+ DSAs will cause MIG *ECC* error. This is typically and user error. For example if user expects a kernel to write 4KB of data in DDR but it produced only 1KB of data and now the user tries to transfer full 4KB of data to host. It can also happen if user supplied 1KB sized buffer to a kernel but the kernel tries to read 4KB of data. Note ECC read-without-write errors occur if since the last bitstream download -- which results in MIG initialization -- no data has been written to a memory location but a read request is made for that sane memory location. ECC errors stall the affected MIG. This can manifest in two different ways:

1. CU may hang or stall because it does not know how to handle this error while reading/writing to/from the affected MIG. ``xbsak query`` will show that the CU is stuck in *BUSY* state and not making progress.
2. AXI Firewall may trip if PCIe DMA request is made to the affected MIG as the DMA engine will be unable to complete request. AXI Firewall trips result in the Linux kernel driver killing all processes which have opened the device node with *SIGBUS* signal. ``xbsak query`` will show if an AXI Firewall has indeed tripped including its timestamp.

Users should review the host code carefully. Once common example is compression where the size of the compressed data is not known upfront and an application may try to read more data from host than was produced by the kernel.

Incorrect Frequency Scaling
...........................

Incorrect frequency scaling usually indicates a tooling or infrastructure bug. Target frequencies for the dynamic region are frozen at compile time and specified in clock_freq_topology section of xclbin. If clocks in the dynamic region are running at incorrect -- higher than specified -- frequency, kernels will demonstrate weird behavior.

1. Often a CU will produce completely incorrect result with no identifiable pattern
2. A CU might hang
3. When run several times, a CU may produce correct results a few times and incorrect results rest of the time
4. A single CU run may produce a pattern of correct and incorrect result segments. Hence for a CU which produces a very long vector output (e.g. vector add), a pattern of correct -- typically 64 bytes or one AXI burst -- segment followed by incorrect segments are generated.

Users should check the frequency of the board with ``xbsak query`` and compare it against the metadata in xclbin. ``xclbincat`` may be used to extract metadata from xclbin.

HLS Caused CU Deadlocks
.......................

HLS scheduler bugs can also result in CU hangs. CU deadlocks AXI data bus at which point neither read nor write operation can make progress. The deadlocks can be observed with ``xbsak query`` where the CU will appear stuck in *BUSY* state. Note this deadlock can cause other CUs which read/write from/to the same MIG to also hang.

Multiple Kernel DDR Access Deadlocks
....................................

TODO

Compiler Bugs
.............

TODO

Platform Bugs
.............

Bitsream Download Failures
  Bitstream download failures are usually caused because of incompatible xclbins. dmesg log would provide more insight into why the download failed. At OpenCL level they usually manifest as Invalid Binary (error -44).

Incorrect Timing Constraints
  If the platform or dynamic region has invalid timing constraints -- which is really a platform or SDx tool bug -- CUs would show bizarre behaviors. This may result in incorrect outputs or CU/application hangs.

Writing Good Bug Reports
~~~~~~~~~~~~~~~~~~~~~~~~

When creating bug reports please include the following:

1. Output of ``dmesg``
2. Output of ``xbsak query``
3. Output of ``xbsak scan``
4. Application binaries: xclbin, host executable and code, any data files used by the application
5. XRT version
6. DSA name and version
