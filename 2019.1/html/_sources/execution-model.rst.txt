.. _execution-model.rst:

Execution Model Overview
************************

Introduction
============
Xilinx FPGA based acceleration platform architecture are described in the :ref:`platforms.rst` document. On *Alveo PCIe* platforms *xocl* driver binds to user physical function and *xclmgmt* driver binds to management physical function. The ioctls exported by xocl are described in :ref:`xocl_ioctl.main.rst` document and ioctls exported by xclmgmt are described in :ref:`mgmt-ioctl.main.rst` document. On *Zynq Ultrascale+ MPSoC* platforms *zocl* driver binds to the accelerator. The ioctls exported by zocl are listed here TODO.

Image Download
==============

Xilinx SDx compiler xocc compiles user's device code into xclbin file which contains FPGA bitstream and collection of metadata like memory topology, IP instantiations, etc. xclbin format is defined in :ref:`formats.rst` document. For Alveo platforms xclmgmt driver provides an ioctl for xclbin download. For Zynq Ultrascale+ MPSoC zocl provides an ioctl for xclbin download. Both drivers support FPGA Manager integration. The drivers walk the xclbin sections, program the FPGA fabric, discover the memory topology, initialize the memory managers for the provided memory topology and discover user's compute units programmed into the FPGA fabric.

Memory Management
=================

Both PCIe based and embedded platforms use a unified multi-thread/process capable memory management API defined in :ref:`xrt.main.rst` document.

For both class of platforms, memory management is performed inside Linux kernel driver. Both drivers use DRM GEM for memory management which includes buffer allocator, buffer mmap support, reference counting of buffers and DMA-BUF export/import. These operations are made available via ioctls exported by the drivers.

xocl
----

Xilinx PCIe platforms like Alveo PCIe cards support various memory topologies which can be dynamically loaded as part of FPGA image loading step. This means from one FPGA image to another the device may expose one or more memory controllers where each memory controller has its own memory address range. We use drm_mm for allocation of memory and drm_gem framework for mmap handling. Since ordinarily our device memory is not exposed to host CPU (except when we enable PCIe peer-to-peer feature) we use host memory pages to back device memory for mmap support. For syncing between device memory and host memory pages XDMA PCIe DMA engine is used. Users call sync ioctl to effect DMA in requested direction.

zocl
----

Xilinx embedded platforms like Zynq Ultrascale+ MPSoC support various memory topologies as well. In addition to memory shared between PL (FPAG fabric) and PS (ARM A-53) we can also have dedicated memory for PL using a soft memory controller that is instantiated in the PL itself. zocl supports both CMA backed memory management where accelerators in PL use physical addresses and SVM based memory management -- with the help of ARM SMMU -- where accelerators in PL use virtual addresses also shared with application running on PS.

Execution Management
====================

Both xocl and zocl support structured execution framework. After xclbin has been loaded by the driver compute units defined by the xclbin are live and ready for execution. The compute units are controlled by driver component called Kernel Domain Scheduler (KDS). KDS queues up execution tasks from client processes via ioctls and then schedules them on available compute units. Both drivers export an ioctl for queuing up execution tasks.

User space submits execution commands to KDS in well defined command packets. The commands are defined in :ref:`ert.main.rst`

KDS notifies user process of a submitted execution task completion asynchronously via POSIX poll mechanism. On PCIe platforms KDS leverages hardware scheduler running on Microblaze soft processor for fine control of compute units. Compute units use interrupts to notify xocl/zocl when they are done. KDS also supports polling mode where KDS actively polls the compute units for completion instead of relying on interrupts from compute units.

On PCIe platforms hardware scheduler (referred to above) runs firmware called Embedded Runtime (ERT). ERT receives requests from KDS on hardware out-of-order Command Queue with upto 128 command slots. ERT notifies KDS of work completion by using bits in Status Register and MSI-X interrupts. ERT source code is also included with XRT source on GitHub.

Board Management
================

For Alveo boards xclmgmt does the board management like board recovery in case compute units hang the data bus, sensor data collection, AXI Firewall monitoring, clock scaling, power measurement, loading of firmware files on embedded soft processors like ERT and CMC.

Execution Flow
==============

1. Load xclbin using DOWNLOAD ioctl
2. Discover compute unit register map from xclbin
3. Allocate data buffers to feed to the compute units using CREATE_BO/MAP_BO ioctl calls
4. Migrate input data buffers from host to device using SYNC_BO ioctl
5. Allocate an execution command buffer using CREATE_BO/MAP_BO ioctl call and fill the command buffer using data in 2 above and following the format defined in ert.h
6. Submit the execution command buffer using EXECBUF ioctl
7. Wait for completion using POSIX poll
8. Migrate output data buffers from device to host using SYNC_BO ioctl
9. Release data buffers and command buffer
