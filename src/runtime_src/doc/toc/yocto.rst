.. _yocto.rst:

===========================
XRT Setup for Embedded Flow
===========================

To use XRT on embedded designs, xrt and zocl two components need to be installed in the root file system. 
Xrt package contains user space libraries and zocl is a driver space module, which requires a device tree node.


Install XRT from Package Feed
------------------------------

Xilinx hosts pre-compiled packages on https://petalinux.xilinx.com. 
If the embedded system enabled dnf package manager, user can use dnf tool to install any packages in package feed.

For example, PetaLinux 2020.x package feeds are hosted at http://petalinux.xilinx.com/sswreleases/rel-v2020. 
User needs to download the package feed specific to the board architecutre and save them to /etc/yum.repos.d directory.

Here's an example to install XRT 2020.1 PU1 for ZCU104.

.. code-block:: bash

        # Execute the following scripts on target embedded system
        # Install dnf repository description
        cd /etc/yum.repos.d
        wget http://petalinux.xilinx.com/sswreleases/rel-v2020/generic/rpm/repos/zynqmp-generic_ev.repo
        wget http://petalinux.xilinx.com/sswreleases/rel-v2020/generic-updates/rpm/repos/zynqmp-generic_ev-update.repo
        # Clean dnf local cache
        dnf clean all
        # Install XRT and zocl
        dnf install xrt
        dnf install zocl


If an updated XRT is installed on target embedded platform, the host development sysroot should also be updated to the same version of XRT.

Here's an example to install XRT 2020.1 PU1 and its development packages to sysroot

.. code-block:: bash

        # Excute the following scripts on development machine
        # Download XRT RPM
        wget http://petalinux.xilinx.com/sswreleases/rel-v2020/generic-updates/rpm/aarch64/xrt-202010.2.7.0-r0.0.aarch64.rpm
        wget http://petalinux.xilinx.com/sswreleases/rel-v2020/generic-updates/rpm/aarch64/xrt-dev-202010.2.7.0-r0.0.aarch64.rpm
        wget http://petalinux.xilinx.com/sswreleases/rel-v2020/generic-updates/rpm/aarch64/xrt-dbg-202010.2.7.0-r0.0.aarch64.rpm
        wget http://petalinux.xilinx.com/sswreleases/rel-v2020/generic-updates/rpm/aarch64/xrt-lic-202010.2.7.0-r0.0.aarch64.rpm
        wget http://petalinux.xilinx.com/sswreleases/rel-v2020/generic-updates/rpm/aarch64/xrt-src-202010.2.7.0-r0.0.aarch64.rpm
        # Download sysroot overlay scripts
        wget https://raw.githubusercontent.com/Xilinx/XRT/master/src/runtime_src/tools/scripts/sysroots_overlay.sh
        # Generate RPM list file
        ls *.rpm > rpm.txt
        # Overlay RPM to sysroot
        ./sysroots_overlay.sh -s <platform_sysroot>/aarch64-xilinx-linux/ -r ./rpm.txt

sysroot_overlay.sh
~~~~~~~~~~~~~~~~~~

A `sysroot_overlay.sh` script is provided in XRT to extract RPM and update sysroot. This script will extract rpm libraries and include a file update in sysroot. 
Besides XRT, this script supports all RPMs for various software packages.

`sysroot_overlay.sh` is provided in XRT source code repository. If XRT repo has been cloned to local, please use it directly at `src/runtime_src/tools/scripts/sysroots_overlay.sh`; 
If not, user can download this script without cloning the whole XRT repository.

.. code-block:: bash

      wget https://raw.githubusercontent.com/Xilinx/XRT/master/src/runtime_src/tools/scripts/sysroots_overlay.sh  

`sysroot_overlay.sh` arguments description:

- `-s / --sysroot` is the sysroot to be overlaid.
- `-r / --rpms-file` is the rpms file that contains the RPM file paths to be overlaid.

