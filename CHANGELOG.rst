XRT ChangeLog
-------------

2.2.0 (201910.2.2)
~~~~~~~~~~~~~~~~~~

Added
.....

* Production support for *QDMA* (Xilinx PCIe Streaming DMA) engine has been added to XRT. Applications can use Xilinx streaming extension APIs defined in cl_ext_xilinx.h to work with streams on QDMA platforms like xilinx_u200_qdma_201910_1. Look for examples on https://github.com/Xilinx/SDAccel_Examples.
* *PCIe peer-to-peer* functionality is fully supported. Please consult https://xilinx.github.io/XRT/2019.1/html/p2p.html for details on how to setup PCIe peer-to-peer BAR and host system requirements. P2P buffers are created by passing ``XCL_MEM_EXT_P2P_BUFFER`` flag to ``clCreateBuffer()`` API. Peer PCIe devices like NVMe can directly DMA from/to P2P buffers. P2P transfers between two Alveo™ boards can be triggered through standard ``clEnqueueCopyBuffers()`` API.
* Support has been added for *AP_CTRL_CHAIN* (data-flow) and *AP_CTRL_NONE* (streaming) execution models. XRT scheduler (including hardware accelerated ERT) have been updated to handle the new execution models. xclbin tools have been updated to annotate xclbin IP_LAYOUT entries with suitable tags to pass the execution model information to XRT.
* *Memory to memory (M2M)* hardware accelerated transfers from one DDR bank to another within a device can be effected on platforms with M2M IP via standard ``clEnqueueCopyBuffer()``
* XRT now looks for ``xrt.ini`` configuration file and if not found looks for legacy sdaccel.ini configuration file. If not found in usual search directories the files are now also searched in working directory.
* Embedded platforms based on Zynq MPSoC US+™ are fully supported. For reference designs please explore reVISION™ stack from Xilinx. Embedded platforms now use interrupts for CU completion notification, significantly reducing ARM CPU usage.
* Profiling support has been extended to embedded platforms with timeline trace and profile summary.
* XRT now makes no assumption about CU base addresses on embedded platforms. CU base addresses can be completely floating and are discovered from ``IP_LAYOUT`` section of xclbin.
* XMA (Xilinx Media Accelerator) is now fully integrated into XRT by using the common config reader and messaging framework (also shared by OCL) provided by XRT core.
* XMA uses XRT core framework for scheduling tasks on encoder/decoder/scaler. New XMA APIs provide a method to prepare register write command packet, send the write command to XRT and then wait for completion of one or more command submissions. Please look at https://github.com/Xilinx/xma-samples for recommended way to write XMA plugins and design video IP control interface.
* Multiple process mode is on by default in this release. This means multiple user processes can simultaneously use the same CU on a board. XRT does time division multiplexing. Note there is no support for pre-emption. In multi-process run only the first process gets profiling support.
* OCL can perform automatic binding of cl_mem to DDR bank by using several heuristics like kernel argument index and kernel instance information. The API ``clCreateKernel`` is enhanced to accept annotated CU name(s) to fetch asymmetrical compute units (If all the CUs of a kernel have exact same port maps or port connections they are symmetrical compute units, otherwise CUs are asymmetrical) and streaming compute units.
* XRT will give error if it cannot identify the buffer location (in earlier releases it used to assume a default location). Remedies: a) Check kernel XCLBIN to make sure kernel argument corresponding to the buffer is mapped to device memory properly b) Use ``clSetKernelArg`` before any enqueue operation on buffer
* Host applications directly linking with libxilinxopencl.so must use ``-Wl,-rpath-link,$(XILINX_XRT)/lib`` in the linker line. Host applications linking with ICD loader, libOpenCL.so do not need to change.
* ``xbutil top`` now reports live CU usage metric.
* ``xclbincat`` and ``xclbinsplit`` are deprecated by ``xclbinutil``.  These deprecated tools are currently scheduled to be obsoleted in the next release.
* Profiling subsystem has been enhanced to show dataflow, PCIe peer to peer transfers, M2M transfers and kernel to kernel streaming information.
* XRT has switched to new header file ``xrt.h`` in place of ``xclhal2.h``. The latter is still around for backwards compatibility but hash includes xrt.h for all definitions. A new file ``xrt-next.h`` has been added for experimental features.


2.1.0 (201830.2.1)
~~~~~~~~~~~~~~~~~~

Added
.....

* xbutil can now generate output in JSON format for easy parsing by other tools. Use ``xbutil dump`` to generate JSON output on stdout.
* Initial support for PCIe peer-to-peer transactions has been added. Please consult https://xilinx.github.io/XRT/2018.3/html/p2p.html for details.
* 64-bit BARs in Alveo shells are natively supported.
* Initial implementation of XRT logging API, xclLogMsg() for use by XRT clients.
* Initial support for Alveo shell KDMA feature in OpenCL.
* Yocto recipes to build XRT for embedded platforms. Please consult https://xilinx.github.io/XRT/2018.3/html/yocto.html for details.


Fixed
.....

* ``xbutil flash -a`` PROM corruption issue with multiple Alveo boards.
* XRT scheduling bug with multiple boards on AWS F1 when scheduler was serializing board access.
* xocl kernel driver bugs in handling multiple processes accessing the same device.
* PPC64LE build failure.
* Several core QDMA driver fixes.
* xocl scheduler thread now yields correctly when running in polling mode.
* Several Coverity/Fortify code scan fixes.

Deprecated
..........

* XMA plugin API xma_plg_register_write has been marked for deprecation. It will be removed in a future release.
* XMA plugin API xma_plg_register_read has been marked for deprecation. It will be removed in a future release.
