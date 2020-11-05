XRT ChangeLog
-------------

2.8.0 (202020.2.8.x)
~~~~~~~~~~~~~~~~~~~~

Added
.....

* Support for Ubuntu 20.04 and CentOS/RHEL 8.2 has been added.
* HBM grouping support has been added which allows contiguous banks to be merged into a single group allowing for larger buffer size.
* Support for AIE graph has been added. New AIE APIs are split into AIE array/shim level APIs in ``xrt_aie.h`` and graph level APIs in ``xrt_graph.h``. AIE APIs are moved to ``libxrt_coreutil.so`` from ``libxrt_core.so``.
* pybind11 based Python wrappers have been added for native XRT C++ APIs.
* Support for PCIe Slave Bridge has been added which allows user kernels to directly read/write host memory.
* Support for data driven *two stage* platforms have been added.
* Slimmed down XRT RPM/DEB package dependencies. XRT package does not depend on other **dev/devel** packages anymore.

Removed
.......

* xbsak, please use xbutil


2.7.0 (202010.2.7.x)
~~~~~~~~~~~~~~~~~~~~

Added
.....

* Support for CentOS and RHEL 7.7, 7,8, and 8.1.
* All OS versions now use Python3.
* Native XRT APIs under $XILINX_XRT/include/experimental are subject to change without warning.

Removed
.......

* Removed all references to python2.
* Removed automatic installation of PyOpenCL.


2.6.0 (202010.2.6)
~~~~~~~~~~~~~~~~~~

Added
.....

* XRT native APIs for PL kernel have been added. These APIs are defined in new header file ``xrt_kernel.h``. Please see ``tests/xrt/22_verify/main.cpp`` and ``tests/xrt/02_simple/main.cpp`` for examples. The APIs are also accessible from python. Please see ``tests/python/22_verify/22_verify.py`` and ``tests/python/02_simple/main.py`` for examples.
* Support for data-driven platforms have been added. XRT uses PCIe VSEC to identify data-driven platforms. For these class of platforms XRT uses device tree to discover IPs in the shell and then initialize them.
* Experimental APIs have been added for AIE control for edge platforms. The APIs are defined in header file ``xrt_aie.h``.
* Support for U30 video acceleration offload device has been added.
* Early access versions of next generation utilities, *xbutil* and *xbmgmt* are available. They can be invoked via *--new* switch as ``xbutil --new``.
* Utilties xbutil and xbmgmt now give a warning when they detect an unsupported Linux distribution version and kernel version.
* Error code paths for clPollStreams() API has been improved.


Removed
.......

* Deprecated utilties xclbincat and xclbinsplit have been removed. Please use xclbinutil to work with xclbin files.
* ``xclResetDevice()`` has been marked as deprecated in this release and will be removed in a future release. Please use xbutil reset to reset device.
* ``xclUpgradeFirmware()``, ``xclUpgradeFirmware2()`` and ``xclUpgradeFirmwareXSpi()`` have been marked as deprecated in this release and will be removed in a future release. Please use xbmgmt utility to flash device.
* ``xclBootFPGA()``, ``xclRemoveAndScanFPGA()`` and ``xclRegisterInterruptNotify()`` have been marked as deprecated in this release and will be removed in a future release. These functionalities are no longer supported.
* ``xclLockDevice()`` and ``xclUnlockDevice()`` have been marked as deprecated in this release and will be removed in a future release. These functionalities are no longer supported.
* This is the last release of XMA legacy APIs. Please port your application to XMA2 APIs.

Known Issues
............

* On CentOS the ``xrtdeps.sh`` script used to install required dependencies for building XRT is trying to install no longer supported ``devtoolset-6``.  In order to build XRT on CentOS or RHEL, a later devtoolset version should be installed, for example ``devtoolset-9``.


2.4.0 (202010.2.4)
~~~~~~~~~~~~~~~~~~

Added
.....

