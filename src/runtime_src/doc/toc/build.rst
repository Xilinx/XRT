.. _build.rst:

Building and Installing Software Stack
--------------------------------------

XRT
~~~

XRT requires C++14 compiler and a few development libraries bundled with modern Linux
distribution. Please install the necessary tools and dependencies
using the provided ``src/runtime_src/tools/scripts/xrtdeps.sh``

On RHEL/CentOS use devtoolset to switch to C++14 devlopment environment. This step
is not applicable to Ubuntu which already has C++14 capable GCC.

::

   scl enable devtoolset-6 bash

Build the runtime
.................

::

   cd build
   ./build.sh

``build.sh`` script builds for both Debug and Release profiles.  On RHEL/CentOS, if ``build.sh`` was accidentally run prior to enabling the devtoolset, then it is necessary to clean stale files makefiles by running ``build.sh clean`` prior to the next build.

Build RPM package on RHEL/CentOS or DEB package on Ubuntu
.........................................................

The package is actually automatically built for the ``Release``
version but not for the ``Debug`` version::

   cd build/Release
   make package
   cd ../Debug
   make package

Install the XRT RPM package
...........................

.. warning:: Before installing a locally built RPM for XRT, please copy aside ``/lib/firmware/xilinx/sched*.bin``, which contain the ERT firmware for MicroBlaze. If XRT is built without access to the MicroBlaze GCC compiler, then ``sched.bin`` and ``sched_u50.bin`` will be missing from the RPM.  After installing a local build of XRT, you must manually copy the firmware files back to ``/lib/firmware/xilinx``.

Install from inside either the ``Release`` or ``Debug`` directory
according to purpose with (the actual package name might differ) ::

   sudo yum reinstall ./XRT-2.1.0-Linux.rpm

Install the XRT DEB package
...........................

.. warning:: Before installing a locally built DEB for XRT, please copy aside ``/lib/firmware/xilinx/sched*.bin``, which contain the ERT firmware for MicroBlaze. If XRT is built without access to the MicroBlaze GCC compiler, then ``sched.bin`` and ``sched_u50.bin`` will be missing from the DEB.  After installing a local build of XRT, you must manually copy the firmware files back to ``/lib/firmware/xilinx``.

Install from inside either the ``Release`` or ``Debug`` directory
according to purpose with (the actual package name might differ) ::

   sudo apt install --reinstall ./xrt_201830.2.1.0_18.10.deb

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
