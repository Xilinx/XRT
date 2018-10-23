Building and Installing Software Stack
--------------------------------------

XRT
~~~

XRT requires C++11 compiler. Please install the necessary tools and dependencies
using the provided ``src/runtime_src/tools/scripts/xrtdeps.sh``

On RHEL/CentOS use devtoolset to switch to C++11 devlopment environment. This step
is not applicable to Ubuntu which already has C++11 capable GCC.

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

Install from inside either the ``Release`` or ``Debug`` directory
according to purpose with (the actual package name might differ) ::

   sudo yum reinstall ./XRT-2.1.0-Linux.rpm

Install the XRT DEB package
...........................

Install from inside either the ``Release`` or ``Debug`` directory
according to purpose with (the actual package name might differ) ::

   sudo apt install --reinstall ./xrt_201830.2.1.0_18.10.deb

XRT Documentation
~~~~~~~~~~~~~~~~~

XRT Documentation can be built automatically using Sphinx doc builder
together with Linux kernel based ``kernel-doc`` utility.

To compile and install the documentation into the ``doc`` directory at
the top of the repository::

   cd src/runtime_src/doc
   # For now the CMake can work only locally
   cmake .
   make xrt_doc
   # To look at the generated local documentation with a web browser:
   xdg-open html/index.html
