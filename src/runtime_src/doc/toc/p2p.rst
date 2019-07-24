PCIe Peer-to-Peer Support
-------------------------

PCIe peer-to-peer communication (P2P) is a PCIe feature which enables two PCIe devices to directly transfer data between each other without using host RAM as a temporary storage. The latest version of SDx PCIe platforms support P2P feature via PCIe Resizeable BAR Capability.

1. Data can be directly transferred between the DDR of one SDx PCIe device and DDR of a second SDx PCIe device.
2. A thirdparty peer device like NVMe can directly read/write data from/to DDR of SDX PCIe device.

To use P2P, the DDRs on a SDx PCIe platform need to be mapped to host IO memory space. The total size of DDR on most SDx PCIe platforms is 64 GB all of which needs to mapped to the host IO memory space. Partial mapping a smaller range of device DDR is not supported in this release of XRT. Considering not all host systems (CPU/BIOS/chipset) support 64 GB IO memory space, P2P feature is off by default after a cold reboot or power cycle. The feature needs to be explicitly enabled after a cold boot.

Note that in addition to BIOS, host CPU should be capable of supporting a very large physical address space. Most desktop class processors do not support very large address space required for supporting 64 GB BAR together with host RAM and address space of all peripherals.

BIOS Setup
~~~~~~~~~~

1. Before turning on P2P, please make sure 64-bit IO is enabled and the maximium host supported IO memory space is greater than total size of DDRs on SDx PCIe platform in host BIOS setup.

2. Enable large BAR support in BIOS. This is sometimes called "Above 4G decoding" and may be found under PCIe configuration or Boot configuration.


Note
.......
It may be necessary to update to the latest BIOS release before enabling P2P.  Not doing so may cause the system to continuously reboot during the boot process.  If this occurs, power-cycle the system to disable p2p and allow the system to boot normally.


Warning
.......

Mother board vendors have different implementations of large PCIe BAR support in BIOS. If the host system does not support large IO memory well or if host Linux kernel does not support this feature, the host could stop responding after P2P is enabled. Please note that in some cases a warm reboot may not recover the system. Power cycle is required to recover the system in this scenario. As previosuly noted SDx PCIe platforms turn off P2P after a power cycle.

Some Mother board BIOS setup allows administrator to set IO Memory base address and some do not. Having high or low IO Memory base could possibly cause memory address collision between P2P memory and host RAM. For example setting IO memory base set to 56T in BIOS will cause Linux to crash after a warm reboot. This is because mapped virtual address of P2P PCIe BAR collides with virtual address of other subsystems in Linux. Recent versions of Linux (4.19+) dont have this issue as they detect this during P2P BAR mapping stage itself and refuse to map the BAR. Setting IO memory base to 1T in BIOS is recommended.

Enable/Disable P2P
~~~~~~~~~~~~~~~~~~

XRT ``xbutil`` is used to enable/disable P2P feature and check current configuration. P2P configuration is persistent across warm reboot. Enabling or disabling P2P requires root privilege.

Enabling P2P after cold boot is likly to fail because it resizes an exisitng P2P PCIe BAR to a large size and usually Linux will not reserve large IO memory for the PCIe bridges. XRT driver checks the maximum IO memory allowed by host BIOS setup and returns error if there is not enough IO memory for P2P. A warm reboot is required in this scenario after which BIOS and Linux will reassign the required expanded IO memory resource for P2P BAR.
If a system stops responding after enabling P2P and warm reboot does not recover the host then power cycle is required to recover the host.

Disabling P2P takes effect immediately. Currently XRT does not check if the P2P memory is in use. Administrator needs to make sure P2P is not in use before disabling it. The result of disabling P2P while it is in use is undefined.

The IO memory region will not be completely released after disabling P2P. Thus, re-enabling P2P does not need reboot.

Current P2P Configuration
.........................

``P2P Enabled`` is shown within ``xbutil query`` output.

::

 # xbutil query
 XSA                             FPGA                        IDCode
 xilinx_vcu1525_dynamic_6_0      xcvu9p-fsgd2104-2L-e        0x14b31093
 Vendor          Device          SubDevice       SubVendor
 0x10ee          0x6a9f          0x4360          0x10ee
 DDR size        DDR count       Clock0          Clock1
 34359738368     2               300             500
 PCIe            DMA chan(bidir) MIG Calibrated  P2P Enabled
 GEN 3x16        2               true            false


There are three possible values for ``P2P Enabled`` field above.

============  =========================================================
Value         Remarks
============  =========================================================
``True``      P2P is enabled
``False``     P2P is disabled
``no iomem``  P2P is enabled in device but system could not allocate IO
              memory, warm reboot is needed
============  =========================================================

Enable P2P
..........

Enable P2P after power up sequence.

::

 # xbutil p2p --enable
 ERROR: resoure busy, please try warm reboot
 # xbutil query
 ...
 PCIe            DMA chan(bidir) MIG Calibrated  P2P Enabled
 GEN 3x16        2               true            no iomem
 # reboot
 ...
 # xbutil query
 ...
 PCIe            DMA chan(bidir) MIG Calibrated  P2P Enabled
 GEN 3x16        2               true            true
 ...

Enable P2P without enough IO memory configured in BIOS.

::

 # xbutil p2p --enable
 ERROR: Not enough iomem space.
 Please check BIOS settings

Disable P2P
...........

Disable and re-enable P2P.

::

 # xbutil query
 ...
 PCIe            DMA chan(bidir) MIG Calibrated  P2P Enabled
 GEN 3x16        2               true            true
 ...
 # xbutil p2p --disable
 # xbutil query
 ...
 PCIe            DMA chan(bidir) MIG Calibrated  P2P Enabled
 GEN 3x16        2               true            false
 ...
 # xbutil p2p --enable
 # xbutil query
 ...
 PCIe            DMA chan(bidir) MIG Calibrated  P2P Enabled
 GEN 3x16        2               true            true
 ...

Force Enable/Disable
....................

This is for advanced user. Force enabling P2P is going to free and renumerate all devices under same root bus. The result of failed freeing of devices other than SDx platform is undefined. The best scenario is there is only SDx platform under the same root bus.

::

 # xbutil p2p --enable -f
 # xbutil query
 ...
 PCIe            DMA chan(bidir) MIG Calibrated  P2P Enabled
 GEN 3x16        2               true            true
 ...
 # xbutil p2p --disable
 # xbutil query
 ...
 PCIe            DMA chan(bidir) MIG Calibrated  P2P Enabled
 GEN 3x16        2               true            false
 ...

PCIe Topology Considerations
............................

For best performance peer devices wanting to exchange data should be under the same PCIe switch.

If IOMMU is enabled then all peer-to-peer transfers are routed through the root complex which will degrade performance significantly.
