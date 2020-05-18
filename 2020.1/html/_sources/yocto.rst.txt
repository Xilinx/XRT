.. _yocto.rst:

Yocto Recipes For Embedded Flow
-------------------------------

XRT provide Yocto recipes to build libraries and driver for MPSoC platform.
This page explains how to build Linux image by PetaLinux Tool.
It is NOT targeting to be a PetaLinux document or user guide.
Please read PetaLinux document before you read the rest of this page.

*NOTE* The purpose of this page is only for easy to get started.
If you are interested in details about creating software images for embedded platform.
Please read the Software Platform section of Xilinx® Document UG1146.

Prerequisite
~~~~~~~~~~~~

Before start to build Linux image, make sure your have:
        1. PetaLinux tool installed and setup;
        2. Hardware Definithion File(.hdf) for your platform;

The PetaLinux tool can be downloaded from xilinx.com.

Create PetaLinux Project with XRT
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

        # Replace <name> in below commands
        $ petalinux-create -t project -n <name> --template zynqMP

        # Get HDF file, which is exported by Vivado
        # A menu will show up for configuration, use below config to avoid password for login.
        #       menu -> "Yocto Setting" -> "Enable Debug Tweaks"
        $ petalinux-config -p <name> --get-hw-description=<HDF>

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

