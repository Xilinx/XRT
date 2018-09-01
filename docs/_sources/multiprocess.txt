Multi-Process Support
---------------------

Support for Multi-Process kernel execution is added in the 2018.2
release as a preview feature.

Requirements
============

Multiple processes can share access to the same device provided each
process use the same xclbin.

Usage
=====

Processes share access to all device resources; as of 2018.2, there is
no support for exclusive access to resources by any process.

If two or more processes execute the same kernel, then these processes
will acquire the kernel's compute units per the xocl kernel driver
compute unit scheduler, which is first come first serve.  All
processes have the same priority in XRT.

To enable multiprocess support, add the following entry to sdaccel.ini
in the same directory as the executable(s).

::

  [Runtime]
  multiprocess=true


Known problems
==============

xclbin must be loaded
~~~~~~~~~~~~~~~~~~~~~

The xclbin shared by multiple processes **must** be pre-programmed.
Failure to pre-program the device results in the following error:

::

  ERROR: Failed to load xclbin
  Error: Failed to create compute program from binary -44!

An xclbin is programmed explicitly by using xbutil::

  xbutil program -p <xclbin>
