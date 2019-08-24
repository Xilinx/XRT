.. _security.rst:

Security of Alveo Platform
**************************

.. image:: XSA-shell.svg
   :align: center

Security is built into Alveo platform hardware and software architecture. The platform
is made up of
two physical partitions: an immutable Shell and user compiled DFX partition. This
design allows end users to perform Dynamic Function eXchange (Partial Reconfiguration
in classic FPGA terminology) in the well defined DFX partition while the static Shell
provides key services.

Shell
=====

The Shell provides core infrastructure to the Alveo platform. It includes hardened PCIe
block which provides physical connectivity to the host PCIe bus via two physical functions
as described in :ref:`platforms.rst`.
The Shell is *trusted* partition of the Alveo platform and for all practical purposes
should be treated as an ASIC. The Shell is loaded on system boot from PROM. The Shell
cannot be changed once the system is up.

In the figure above, the Shell peripherals shaded blue can only be accessed from physical
function 0 (PF0). The Shell peripherals shaded violet can be accessed from physical
function 1 (PF1). From PCIe topology point of view PF0 **owns** the device and performs
supervisory actions on the device. Peripherals shaded blue are trusted but peripherals
shaded violet are not. A malicious actor who has direct access to blue shaded peripherals
can crash the device and PCIe bus. However, a malicious actor who has direct access to
violet shaded peripherals cannot crash the device or PCIe bus. This is particularly
significant in pass-through virtualization deployment as discussed later.

All peripherals in the shell except XDMA are slaves from PCIe point of view and cannot
initiate PCIe transactions. `XDMA <https://www.xilinx.com/support/documentation/ip_documentation/xdma/v4_1/pg195-pcie-dma.pdf>`_
is a regular PCIe scatter gather DMA engine with a well defined programming model.

The Shell provides a *control* path and a *data*
path to the user compiled image loaded on DFX partition. The Firewalls in control and data
paths protect the Shell from un-trusted DFX partition. For example if a slave in DFX has a
bug or is malicious the appropriate firewall will step in and protect the Shell from the
failing slave.

The shell image is itself distributed as signed RPM and DEB package files by Xilinx.
Shells may be upgraded using XRT **xbmgmt** tool by system administrators. The upgrade
process will update the PROM.


Dynamic Function eXchange
=========================

User compiled image packaged as xclbin is loaded on the Dynamic Functional eXchange
partition by the Shell. The image may be signed with a private key and its public
key should be registered with Linux kernel keyring. The xclbin signature is validated by
xclmgmt driver. This guarantees that only known good user compiled images are loaded by the
Shell. The image load is itself effected by xclmgmt driver which binds to the PF0.

xclbin is a container which packs FPGA bitstream for the DFX partition and host of related
metadata like clock frequencies, information about instantiated compute units, etc. The
compute units typically expose a well defined register space on the PCIe BAR for access by
XRT. An user compiled image does not have any physical path to directly interact with PCIe
Bus. Compiled images do have access to device DDR.


Xclbin Generation
=================

Users compile their Verilog/VHDL/OpenCL/C/C++ design using SDx compiler which also takes
the shell specification as a second input. By construction the SDx compiler generates image
compatible with DFX partition of the shell. The compiler uses a technology called *PR Verify*
to ensure that the user design physically confines itself to DFX partition and does not attempt
to overwrite portions of the Shell.


Firewall
========

Alveo hardware design uses standard AXI bus. As shown in the figure the control path uses AXI-Lite
and data path uses AXI4 full. Specialized hardware element called AXI Firewall monitors all transactions
going across the bus into the un-trusted DFX partition. It is possible that one or more AXI slave in the DFX
partition is not fully AXI-compliant or deadlocks/stalls/hangs during operation. When an AXI slave in DFX
partition fails AXI Firewall *trips* -- it starts completing AXI transactions on behalf of the slave so the
master and the specific AXI bus is not impacted -- to protect the Shell. The AXI Firewall starts completing
all transactions on behalf of misbehaving slave while also notifying the mgmt driver about the trip. The
mgmt driver then starts taking recovery action.

Deployment Models
=================

Baremetal
---------

In Baremetal deployment model, both physical functions are visible to the end user who *does not*
have root privileges. End user have access to both XRT **xclmgmt** and XRT **xocl** drivers. The system
administrator trusts both drivers which provide well defined :ref:`mgmt-ioctl.main.rst` and :ref:`xocl_ioctl.main.rst`.
End user does have the privilege to load xclbins which should be signed for maximum security. This
will ensure that only known good xclbins are loaded by end users.

