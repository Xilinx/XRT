.. _build.rst:

Building and Installing Software Stack
--------------------------------------

XRT
~~~

XRT requires C++14 compiler and a few development libraries bundled
with modern Linux distribution. Please install the necessary tools and
dependencies using the provided
``src/runtime_src/tools/scripts/xrtdeps.sh``.

.. warning:: If ``xrtdeps.sh`` fails when installing devtoolset-6, then please manually install a later devtoolset, for example ``devtoolset-9``.
             
The ``xrtdeps.sh`` script installs the standard distribution packages
for the tools and libraries XRT depends on. If any system libraries
XRT depends on (for example Boost libraries) are updated to non
standard versions, then XRT must be rebuilt.

On RHEL7.x/CentOS7.x use devtoolset to switch to C++14 devlopment
environment. This step is not applicable to Ubuntu which already has
C++14 capable GCC.

::

   scl enable devtoolset-9 bash

Build the runtime
.................

::

   cd build
   ./build.sh

``build.sh`` script builds for both Debug and Release profiles.  On
RHEL/CentOS, if ``build.sh`` was accidentally run prior to enabling
the devtoolset, then it is necessary to clean stale files makefiles by
running ``build.sh clean`` prior to the next build.

Build RPM package on RHEL/CentOS or DEB package on Ubuntu
.........................................................

.. warning:: XRT includes source code for ERT firmware, which must compiled using the MicroBlaze GCC compiler available with Xilinx Vitis.  Before building XRT locally, it is recommended that Xilinx Vitis is installed and that XILINX_VITIS environment variable is set accordingly. 

The package is automatically built for the ``Release``
version but not for the ``Debug`` version::

   cd build/Release
   make package
   cd ../Debug
   make package

Install the XRT RPM package
...........................

.. warning:: Before installing a locally built RPM for XRT, please make sure the ERT firmware was built by checking that ``build/Release/opt/xilinx/xrt/share/fw/sched*.bin`` exists under the build tree.  If ``sched.bin`` files are missing, please download and install Xilinx Vitis, set XILINX_VITIS, and build XRT again.  If you do not plan to install the RPM package then existing firmware under ``/lib/firmware/xilinx`` will continue to be used.

Install by providing a full path to the RPM package, for example, from
inside either the ``Release`` or ``Debug`` directory according to
purpose with (the actual package name might differ) ::

   sudo yum reinstall ./xrt_202020.2.7.0_7.4.1708-x86_64-xrt.rpm

Install the XRT DEB package
...........................

.. warning:: Before installing a locally built DEB for XRT, please make sure the ERT firmware was built by checking that ``build/Release/opt/xilinx/xrt/share/fw/sched*.bin`` exists under the build tree.  If ``sched.bin`` files are missing, please download and install Xilinx Vitis, set XILINX_VITIS, and build XRT again.  If you do not plan to install the DEB package then existing firmware under ``/lib/firmware/xilinx`` will continue to be used.

Install by providing a full path to the DEB package, for example, from
inside either the ``Release`` or ``Debug`` directory according to
purpose with (the actual package name might differ) ::

   sudo apt install --reinstall ./xrt_202020.2.7.0_18.04-amd64-xrt.deb

XRT Documentation
~~~~~~~~~~~~~~~~~

XRT Documentation can be built automatically using ``Sphinx`` doc builder
together with Linux kernel based ``kernel-doc`` utility.

To compile and install the documentation into the ``doc`` directory at
the top of the repository::

   cd build
   ./build.sh docs
   # To browse the generated local documentation with a web browser:
   xdg-open Release/runtime_src/doc/html/index.html
