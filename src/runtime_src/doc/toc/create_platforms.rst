Create MPSoC Based Embedded Platforms
-------------------------------------

An MPSoC Based Embedded platform defines a base hardware and software
architecture and application context. We provide a few scripts and
some basic source files as an example to create a custom MPSoC platform.

- ``src/platform/zcu102``
- ``src/platform/zcu102_svm``

Before creating platform, make sure to properly set up environment
for vivado. It takes same procedures to create platform for
zcu102 and zcu102_svm. Take zcu102 as an example.

*NOTE* The purpose of this page is only for easy to getting start.
If you are intrested in details about create DSA for a platform.
Please read the Hardware Platform section of Xilinx Document UG1146.

Build Hardware Platform
~~~~~~~~~~~~~~~~~~~~~~~

To create Hardware Platform, under platform/zcu102/

::

    vivado -mode batch -notrace -source ./zcu102_dsa.tcl

This will generate hardware Device Support Archive
``platform/zcu102/zcu102.dsa`` and Hardware Definition File
``platform/zcu102/zcu102_vivado/zcu102.hdf`` 

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

Copy image.ub to ``platform/zcu102/src/a53/ocl/image/`` and copy other elf
files to ``platform/zcu102/src/boot/``

::

    cp image.ub platform/zcu102/src/a53/ocl/image/
    mkdir platform/zcu102/src/boot
    cp bl31.elf platform/zcu102/src/boot/
    cp fsbl.elf platform/zcu102/src/boot/
    cp pmufw.elf platform/zcu102/src/boot/
    cp u-boot.elf platform/zcu102/src/boot/

Build Software Platform
~~~~~~~~~~~~~~~~~~~~~~~

To Create Software Platform, under platform/zcu102/

::

    xsct -sdx ./zcu102_pfm.tcl

The created zcu102 platform will be posted at

- ``platform/zcu102/output/zcu102/export/zcu102``
