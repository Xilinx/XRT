.. _`Yocto Recipes For Embedded Flow`:

Yocto Recipes For Embedded Flow
-------------------------------

XRT provide Yocto recipes to build libraries and driver for MPSoC platform.
This page explains how to build Linux image with by PetaLinux Tool.
But this page is NOT the petalinux document or user guide.
Please read PetaLinux document before you read the rest of this page.
At the end, you can get these files as output.

- ``image.ub``
- ``bl31.elf``
- ``fsbl.elf``
- ``pmufw.elf``
- ``u-boot.elf``

*NOTE* The purpose of this page is only for easy to getting start.
If you are intrested in details about create software images for a platform.
Please read the Software Platform section of Xilinx Document UG1146.

Prerequisite
~~~~~~~~~~~~

Before start to build Linux image. Make sure your have:
        1. PetaLinux tool chain installed and setup;
        2. Hardware Definithion File(.hdf) for your platform;

The PetaLinux tool chain can be downloaded from xilinx.com.
If you don't have .hdf file, please see :ref:`Build Boot Images`.

Create PetaLinux Project with XRT recipes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

All of the XRT recipes are in ``XRT/src/platform/recipes-xrt`` directory.

.. code-block:: bash

        # Replace <name> in below commands
        $ petalinux-create -t project -n <name> --template zynqMP

        # Get HDF file, which is exported by Vivado
        $ petalinux-config -p <name> --get-hw-description=<HDF>

        # Go to the project specific directory of the project
        $ cd <name>/project-spec/meta-user/

        # Copy XRT recipes
        $ cp -r <XRT>/src/platform/recipes-xrt .

        # If you are using PetaLinux 2018.3 or earlier version, do below steps
        $ mkdir recipes-xrt/opencl-headers
        $ wget -O recipes-xrt/opencl-headers/opencl-headers_git.bb http://cgit.openembedded.org/meta-openembedded/plain/meta-oe/recipes-core/opencl-headers/opencl-headers_git.bb

The above commands add necessary recipes to build XRT library and driver. Please check all the .bb file for details of the recipes.

The next step is to add all recipes to PetaLinux Rootfs Menu.
Still stay in ``meta-user`` directory. Open ``recipes-core/images/petalinux-image.bbappend`` then add below lines at the end.

        | IMAGE_INSTALL_append = " xrt-dev"
        | IMAGE_INSTALL_append = " xrt"
        | IMAGE_INSTALL_append = " zocl"

Add XRT kernel node in device tree
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An example of device tree is in ``<XRT>/src/runtime_src/driver/zynq/fragments/xlnk_dts_fragment_mpsoc.dts``. You can attach it to ``project-spec/meta-user/recipes-bsp/device-tree/files/system-user.dtsi`` in your PetaLinux project.

Configure Linux kernel and enable XRT module
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

        #Configure Linux kernel (default kernel config is good for zocl driver)
        $ petalinux-config -c kernel

        # Configure rootfs, enable below components:
        #   menu -> "user packages" -> xrt
        #   menu -> "user packages" -> xrt-dev
        #   menu -> "user packages" -> zocl
        $ petalinux-config -c rootfs

        # Build package
        $ petalinux-build

You can find all output images in images/linux directory in your PetaLinux project.

