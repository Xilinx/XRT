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

The stack is tested on RHEL/CentOS and Ubuntu. For the detailed list of supported OS, please refer to the specific version of `UG1451 XRT Release Notes <https://www.xilinx.com/search/site-keyword-search.html#q=ug1451>`_. 


   CentOS/RHEL 7.4, 7.5, 7.6 require additional steps to install C++14 tool set and a few dependent libraries. Please use the provided script ``src/runtime_src/tools/scripts/xrtdeps.sh`` to install the dependencies for both CentOS/RHEL and Ubuntu distributions. Additional information for RHEL/CentOS is below.
   
.. warning:: If ``xrtdeps.sh`` fails when installing devtoolset-6, then please manually install a later devtoolset, for example ``devtoolset-9``.  

To deploy XRT, simply install
the proper RPM or DEB package obtained from Xilinx.

To build a custom
version of XRT, please follow the instructions in :ref:`build.rst`.


MPSoC Based Embedded Platforms
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For building embedded platforms please refer to :ref:`yocto.rst`.
