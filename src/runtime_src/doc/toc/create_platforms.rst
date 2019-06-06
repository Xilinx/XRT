Create MPSoC Based Embedded Platforms
-------------------------------------

An MPSoC Based Embedded platform defines a base hardware and software
architecture and application context. We provide a few scripts and
some basic source files as an example to create a custom MPSoC platform.

- ``src/platform/zcu102ng``
- ``src/platform/zcu102ng_svm``

Before creating platform, make sure to properly set up environment
for vivado. It takes same procedures to create platform for
zcu102ng and zcu102ng_svm. Take zcu102ng as an example.

*NOTE* The purpose of this page is only for easy to get started.
If you are interested in details about creating DSA for a platform.
Please read the Hardware Platform section of Xilinx Document UG1146.

Build Hardware Platform
~~~~~~~~~~~~~~~~~~~~~~~

To create Hardware Platform, under platform/zcu102ng/

::

    vivado -mode batch -notrace -source ./zcu102ng_dsa.tcl

This will generate hardware Device Support Archive
``platform/zcu102ng/zcu102ng.dsa`` and Hardware Definition File
``platform/zcu102ng/zcu102ng_vivado/zcu102ng.hdf`` 

.. _`Build Boot Images`:

Build Boot Images
~~~~~~~~~~~~~~~~~

Using PetaLinux to build necessray Boot Images for the software
platform with the Hardware Definition File we created. :ref:`Yocto Recipes For Embedded Flow`.

The boot image files required include

- ``image.ub``
- ``bl31.elf``
- ``fsbl.elf``
- ``pmufw.elf``
- ``u-boot.elf``

Copy image.ub to ``platform/zcu102ng/src/a53/xrt/image/`` and copy other elf
files to ``platform/zcu102ng/src/boot/``

::

    cp image.ub         platform/zcu102ng/src/a53/xrt/image/image.ub
    mkdir platform/zcu102ng/src/boot
    cp bl31.elf         platform/zcu102ng/src/boot/bl31.elf
    cp zynqmp_fsbl.elf  platform/zcu102ng/src/boot/fsbl.elf
    cp pmufw.elf        platform/zcu102ng/src/boot/pmufw.elf
    cp u-boot.elf       platform/zcu102ng/src/boot/u-boot.elf

Build Software Platform
~~~~~~~~~~~~~~~~~~~~~~~

To Create Software Platform, under platform/zcu102ng/

::

    xsct -sdx ./zcu102ng_pfm.tcl

The created zcu102ng platform will be posted at

- ``platform/zcu102ng/output/zcu102ng/export/zcu102ng``

Make use of Platform
~~~~~~~~~~~~~~~~~~~~

Here is a simple example of how to make use of the customized platform we built.
Suppose we have a hello world OpenCL application hello.cl, we can use xocc tool
to build boot images (including BOOT.BIN) on zcu102 board.

::

    xocc -c -t hw --platform <PATH_TO_PLATFORM>/zcu102ng.xpfm hello.cl -o hello.xo
    xocc -l -t hw --platform <PATH_TO_PLATFORM>/zcu102ng.xpfm hello.xo -o hello.xclbin --sys_config xrt

The boot images will be posted at sd_card directory
