..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
   comment:: Copyright (C) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.

================
FleXible RunTime
================

.. image:: https://scan.coverity.com/projects/17781/badge.svg
    :target: https://scan.coverity.com/projects/xilinx-xrt-5f9a8a18-9d52-4cb2-b2ac-2d8d1b59477f

-------------------------------------------------------------------------------

.. image:: src/runtime_src/doc/toc/XRT-Layers.svg
   :align: center


FleXible RunTime (**XRT**) is implemented as a combination of user-space and kernel driver components. It provides
an abstracted runtime software interface for AMD NPUs and AMD FPGAs, enabling seamless access across
`AMD Ryzen™ <https://www.amd.com/en/products/processors/desktops/ryzen.html>`_ client,
`AMD Ryzen™ Embedded <https://www.amd.com/en/products/embedded/ryzen.html>`_ ,
`AMD Versal™ Adaptive SoCs <https://www.amd.com/en/products/adaptive-socs-and-fpgas/versal.html>`_ ,
`AMD Alveo™ Adaptable Accelerator Cards <https://www.amd.com/en/products/accelerators/alveo.html>`_ , and
`AMD Zynq™ UltraScale+™ MPSoCs <https://www.amd.com/en/products/adaptive-socs-and-fpgas/soc/zynq-ultrascale-plus-mpsoc.html>`_ .
XRT runs on both Linux and Windows, hosted on *x86_64* or *aarch64* host CPU architectures. XRT uses Linux
*accel* driver model on Linux and
Windows *MCDM* driver model on Windows. XRT ships with a command line tool,
``xrt-smi``, which may be used to examine, configure and validate NPU and FPGA devices.

`XRT API header files <https://github.com/Xilinx/XRT/tree/master/src/runtime_src/core/include/xrt>`_

-------------------------------------------------------------------------------

`System Requirements <https://xilinx.github.io/XRT/master/html/system_requirements.html>`_

-------------------------------------------------------------------------------

`Build Instructions <https://xilinx.github.io/XRT/master/html/build.html>`_

-------------------------------------------------------------------------------

`Documentation xilinx.github.io/XRT <https://xilinx.github.io/XRT>`_

-------------------------------------------------------------------------------
