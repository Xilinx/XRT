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

   cd XRT/build
   ./build.sh

``build.sh`` script builds for both Debug and Release profiles.

Build RPM package on RHEL/CentOS or DEB package on Ubuntu
.........................................................

::

   cd XRT/build/Release
   make package

Install the XRT RPM package
...........................

::

   yum reinstall ./XRT-2.1.0-Linux.rpm

Install the XRT DEB package
...........................

::

   apt install --reinstall ./XRT-2.1.0-Linux.deb

XRT Documentation
~~~~~~~~~~~~~~~~~

XRT Documentation can be built automatically using Sphinx doc builder together with Linux kernel based kernel-doc utility.

::

   cd XRT/src/runtime_src/doc/
   make
   firefox html/index.html
