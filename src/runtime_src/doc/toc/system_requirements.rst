.. _system_requirements.rst:

System Requirements
-------------------

Host Platform for PCIe Accelerator Cards
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. x86_64
2. AARCH64
3. PPC64LE

Supported Xilinx® Accelerator Cards are listed in `platforms.rst <platforms.rst>`_.


Software Platform for PCIe Accelerator Cards
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XRT and OpenCL runtime require Linux kernel 3.10+ and GCC with C++14 features. The stack has been tested on RHEL/CentOS 7.4, 7.5, 7.6 and Ubuntu 16.04.4 LTS, 18.04.1 LTS. CentOS/RHEL 7.4, 7.5, 7.6 require additional steps to install C++14 tool set and a few dependent libraries. Please use the provided script ``src/runtime_src/tools/scripts/xrtdeps.sh`` to install the dependencies for both CentOS/RHEL and Ubuntu distributions. Additional information for RHEL/CentOS is below.


CentOS/RHEL 7.4, 7.5, 7.6
.........................

XRT requires *EPEL 7* and SCL repositories. The included xrtdeps.sh script will attempt to automatically configure the repositories and download the required dependent packages from standard RHEL/CentOS repositories.

Switching to C++14 build environment
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

 scl enable devtoolset-6 bash


MPSoC Based Embedded Platforms
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For building embedded platforms please refer to `yocto.rst <yocto.rst>`_.
