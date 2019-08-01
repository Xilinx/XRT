==============
Xilinx Runtime
==============

.. image:: https://travis-ci.org/Xilinx/XRT.svg?branch=master
    :target: https://travis-ci.org/Xilinx/XRT

.. image:: https://scan.coverity.com/projects/17781/badge.svg
    :target: https://scan.coverity.com/projects/xilinx-xrt-5f9a8a18-9d52-4cb2-b2ac-2d8d1b59477f

-------------------------------------------------------------------------------

.. image:: src/runtime_src/doc/toc/XRT-Layers.svg
   :align: center
   
Xilinx Runtime (XRT) is implemented as as a combination of userspace and kernel
driver components. XRT supports both PCIe based boards like U200, U250 and MPSoC
base embedded platforms. XRT provides a standardized software interface to Xilinx 
FPGA. The key user APIs are defined in
`xclhal2.h <src/runtime_src/driver/include/xclhal2.h>`_ header file.

-------------------------------------------------------------------------------

`System Requirements <src/runtime_src/doc/toc/system_requirements.rst>`_

-------------------------------------------------------------------------------

`Build Instructions <src/runtime_src/doc/toc/build.rst>`_

-------------------------------------------------------------------------------

`Test Instructions <src/runtime_src/doc/toc/test.rst>`_

-------------------------------------------------------------------------------

Comprehensive documentation on `xilinx.github.io/XRT <https://xilinx.github.io/XRT>`_

-------------------------------------------------------------------------------
