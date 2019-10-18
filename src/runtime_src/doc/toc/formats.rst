.. _formats.rst:

Binary Formats
--------------

xclbin
~~~~~~

**xclbin** container format (also known as AXLF) is defined in file
``xclbin.h``. The file uses **xclbin2** as the magic word. AXLF is
sections based extensible container. Different sections store different
parts of compiled application like bitstreams for PL (FPGA fabric), ELF
for AIE tiles and embedded processors like Microblaze. It also contains
well structured metadata to define memory topology, IP layout of instantiated
peripherals and compute kernels, clocking details and kernel connectivity
for each compute kernel.

The compiler generates unique xclbin file tagged with UUID for every application
compiled. Each xclbin also has a another UUID which defines its compatbility to
the Shell. Vitis compiler, v++ generates this file as part of linking stage. End-users
load this file via XRT xclLoadXclbin() API. XRT userspace and kernel space
components consume different sections of xclbin by programming the hardware
and initializing key data structures in XRT userspace libraries and XRT
kernel drivers.

xclbins can also be signed. More information can be found in :ref:`security.rst`.

The path to ``xclbin.h`` is ``xrt/include/xclbin.h`` inside XRT
installation directory.

XRT provides a very powerful utility, **xclbinutil** which can be used to read/write/change
xclbins. More information can be found in the section on :ref:`xclbintools.rst`

Feature ROM
~~~~~~~~~~~

Feature ROM is like a BIOS like table for FPGA which describes key
properties of the device like its name and features enabled in the
Shell of the platform. The format for the data in Feature ROM is
defined in file ``xclfeatures.h``. It a section of memory mapped BRAM
memory which can be used for data sharing, error checking,
functionality discovery in Alveo platforms. Vivado tools will
programmatically capture and populate BRAM memory in the platform
during platform creation time. Runtime components like drivers read it
and enable functionality in driver and also use the information to
perform hardware/software compatibility checks.

The path to ``xclfeatures.h`` is ``xrt/include/xclfeatures.h`` inside
XRT installation directory.
