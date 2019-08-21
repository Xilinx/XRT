.. _security.rst:

Security of Alveo Platform
**************************

.. image:: XSA-shell.svg
   :align: center

Security is built into Alveo platform architecture. The platform is made up of
two physical partitions: an immutable Shell and user compiled DFX partition. This
design allows end users to perform Dynamic Function eXchange (Partial Reconfiguration
in classic FPGA parlance) in the well defined DFX partition while the Shell remains
unchanged.

The Shell peripherals shaded blue can only be accessed from physical function 0. The Shell
peripheral shaded violet can be accessed from physical function 1. The Shell provides a
control path and a data path to the user compiled image loaded on DFX partition. The Firewalls
protect the Shell from DFX partition. For example if a slave in DFX has a bug or is malicious
the appropriate firewall will step in and protect the Shell from the failing slave.


Shell
=====

The Shell provides core infrastructure to the Alveo platform. It provides connectivity
to the host PCIe bus via two physical functions as described in :ref:`platforms.rst`.
The Shell is *trusted* partition of the Alveo platform and can almost be treated as a
fixed ASIC of the accelerator. The Shell is loaded on system boot from PROM. The Shell
cannot be changed once the system is up.

The shell includes hardened PCIe block which provides physical connectivity to host
PCIe bus. The Shell is immutable and comprises two physical functions: *mgmt pf* and
*user pf*. The details of these physical functions are described in :ref:`platforms.rst`.
All peripherals in the shell except XDMA are slaves from PCIe point of view and cannot
initiate PCIe transactions. `XDMA <https://www.xilinx.com/support/documentation/ip_documentation/xdma/v4_1/pg195-pcie-dma.pdf>`_
is a regular PCIe scatter gather DMA engine with a well defined programming model.

The shell image is itself distributed as signed RPM and DEB package files by Xilinx.
Shells may be upgraded using XRT ``xbmgmt`` tool by system administrators. The upgrade
process will update the PROM.


Dynamic Function eXchange
=========================

User compiled image packaged as xclbin is loaded on the Dynamic Functional eXchange
partition by the Shell. The image may be signed with a private key and its public
key should be registered with Linux kernel keyring. The signature is validated by xclmgmt
driver. This guarantees that only known good user compiled images are loaded by the Shell.
The image load is itself effected by xclmgmt driver which binds to the Physical Function 0.

xclbin is a container which packs FPGA bitstream for the DFX partition and host of related
metadata like clock frequencies, information about instantiated compute units, etc. The
compute units typically expose a well defined register space on the PCIe BAR for access by
XRT. An user compiled image does not have any physical path to directly interact with PCIe
Bus. Compiled images do have access to device DDR.


xclbin Generation
=================

Users compile their Verilog/VHDL/OpenCL/C/C++ design using SDx compiler which also takes
the shell specification as a second input. By construction the SDx compiler generates image
compatible with DFX partition of the shell. The compiler uses a technology called *PR Verify*
to ensure that the user design physically confines itself to DR partition and does not attempt
to overwrite portions of the Shell.


Firewall
========

Alveo hardware design uses standard AXI bus. As shown in the figure the control path uses AXI-Lite
and data path uses AXI4 full. Specialized hardware element called AXI Firewall monitors all transactions
going across the bus into the bun-trusted DFX partition. It is possible that one or more AXI slave in the DFX
partition is not fully AXI-compliant or deadlocks/stalls/hangs during operation. At this point to protect
the Shell, AXI Firewall trips -- it starts completing AXI transactions on behalf of the slave so the master
and the specific AXI bus is not impacted. The AXI Firewall starts completing all transactions on behalf of
the slave while also notifying the mgmt driver about the trip. The mgmt driver then starts taking recovery
action.

Deployment Models and Trust Roles
=================================

Baremetal
---------

In Baremetal deployment model, both physical functions are visible to the end user. End users interact
with both xclmgmt and xocl drivers directly. The system administrator trusts both drivers. End users
have the privilege to load xclbins which should be signed for most security. Ideally in an enterprise
the system administrator should sign the xclbins with an enterprise key which is registered with the
system key-ring. In this case xclmgmt driver would only permit signed xclbins to be loaded. This will
ensure that only known good xclbins are loaded by end users.

Certain operations like resetting the board and upgrading the flash image on PROM (from which the shell is
loaded on system boot) require root privileges and are effected by xclmgmt driver.

Pass-through Virtualization
---------------------------

In Pass-through Virtualization deployment model, management physical function is only visible to the host
but user physical function is visible to the guest VM. Users in guest VM cannot perform any privileged
operation like updating flash image or device reset. Since xclbin downloads are done by xclmgmt driver
xclbins are passed on to the host via a plugin based MPD/MSD defined in :ref:`mailbox.main.rst`. Host can
add any extra checks necessary to validate xclbins received from guest VM. This deployment model is ideal
for public cloud where host does not trust the guest VM. This is the prevalent deployment model for FaaS
operators.

Signing of Xclbins
==================

xclbin signing process is similar to signing of Linux kernel modules. xclbins can be signed by XRT utility,
``xclbinutil``. The signing adds a PKCS7 signature at the end of xclbin. The signing certificate is then
registered with appropriate key-ring. XRT supports one of three levels of security which can be configured
with xbmgmt utility running with root privileges.

Level 0
-------

In this configuration xclmgmt driver does not perform any signature verification

Level 1
-------

In this configuration xclmgmt driver looks for signing certificate in *.xilinx_fpga_xclbin_keys* key-ring

Level 2
-------

In this configuration xclmgmt driver is running in UEFI secure mode and only trusts *system* key-ring.


Mailbox
=======

Mailbox is used for communication between user physical function driver, xocl and management physical
function driver, xclmgmt. :ref:`mailbox.main.rst` has details on mailbox usage.
