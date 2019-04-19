XRT Platforms
-------------

.. image:: XRT-Architecture.svg
   :align: center

XRT exports a common stack across PCIe based platforms and MPSoC based platforms.
From user perspective there is very little porting effort when migrating an
application from one class of platform to another.

PCIe Based Platforms
~~~~~~~~~~~~~~~~~~~~

1. VCU1525
2. U200
3. U250
4. U280
5. AWS F1

PCIe based platforms are supported on x86_64, PPC64LE and AARCH64 host architectures.
The shell (previously known as DSA) has two phsycial functions: mgmt pf and user pf.

MGMT PF
.......

XRT Linux kernel driver *xclmgmt* binds to mgmt pf. The driver is organized into subdevices and handles
the following functionality:

1. ICAP programming
2. CLock scaling
3. Loading firmware container, dsabin (RL Shell for 2 RP solution, embedded Microblaze firmware: ERT, XMC)
4. In-band sensors: Temp, Voltage, Power, etc
5. AXI Firewall management
6. Access to Flash programmer
7. Device reset and rescan
8. Hardware mailbox
9. Interrupt handling for AXI Firewall and Mailbox

USER PF
.......

XRT Linux kernel driver *xocl* binds to user pf. The driver is organized into subdevices and handles the
following functionality:

1. Device memory management as abstracted buffer objects
2. XDMA MM PCIe DMA engine programming
3. QDMA Streaming DMA engine programming
4. Multi-process aware context management
5. Standardized compute unit execution management (optionally with help of ERT) for client processes
6. Interrupt handling for DMA, Compute unit completion and Mailbox


MPSoC Based Embedded Platforms
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. ZCU19
2. ZCU102
3. ZCU104
4. ZCU106

MPSoC based platforms are supported with PetaLinux base stack. XRT Linux kernel
driver *zocl* does the heavy lifting for the embedded platform. It handles the
following functionality

1. CMA buffer management
2. SMMU programming for SVM platforms
3. Standardized compute unit execution management on behalf of client processes
4. xclbin download for platforms with Partial Reconfiguration support
