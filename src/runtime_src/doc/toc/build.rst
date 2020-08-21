.. _build.rst:

Building the XRT Software Stack
--------------------------------------

Building the XRT Installation Package
~~~

Installing Building Dependencies
................................

XRT requires C++14 compiler and a few development libraries bundled
with modern Linux distribution. Please install the necessary tools and
dependencies using the provided ``xrtdeps.sh``.

::

   sudo <XRT>/src/runtime_src/tools/scripts/xrtdeps.sh

.. warning:: If ``xrtdeps.sh`` fails when installing devtoolset-6, then please manually install a later devtoolset, for example ``devtoolset-9``.
             
The ``xrtdeps.sh`` script installs the standard distribution packages
for the tools and libraries XRT depends on. If any system libraries
XRT depends on (for example Boost libraries) are updated to non
standard versions, then XRT must be rebuilt.

On RHEL7.x/CentOS7.x use devtoolset to switch to C++14 devlopment
environment. This step is not applicable to Ubuntu, which already has
C++14 capable GCC.

::

   scl enable devtoolset-9 bash

XRT includes source code for ERT firmware, which must compiled using the MicroBlaze GCC compiler, 
which is available in Xilinx Vitis. 
To compile the complete XRT package, please install Vitis Software Stack and set XILINX_VITIS environment. 
If Vitis is not available in the build system, ERT building will be skipped. XRT will use ERT firmware in ``/lib/firmware/xilinx`` on the deployment system. 
If it's not available, errors will be reported. 


Building the XRT Runtime
........................

::

   cd build
   ./build.sh

``build.sh`` script builds for both Debug and Release profiles.  

On RHEL/CentOS, if ``build.sh`` was accidentally run prior to enabling
the devtoolset, then it is necessary to clean stale files makefiles by
running ``build.sh clean`` prior to the next build.

Please check ERT firmware is built properly at ``build/Release/opt/xilinx/xrt/share/fw/sched*.bin``.


Packaging RPM on RHEL/CentOS or DEB on Ubuntu
.........................................................

The package is automatically built for the ``Release``
version but not for the ``Debug`` version::

   cd build/Release
   make package
   cd ../Debug
   make package



Building the XRT Documentation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XRT Documentation can be built automatically using ``Sphinx`` doc builder
together with Linux kernel based ``kernel-doc`` utility.

To compile and install the documentation into the ``doc`` directory at
the top of the repository::

   cd build
   ./build.sh docs
   # To browse the generated local documentation with a web browser:
   xdg-open Release/runtime_src/doc/html/index.html