Certain operations like resetting the board and upgrading the flash image on PROM (from which the shell
is loaded on system boot) require root privileges and are effected by xclmgmt driver.

Pass-through Virtualization
---------------------------

In Pass-through Virtualization deployment model, management physical function is only visible to the host
but user physical function is visible to the guest VM. Host considers the guest VM a *hostile* environment.
End users in guest VM may be root and may be running modified implementation of XRT **xocl** driver -- XRT
**xclmgmt** driver does not trust XRT xocl driver. xclmgmt as described before exposes well defined
:ref:`mgmt-ioctl.main.rst` to the host. In a good and clean deployment end users in guest VM interact with
standard xocl using well defined :ref:`xocl_ioctl.main.rst`.

As explained under the Shell section above, by design xocl has limited access to violet shaded Shell peripherals.
This ensures that users in guest VM cannot perform any privileged operation like updating flash image or device
reset. A user in guest VM can only perform operations listed under USER PF (PF1) section in :ref:`platforms.rst`.

A guest VM user can potentially crash a compute unit in DFX partition, deadlock data path AXI bus or corrupt
device memory. If the user has root access he may compromise VM memory. But none of this can bring down the
host or the PCIe bus. Host memory is protected by system IOMMU. Device reset and recovery is described below.

A user cannot load a malicious xclbin on the DFX partition since xclbin downloads are done by xclmgmt
drive. xclbins are passed on to the host via a plugin based MPD/MSD framework defined in
:ref:`mailbox.main.rst`. Host can add any extra checks necessary to validate xclbins received from guest VM.

This deployment model is ideal for public cloud where host does not trust the guest VM. This is the prevalent
deployment model for FaaS operators.

Summary
-------

+------------------------------+---------------------------+
| Behavior                     |     Deployment Model      |
|                              +------------+--------------+
|                              | Bare Metal | Pass-through |
+=================+============+============+==============+
| System admin    | xocl       | Yes        | No           |
| trusted driver  +------------+------------+--------------+
|                 | xclmgmt    | Yes        | Yes          |
+-----------------+------------+------------+--------------+
| End user root   | xocl       | No         | Maybe        |
| access          +------------+------------+--------------+
|                 | xclmgmt    | No         | No           |
+-----------------+------------+------------+--------------+
| End user can crash device    | Yes        | Yes          |
+------------------------------+------------+--------------+
| End user can crash PCIe bus  | No         | No           |
+------------------------------+------------+--------------+
| End user with root access    | Yes        | No           |
| can crash PCIe bus           |            |              |
+------------------------------+------------+--------------+



Signing of Xclbins
==================

xclbin signing process is similar to signing of Linux kernel modules. xclbins can be signed by XRT tool,
**xclbinutil**. The signing adds a PKCS7 signature at the end of xclbin. The signing certificate is then
registered with appropriate key-ring. XRT supports one of three levels of security which can be configured
with xbmgmt tool running with root privileges.

=============== =================================================================
Security level  xclmgmt driver xclbin signature verification behavior
=============== =================================================================
0               No verification
1               Signature verification enforced using signing certificate in
                *.xilinx_fpga_xclbin_keys* key-ring
2               Linux is running in UEFI secure mode and signature verification
                is enforced using signing certificate in *system* key-ring
=============== =================================================================

Mailbox
=======

Mailbox is used for communication between user physical function driver, xocl and management physical
function driver, xclmgmt. The Mailbox hardware design and xclmgmt driver mailbox handling implementation
has the ability to throttle requests coming from xocl which
protects it from DoS initiated by a malicious xocl. :ref:`mailbox.main.rst` has details on mailbox usage.

Device Reset and Recovery
=========================

Device reset and recovery is a privileged operation and can only be performed by xclmgmt driver. xocl
driver can request device reset by sending a message to xclmgmt driver over the Mailbox. An end user
can reset a device by using XRT **xbutil** tool. This tool talks to xocl driver which uses the reset
message as defined in :ref:`mailbox.main.rst`

Currently Alveo boards are reset by using PCIe bus *hot reset* mechanism. This resets the board peripherals
and also the PCIe link. The drivers reset their platform devices and kill all the clients which have opened
the device node by sending them a SIGBUS.
