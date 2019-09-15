.. _multiprocess.rst:

Multi-Process Support
*********************

Support for Multi-Process kernel execution is default in 2019.1 release

Requirements
============

Multiple processes can share access to the same device provided each
process uses the same ``xclbin``. Attempting to load different xclbins via
different processes concurrently will result in only one process being
successfull in loading its xclbin. The other processes will get error code
-EBUSY or -EPERM.

Usage
=====

Processes share access to all device resources; as of 2019.1, there is
no support for exclusive access to resources by any one process.

If two or more processes execute the same kernel, then these processes
will acquire the kernel's compute units per the ``xocl`` kernel driver
compute unit scheduler, which is first-come first-serve.  All
processes have the same priority in XRT.

To disable multi-process support, add the following entry to :ref:`xrt_ini.rst`
in the same directory as all the executable(s)::

  [Runtime]
  multiprocess=false


Known problems
==============

Debug and Profile will only be enable for the first process when multi-process
has been enabled. Emulation flow does not have support for multi-process yet.


Implementation Details For Curious
==================================

Since 2018.3 downloading an xclbin to the device does not guarantee an automatic lock
on the device for the downloading process. Application is required to create explicit
context for each Compute Unit (CU) it wants to use. OCL applications automatically handle
context creation without user needing to change any code. XRT native applications
should create context on a CU with xclOpenContext() API which requires xclbin UUID
and CU index. This information can be obtained from the xclbin binary. xclOpenContext()
increments the xclbin UUID which prevents that xclbin from being unloaded. A corresponding
xclCloseContext() releases the reference count. xclbins can only be swapped if the reference
count is zero. If an application dies or exits without explicitly releasing the contexts it
had opened before the driver would automatically release the stale contexts.

The following diagram shows a possibility with 7 processes concurrently using a device. The
processes in green are successful but processes in red fail at diffrent stages with appropriate
error codes. Processes P0, P1, P2, P3, P4 and P6 are each trying to use xclbin with UUID_X,
process P5 is attempting to use UUID_Y. Processes P0, P1, P3, P4, and P6 are trying to use CU_0 in
UUID_X. Process P2 is trying to use CU_1 in UUID_X and Process P5 is trying to use CU_0 in UUID_Y.
The diagram shows timeline view with all 7 processes running concurrently.

.. graphviz:: multi.dot
	:caption: Multi-process interaction diagram