* ``xclUnmapBO()`` was added to match ``xclMapBO()``.  This new API should be called when unmapping addresses returned by ``xclMapB()``.  On Linux the API ends up calling POSIX ``munmap()`` but on Windows the implementation is different.

2.3.0 (201920.2.3)
~~~~~~~~~~~~~~~~~~

Added
.....

* ``xclRead()`` and ``xclWrite()`` have been marked as deprecated in this release and will be removed in a future release. For direct register access please use replacement APIs ``xclRegRead()`` and ``xclRegWrite()`` which are more secure and multi-process aware.
* Edge platforms can now use DFX also known as Partial Reconfiguration.
* Support for U50 board has been added to XRT.
* Support for signing xclbins using xclbinutil and validating xclbin signature in xclbin driver has been added to XRT. Please refer to XRT Security documentation https://xilinx.github.io/XRT/2019.2/html/security.html for more details.
* Edge platforms based on MPSoC now support M2M feature via **Zynqmp built-in DMA engine**. M2M for both PCIe and edge platforms can be performed using ``xclCopyBO()`` XRT API or ``clEnqueueCopyBuffers()`` OCL API. Note that the same APIs can also be used to copy buffers between two devices using PCIe peer-to-peer transfer.
* For edge platforms XRT now supports ACC (adapter execution model).
* XRT documentation has been reorganized and significantly updated.
* XRT now natively supports fully virtualized environments where management physical function (PF0) is completely hidden in host and only user physical function (PF1) is exported to the guest. End-user applications based on libxrt_core and xbutil command line utility do not need directly interact with xclmgmt driver. Communication between xocl driver and xclmgmt driver is done over hardware mailbox and MPD/MSD framework. For more information refer to MPD/MSD and Mailbox sections in XRT documentation.
* Management Physical Function (PF0) should now be managed using ``xbmgmt`` utility which is geared towards system adminstrators. ``xbutil`` continues to be end-user facing utility.
* Support has been added for device memory only buffer with no backing shadow buffer in host on PCIe platforms. To allocate such buffers use ``XCL_BO_FLAGS_DEV_ONLY`` in flags field of xclAllocBO() or ``CL_MEM_HOST_NO_ACCESS`` in flags field of OCL API.
* XRT now has integrated support for Linux hwmon. Run Linux sensors utility to see all the sensor values exported by Alveo/XRT.
* XRT now has production support for edge platforms. The following non DFX platforms edge platforms are supported: zcu102_base, zcu104_base, zc702, zc706. In addition zcu102_base_dfx platform has DFX support.
* Emulation and HW profiling support has been enabled for all the above mentioned edge platforms. Zynq MPSoC platforms: zcu102_base, zcu104_base and zcu102_base_dfx also has emulation profiling enabled.
* Improved handling of PCIe reset via ``xbutil reset`` which resolves system crash observed on some servers.
* Resource management has been moved out of XMA library.
* Only signed xclbins can be loaded on systems running in UEFI secure boot mode. You can use DKMS key used to sign XRT drivers to sign xclbins as well. As root please use the following command to sign xclbin with DKMS UEFI key--
  ``xclbinutil --private-key /var/lib/shim-signed/mok/MOK.priv --certificate /var/lib/shim-signed/mok/MOK.der --input a.xclbin --output signed.xclbin``


Known Issue
...........

* On U280 Platform, downloading XCLBIN is going to reset P2P BAR size back to 256M internally. XRT workaround this issue by reading BAR size register and writing back the same value. This sets the P2P BAR size back to the value before downloading XCLBIN.
* On edge platforms intermittent hang is observed when downloading different xclbins multiple times while CU interrupt is enabled.
* Dynamic clock scaling is not enabled for edge platforms.
* On PPC64LE ``xbutil reset`` uses PCIe fundamental reset effectively reloading the platform from PROM. Note on x86_64 ``xbutil reset`` continues to use PCIe warm reset which just resets the shell and the dynamic region without reloading the platform from PROM.

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
