XRT/Board Debug FAQ
-------------------

Debugging failures on board runs can be a daunting task which often requires *tribal knowledge* to be effective. This document attempts to document the tricks of the trade to help reduce debug cycles for all users. This is a living document and will be continuously updated.

Tools of the Trade
~~~~~~~~~~~~~~~~~~

``dmesg``
   Capture Linux kernel and XRT drivers log
``strace``
   Capture trace of system calls made by an XRT application
``gdb``
   Capture stack trace of an XRT application
``lspci``
   Enumerate Xilinx PCIe devices
``xbutil``
   Query status of Xilinx PCIe device
``xclbinsplit``
   Unpack an ``xclbin``
XRT API Trace
   Run failing application with HAL logging enabled in ``sdaccel.ini`` ::

     [Runtime]
     hal_log=myfail.log

Validating a Working Setup
~~~~~~~~~~~~~~~~~~~~~~~~~~

When observing an application failure on a board, it is important to step back and validate the board setup. That will help establish and validate a clean working environment before running the failing application. We need to ensure that the board is enumerating and functioning.

Board Enumeration
  Check if BIOS and Linux can see the board. So for Xilinx boards use ``lspci`` utility ::

    lspci -v -d 10ee:

  Check if XRT can see the board and reports sane values ::

    xbutil scan
    xbutil query

DSA Sanity Test
  Check if verify kernel works ::

    cd test
    ./verify.exe verify.xclbin

  Check DDR and PCIe bandwidth ::

    xbutil dmatest

Common Reasons For Failures
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Incorrect Memory Topology Usage
...............................

5.0+ DSAs are considered dynamic platforms which use sparse
connectivity between acceleration kernels and memory controllers
(MIGs). This means that a kernel port can only read/write from/to a
specific MIG. This connectivity is frozen at ``xclbin`` generation
time in specified in ``mem_topology`` section of ``xclbin``. The host
application needs to ensure that it uses the correct memory banks for
buffer allocation using ``cl_mem_ext_ptr_t`` for OpenCL
applications. For XRT native applications the bank is specified in
flags to ``xclAllocBO()`` and ``xclAllocUserPtr()``.

If an application is producing incorrect results it is important to
review the host code to ensure that host application and ``xclbin``
agree on memory topology. One way to validate this at runtime is to
enable HAL logging in ``sdaccel.ini`` and then carefully go through
all buffer allocation requests.

Memory Read Before Write
........................

Read-Before-Write in 5.0+ DSAs will cause MIG *ECC* error. This is typically a user error. For example if user expects a kernel to write 4KB of data in DDR but it produced only 1KB of data and now the user tries to transfer full 4KB of data to host. It can also happen if user supplied 1KB sized buffer to a kernel but the kernel tries to read 4KB of data. Note ECC read-before-write error occurs if — since the last bitstream download which results in MIG initialization — no data has been written to a memory location but a read request is made for that same memory location. ECC errors stall the affected MIG since usually kernels are not able to handle this error. This can manifest in two different ways:

1. CU may hang or stall because it does not know how to handle this error while reading/writing to/from the affected MIG. ``xbutil query`` will show that the CU is stuck in *BUSY* state and not making progress.
2. AXI Firewall may trip if PCIe DMA request is made to the affected MIG as the DMA engine will be unable to complete request. AXI Firewall trips result in the Linux kernel driver killing all processes which have opened the device node with *SIGBUS* signal. ``xbutil query`` will show if an AXI Firewall has indeed tripped including its timestamp.

Users should review the host code carefully. One common example is compression where the size of the compressed data is not known upfront and an application may try to migrate more data to host than was produced by the kernel.

Incorrect Frequency Scaling
...........................

Incorrect frequency scaling usually indicates a tooling or
infrastructure bug. Target frequencies for the dynamic (partial
reconfiguration) region are frozen at compile time and specified in
``clock_freq_topology`` section of ``xclbin``. If clocks in the dynamic region
are running at incorrect — higher than specified — frequency,
kernels will demonstrate weird behavior.

1. Often a CU will produce completely incorrect result with no identifiable pattern
2. A CU might hang
3. When run several times, a CU may produce correct results a few times and incorrect results rest of the time
4. A single CU run may produce a pattern of correct and incorrect result segments. Hence for a CU which produces a very long vector output (e.g. vector add), a pattern of correct — typically 64 bytes or one AXI burst — segment followed by incorrect segments are generated.

Users should check the frequency of the board with ``xbutil query``
and compare it against the metadata in ``xclbin``. ``xclbincat`` may
be used to extract metadata from ``xclbin``.

CU Deadlock
...........

HLS scheduler bugs can also result in CU hangs. CU deadlocks AXI data bus at which point neither read nor write operation can make progress. The deadlocks can be observed with ``xbutil query`` where the CU will appear stuck in *START* or *---* state. Note this deadlock can cause other CUs which read/write from/to the same MIG to also hang.

Multiple CU DDR Access Deadlock
...............................

TODO

AXI Bus Deadlock
................

AXI Bus deadlocks can be caused by `Memory Read Before Write`_, `CU Deadlock`_ or `Multiple CU DDR Access Deadlock`_ described above. These usually show up as CU hang and sometimes may cause AXI FireWall to trip. Run ``xbutil query`` to check if CU is stuck in *START* or *--* state or if one of the AXI Firewall has tripped. If CU seems stuck we can confirm the deadlock by running ``xbutil status`` which should list and performance counter values. Optionally run ``xbutil dmatest`` which will force transfer over the deadlocked bus causing either DMA timeout or AXI Firewall trip.


Platform Bugs
.............

Bitsream Download Failures
  Bitstream download failures are usually
  caused because of incompatible ``xclbin``\ s. ``dmesg`` log would
  provide more insight into why the download failed. At OpenCL level
  they usually manifest as Invalid Binary (error -44).

  Rarely MIG calibration might fail after bitstream download. This
  will also show up as bitstream download failure. Usually XRT driver
  messages in ``dmesg`` would reveal if MIG calibration failed.

Incorrect Timing Constraints
  If the platform or dynamic region has invalid timing constraints — which is really a platform or SDx tool bug — CUs would show bizarre behaviors. This may result in incorrect outputs or CU/application hangs.

Board in Crashed State
~~~~~~~~~~~~~~~~~~~~~~

When board is in crashed state PCIe read operations start returning
``0XFF``. In this state ``xbutil`` query would show bizarre
metrics. For example ``Temp`` would be very high. Boards in crashed state
may be recovered with PCIe hot reset ::

  xbutil reset -h

If this does not recover the board perform a warm reboot. After reset/reboot please follow steps in `Validating a Working Setup`_

XRT Scheduling Options
~~~~~~~~~~~~~~~~~~~~~~

XRT has three kernel execution schedulers today: ERT, KDS and
legacy. By default XRT uses ERT which runs on Microblaze. ERT is
accessed through KDS which runs inside ``xocl`` Linux kernel
driver. If ERT is not available KDS uses its own built-in
scheduler. From 2018.2 release onward KDS (together with ERT if
available in the DSA) is enabled by default. Users can optionally
switch to legacy scheduler which runs in userspace. Switching
scheduler will help isolate any scheduler related XRT bugs ::

  [Runtime]
  ert=false
  kds=false

Writing Good Bug Reports
~~~~~~~~~~~~~~~~~~~~~~~~

When creating bug reports please include the following:

1. Output of ``dmesg``
2. Output of ``xbutil query``
3. Output of ``xbutil scan``
4. Application binaries: xclbin, host executable and code, any data files used by the application
5. XRT version
6. DSA name and version
