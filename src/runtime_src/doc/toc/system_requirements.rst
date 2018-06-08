System Requirements
-------------------

Host Platform
~~~~~~~~~~~~~

1. x86_64
2. AARCH64
3. PPC64LE

Xilinx Accelerator Card
~~~~~~~~~~~~~~~~~~~~~~~

1. VCU1525 (xilinx_vcu1525_dynamic_5_1)
2. KCU1500 (xilinx_kcu1500_dynamic_5_0)
3. AWS F1 (xilinx_aws-vu9p-f1_dynamic_5_0)

Software Platform
~~~~~~~~~~~~~~~~~

XRT and OpenCL runtime require Linux kernel 3.10 and GCC with C++11 features. The stack has been tested on RHEL/CentOS 7.3 and Ubuntu 16.04.3 LTS. CentOS/RHEL 7.3 require additional steps to get C++11 tool set. Detailed dependencies and instructions are below.

CentOS/RHEL 7.3
...............

Please install *EPEL 7* by following instructions at https://fedoraproject.org/wiki/EPEL
Then following packages should be installed with ``sudo yum install``.

 * ocl-icd
 * ocl-icd-devel
 * opencl-headers
 * kernel-headers
 * kernel-devel
 * gcc-c++
 * gcc
 * gdb
 * libstdc++-static
 * make
 * opencv
 * libjpeg-turbo-devel
 * libpng12-devel
 * libtiff-devel
 * compat-libtiff3
 * python
 * git
 * dmidecode
 * pciutils
 * strace
 * perl
 * boost-devel
 * boost-filesystem
 * gnuplot
 * cmake
 * lm_sensors
 * unzip
 * redhat-lsb
 * kernel-headers-$(uname -r)

Installing C++11 build tools on CentOS 7.X
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

 sudo yum install centos-release-scl (CentOS)
 sudo yum install devtoolset-6

Installing C++11 build tools on RHEL 7.X
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

 sudo yum-config-manager --enable rhel-server-rhscl-7-rpms
 sudo yum install devtoolset-6

Switching to C++11 build environment
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

 scl enable devtoolset-6 bash

Ubuntu 16.04.3 LTS
..................

The following packages should be installed with ``sudo apt-get install``.

 * ocl-icd-libopencl1
 * opencl-headers
 * ocl-icd-opencl-dev
 * linux-headers
 * linux-libc-dev
 * g++
 * gcc
 * gdb
 * make
 * libopencv-core
 * opencv
 * libjpeg-dev
 * libpng-dev
 * libtiff5-dev
 * python
 * git
 * dmidecode
 * pciutils
 * strace
 * perl
 * libboost-dev
 * libboost-filesystem-dev
 * gnuplot
 * cmake
 * lm-sensors
 * lsb
 * unzip
 * linux-headers-$(uname -r)
 * python3-sphinx-rtd-theme
 * sphinx-common
 * python3-sphinx
