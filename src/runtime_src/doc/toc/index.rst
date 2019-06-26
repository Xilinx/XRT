=================================
Xilinx Runtime (XRT) Architecture
=================================

Xilinx Runtime (XRT) is implemented as a combination of userspace and kernel
driver components. XRT supports both PCIe based accelerator cards and MPSoC
based embedded architecture provides standardized software interface to Xilinx
FPGA. The key user APIs are defined in ``xrt.h`` header file.

.. image:: XRT-Layers.svg
   :align: center

----------------------------------------------------------------------------

.. toctree::
   :maxdepth: 1
   :caption: XRT Introduction

   platforms.rst
   system_requirements.rst
   build.rst
   test.rst


.. toctree::
   :maxdepth: 1
   :caption: XRT Use Model and Features

   execution-model.rst
   multiprocess.rst
   p2p.rst
   m2m.rst
   xma_user_guide.rst


.. toctree::
   :maxdepth: 1
   :caption: XRT API Library

   xrt.main.rst
   ert.main.rst
   xma.main.rst


.. toctree::
   :caption: Kernel Driver 
   :maxdepth: 1

   sysfs.rst
   mgmt-ioctl.main.rst
   xocl_ioctl.main.rst


.. toctree::
   :caption: Tools and Utilities  
   :maxdepth: 1

   tools.rst
   formats.rst
   xclbintools.rst


.. toctree::
   :caption: Platform Building  
   :maxdepth: 1

   yocto.rst
   test.rst
   newxsa-bringup.rst
   create_platforms.rst


.. toctree::
   :caption: Cloud Support   
   :maxdepth: 1

   mailbox.main.rst
   mailbox.proto.rst


.. toctree::
   :caption: Debug and Faqs  
   :maxdepth: 1

   debug-faq.rst
