.. _formats.rst:

Binary Formats
--------------

xclbin
~~~~~~

``xclbin`` container format is defined in file ``xclbin.h``. Since
2017.1, tools use ``xclbin2`` format also known as AXLF format. AXLF is
an extensible, future-proof container of (bitstream/platform) hardware
as well as software (MPSoC/Microblaze ELF files) design data.  This
unified container can hold outputs of our hardware compilers (SDAccel
``xocc``) as well as software compilers (processor ELF formats for
MPSoc/Microblaze). This allows for easier integration of embedded
processors and SDx on the same device. It also has structures to
describe the memory topology, acceleration kernel instantiations and
kernel connectivity for each kernel.

The compiler generates unique ``xclbin`` file for every design
compiled. The Vivado front-end tools programmatically
populate this file (layout described in ``driver/include/xclbin.h``,
attached) and then the runtime & device drivers read the information and apply
it accordingly.

The path to ``xclbin.h`` is ``runtime/driver/include/xclbin.h`` under
SDx installation directory.

Feature ROM
~~~~~~~~~~~

Feature ROM is like a BIOS like table for FPGA which describes key
properties of the device like its name and features enabled in the
Shell of the platform. The format for the data in Feature ROM is
defined in file ``xclfeatures.h``. It a section of memory mapped BRAM
memory which can be used for data sharing, error checking,
functionality discover and optimizations in SDx. The Vivado tools will
programmatically capture and populate BRAM memory in the platform
during platform creation time. Runtime components like drivers read it
and enable functionality in driver and also use the information to
perform hardware/software compatibility checks.

The path to ``xclfeatures.h`` is
``runtime/driver/include/xclfeatures.h`` under SDx installation
directory.
