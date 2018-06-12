Building and Installing Software Stack
--------------------------------------

XRT
~~~

::

   git clone https://github.com/Xilinx/XRT.git
   cd XRT/build
   ./build.sh

``build.sh`` script builds for both Debug and Release profiles.

To build RPM package on RHEL/CentOS or DEB package on Ubuntu

::

   cd XRT/build/Release
   make package

To install the XRT RPM package

::
   yum install ./XRT-2.1.0-Linux.rpm

To install the XRT DEB package

::

   apt install ./XRT-2.1.0-Linux.deb

XRT Documentation
~~~~~~~~~~~~~~~~~

XRT Documentation can be built automatically using Sphinx doc builder together with Linux kernel based kernel-doc utility.

::

   cd XRT/src/runtime_src/doc/
   make
