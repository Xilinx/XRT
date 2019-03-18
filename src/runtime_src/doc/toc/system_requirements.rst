.. _system_requirements.rst:

System Requirements
*******************

Host Platform
=============

1. x86_64
2. PPC64LE
3. AARCH64

Xilinx PCIe Accelerator Card
============================

1. VCU1525
2. Alveo U200
3. Alveo U250
4. Alveo U280
5. AWS F1


Software Platform
=================

XRT and OpenCL runtime require Linux kernel 3.10 and GCC with C++14 features. The stack has been tested on **RHEL/CentOS 7.4, 7.5** and **Ubuntu 16.04.4 LTS, 18.04.1 LTS**. CentOS/RHEL 7.4, 7.5 require additional steps to get C++11 tool set and a few dependent libraries.

Please use the provided script ``src/runtime_src/tools/scripts/xrtdeps.sh`` to install the dependencies for both CentOS/RHEL and Ubuntu distributions.

Additional information for RHEL/CentOS is below.

CentOS/RHEL 7.4, 7.5
--------------------

XRT requires *EPEL 7* and SCL repositories. The included xrtdeps.sh script will attempt to automatically configure the repositories and download the required dependent packages.

Switching to C++14 build environment
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

 scl enable devtoolset-6 bash

Ubuntu 16.04, 18.04
-------------------

Native compiler tool chain supports C++14 features needed by XRT

Build and Install
=================

Refer to :ref:`build.rst`
