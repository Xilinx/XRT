.. _system_requirements.rst:

System Requirements
-------------------

Host Platform for PCIe Accelerator Cards
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. x86_64
2. AARCH64
3. PPC64LE

Supported Xilinx® Accelerator Cards are listed in :ref:`platforms.rst`.


Software Platform for PCIe Accelerator Cards
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XRT and OpenCL runtime require Linux kernel 3.10+ and GCC with C++14 features. 

The stack has been tested on the following OS distributions

1. RHEL/CentOS 7.4 
2. RHEL/CentOS 7.5 
3. RHEL/CentOS 7.6 
4. Ubuntu 16.04.4 LTS
5. Ubuntu 18.04.1 LTS 

CentOS/RHEL 7.4, 7.5, 7.6 require additional steps to install C++14 tool set and a few dependent libraries. Please use the provided script ``src/runtime_src/tools/scripts/xrtdeps.sh`` to install the dependencies for both CentOS/RHEL and Ubuntu distributions. Additional information for RHEL/CentOS is below.

To deploy XRT, simply install
the proper RPM or DEB package obtained from Xilinx.

To build a custom
version of XRT, please follow the instructions in :ref:`build.rst`.


MPSoC Based Embedded Platforms
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For building embedded platforms please refer to :ref:`yocto.rst`.
