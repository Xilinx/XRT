.. _system_requirements.rst:

System Requirements
-------------------

Host Platform for PCIe Accelerator Cards
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. x86_64
2. AARCH64
3. PPC64LE

Supported Xilinx® Accelerator Cards are listed in :ref:`platforms.rst`.


XRT Software Stack for PCIe Accelerator Cards
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XRT software stack requires Linux kernel 3.10+. 

The XRT software stack is tested on RHEL/CentOS and Ubuntu. 
For the detailed list of supported OS, please refer to the specific release versions of `UG1451 XRT Release Notes <https://www.xilinx.com/search/site-keyword-search.html#q=ug1451>`_. 

XRT is needed on both application development and deployment environments. 

To install XRT on the host, please refer to page :ref:`install.rst` for dependencies installation steps and XRT installation steps.

To build a custom XRT package, please refer to page :ref:`build.rst` for dependencies installation steps and building steps.

XRT Software Stack for Embedded Platforms
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XRT software stack requires Linux kernel 3.10+. XRT for embedded platforms is tested with PetaLinux.

XRT needs to be installed on the development environment (rootfs or sysroot) and deployment environment (rootfs) of embedded platforms.

If embedded processor native compile is to be used, XRT, xrt-dev and GCC needs to be installed on the target embedded system rootfs.

If application is developed on a server with cross compiling technique, XRT needs to be installed into sysroot. The application can be cross compiled against the sysroot. 
XRT for server is not required on the cross compile server.

The embedded platform for deployment should have XRT and ZOCL installed. For details about building embedded platforms please refer to :ref:`yocto.rst`.