This command depends on the following tools in the system. If the tools are not available, please install them.

- rpm2cpio
- cpio





Build XRT from Yocto Recipes 
----------------------------

XRT provide Yocto recipes to build libraries and driver for embedded platforms.
This section explains how to build Linux image by PetaLinux Tool.
It is NOT targeting to be a PetaLinux document or user guide.
Please read PetaLinux document before you read the rest of this page.

*NOTE* The purpose of this page is only for easy to get started.
If you are interested in details about creating software images for embedded platform.
Please read the Software Platform section of Xilinx® Document `UG1393 <https://www.xilinx.com/html_docs/xilinx2020_1/vitis_doc/tsf1596051751964.html>`_.

Prerequisite
~~~~~~~~~~~~

Before start to build Linux image, make sure your have:

1. PetaLinux tool installed and setup;
2. Vivado exported expandable XSA for your platform;

The PetaLinux tool can be downloaded from `Xilinx Download Center <https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/embedded-design-tools.html>`_.

Create PetaLinux Project with XRT
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

        # Replace <name> in below commands
        $ petalinux-create -t project -n <name> --template zynqMP

        # Get XSA file, which is exported by Vivado
        # A menu will show up for configuration, use below config to avoid password for login.
        #       menu -> "Yocto Setting" -> "Enable Debug Tweaks"
        $ petalinux-config -p <name> --get-hw-description=<XSA>

        #Now we can configure Linux kernel and rootfs.

        #Configure Linux kernel (default kernel config is good for zocl driver)
        $ petalinux-config -c kernel

        # Configure rootfs, enable below components:

        #   menu -> "user packages" -> xrt
        #   menu -> "user packages" -> xrt-dev
        #   menu -> "user packages" -> zocl
        #   menu -> "user packages" -> opencl-headers-dev
        #   menu -> "user packages" -> opencl-clhpp-dev
        $ petalinux-config -c rootfs

	# Enable "xrt" and "xrt-dev" options will install XRT libraries and header files to /opt/xilinx/xrt directory in rootfs. Enable "zocl" option will install zocl.ko in rootfs. The zocl.ko driver is a XRT driver module only for MPSoC platform.


        # Build package
        $ petalinux-build

You can find all output files from images/linux directory in your PetaLinux project.
These files can be used when creating an embedded platform.

- ``image.ub``
- ``bl31.elf``
- ``fsbl.elf``
- ``pmufw.elf``
- ``u-boot.elf``

Use an Asynchronous Version of XRT for PetaLinux Project
********************************************************

The PetaLinux built-in XRT version is verified by Xilinx. If in any case user would like to use a different version of XRT in PetaLinux project, here's the procedure.

Please note the intermediate versions of XRT are not tested by Xilinx.

1. Create the following files in `project-spec/meta-user` directory.

        - recipes-xrt/xrt/xrt_git.bbappend
        - recipes-xrt/zocl/zocl_git.bbappend

2. Add the following contents to the above two files.

.. code-block:: python

        BRANCH = "master"
        SRCREV = "<commit ID>"

The `BRANCH` parameter should match XRT branch name and `SRCREV` should match the commit ID of XRT git history in `XRT github repo <https://github.com/Xilinx/XRT/commits/master>`_.

3. Build PetaLinux with `petalinux-build` command.


Build XRT C/C++ applications through PetaLinux tool flow
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

	$ petalinux-create -t apps [--template TYPE] --name <user-applicationname> --enable
	#The new application sources can be found in the <plnx-proj-root>/project-spec/meta-user/recipes-apps/myapp directory.	

	# Change to the newly created application directory.
	$ cd <plnx-proj-root>/project-spec/meta-user/recipes-apps/myapp

	# myapp.c/myapp.cpp file can be edited or replaced with the real source code for your application.

	$ petalinux-build
	# This will rebuild the system image including the selected user application myapp.

