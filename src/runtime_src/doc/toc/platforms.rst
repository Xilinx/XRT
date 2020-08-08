.. _platforms.rst:


Platform Overview
*****************

XRT exports a common stack across PCIe based platforms and MPSoC based edge platforms.
From user perspective there is very little porting effort when migrating an
application from one class of platform to another.

User Application Compilation
============================

User application is made up of host code written in C/C++/OpenCL or Python. Device code may be written in C/C++/OpenCL or VHDL/Verilog hardware description language.

.. figure:: Alveo-Compilation-Flow.svg
    :figclass: align-center

    User application compilation and execution

Users use Vitis™ compiler, v++ to compile and link device code for the target platform. Host code written in C/C++/OpenCL may be compiled with gcc/g++. Host code may be written in Python OpenCL (using PyOpenCL) or Python XRT (using builti-in python binding).

PCIe Based Platforms
====================

.. image:: XRT-Architecture-PCIe.svg
   :align: center

XRT supports following PCIe based devices:

1. U200
2. U250
3. U280
4. U50
5. AWS F1
6. Advantech VEGA-4000/4002

PCIe based platforms are supported on x86_64, PPC64LE and AARCH64 host architectures. The
platform is comprised of *Shell* and *Dynamic Region*. The Shell (previously known as DSA)
has two physical functions: PF0 also called *mgmt pf* and PF1 also called *user pf*.
Dynamic Region contains *Role* which is user compiled binary. Roles are swapped by user
using process called *Dynamic Function Exchange (DFX)*.

MGMT PF (PF0)
-------------

XRT Linux kernel driver *xclmgmt* binds to management physical function. Management physical function
provides access to Shell components responsible for privileged operations. xclmgmt driver is organized
into subdevices and handles the following functionality:

1.  ICAP programming
2.  Clock scaling
3.  Loading firmware container called dsabin (renamed to xsabin since 2019.2). Dsabin contains RL Shell (for 2 RP solution)
    and embedded Microblaze firmware for ERT and XMC.
4.  Access to in-band sensors: Temperature, Voltage, Current, etc.
5.  AXI Firewall management
6.  Access to flash programmer
7.  Device reset and rescan
8.  Hardware mailbox for communication with xocl driver
9.  Interrupt handling for AXI Firewall and Mailbox
10. Device DNA discovery and validation
11. ECC handling

USER PF (PF1)
-------------

XRT Linux kernel driver *xocl* binds to user physical function. User physical function provides access
to Shell components responsible for non privileged operations. It also provides access to compute units
in DFX partition. xocl driver is organized into subdevices and handles the following functionality:

1.  Device memory topology discovery and memory management
2.  Device memory management as abstracted buffer objects
3.  XDMA memory mapped PCIe DMA engine programming
4.  QDMA streaming DMA engine programming
5.  Multi-process aware context management
6.  Standardized compute unit execution management (optionally with help of ERT) for client processes
7.  Interrupt handling for DMA, Compute unit completion and Mailbox
8.  Buffer object migration between device and host as DMA operation
9.  Queue creation/deletion read/write operation for streaming DMA operation
10. AIO support for the streaming queues
11. Buffer import and export via DMA-BUF
12. PCIe peer-to-peer buffer mapping and sharing
13. Access to in-band sensors via MailBox proxy into xclmgmt
14. Hardware mailbox for communication with xclmgmt driver


PCIe platform security and robustness is described in section :ref:`security.rst`.

Zynq-7000 and ZYNQ Ultrascale+ MPSoC Based Embedded Platforms
=============================================================

.. image:: XRT-Architecture-Edge.svg
   :align: center

XRT supports ZYNQ-7000 and ZYNQ Ultrascale+ MPSoC. User can create their own embedded platforms
 and enable XRT with the steps described :ref:`yocto.rst`. 

`Source code <https://github.com/Xilinx/Vitis_Embedded_Platform_Source>`_ and 
`pre-built <https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/embedded-platforms.html>`_ 
embedded platforms for the following Xilinx evaluation boards are provided:

1. ZC706
2. ZCU102
3. ZCU104

File /etc/xocl.txt needs to be in the root file system so that XRT can know which platform it is running on.

MPSoC based platforms are supported with PetaLinux base stack. XRT Linux kernel
driver *zocl* does the heavy lifting for the embedded platform. It handles the
following functionality

1.  CMA buffer management and cache management
2.  SMMU programming for SVM platforms
3.  Standardized compute unit execution management on behalf of client processes
4.  xclbin download for platforms with Partial Reconfiguration support
5.  Buffer import and export via DMA-BUF
6.  Interrupt handling for compute unit completion
