New DSA Bringup
---------------

To fullfill the different requirements, new DSAs are invented based on existing SDAccel DSAs. The first thing to verify the new DSA with XRT is to attach XRT drivers with it and see how it works. Then it might need to customizing and making changes in XRT to support the new DSA.

This section focuses on how to modify XRT drivers to identify the new DSA and how to customize XRT drivers for new DSA.

Assumption
~~~~~~~~~~

1. The firmware(.dsabin) file along with the new DSA is installed to the host properly. This firmware file could contain FPGA flash image, XMC image, microblaze image etc. which is consumed by driver and tools.

2. The new DSA bitstream is programed to the FPGA board properly.

Identify DSA
~~~~~~~~~~~~

There are two factor could be used to identify the new DSA. The first is PCI vendor-id, device-id and subsystem-id combination. The second is DSA VBNV name. For XRT driver ``src/runtime_src/core/pcie/driver/linux/xocl/devices.h`` need to be changed to identify the new DSA.

Add new PCI ID combination
..........................
Open devices.h and search
    ``XOCL_MGMT_PCI_IDS``      for management PCI function

    ``XOCL_USER_XDMA_PCI_IDS`` for XDMA user PCI function

    ``XOCL_USER_QDMA_PCI_IDS`` for QDMA user PCI function


Then add entries corresponding for the new DSA. The PCI ID combination has to be unique. ``PCI_ANY_ID`` is acceptable wildcast. The behavior of overlapped combination is undefined.

::

 { XOCL_PCI_DEVID(0x10EE, 0x4B88, 0x4351, USER_XDMA) }, 
 { XOCL_PCI_DEVID(0x10EE, 0x6850, PCI_ANY_ID, USER_XDMA) },

The "USER_XDMA" in above example is DSA profile macro which describes the IPs implemented by the DSA.

Add new VBNA
............
In some cases, two different DSAs use the same PCI ID combination. VBNV is used to identify them. Macro ``XOCL_DSA_VBNV_MAP`` in devices.h is used to combine the VBNV name with DSA profile macro.

::

 { 0x10EE, 0x5001, PCI_ANY_ID, "xilinx_u200_xdma_201820_1", &XOCL_BOARD_USER_XDMA },

Above example specifies xdma DSA profile for DSA which has VBNV "xilinx_u200_xdma_201820_1". This is going to overwrite the DSA profile combination defined in PCI ID table.

Customize DSA
~~~~~~~~~~~~~

Each DSA is described by a DSA profile macro in devices.h. This macro defines all the required information of DSA, include IP implemented in DSA, IO address and IRQ ranges for each IP, flags etc. The easiest way for new DSA is inheriting from an existing profile and customizing it.

Here are the supported IPs. And IO address and IRQ ranged are pre-defined in devices.h. If the new DSA introduces new IP or changes the IO address or IRQ, it has to add or modify the corresponding macro. For new IP, XRT is going to create a sub device node (platform devcie) and the driver of the new IP has to developed.

    feature ROM (FEATURE_ROM),

    Memmory Mapped DMA (MM_DMA),

    Embedded Runtime Scheduler(MB_SCHEDULER),

    Xilinx Virtual Cable (XVC_PUB, XVC_PRI),

    System Monitor (SYSMON),

    Axi Firewall (AF),

    Memory Interface Generator (MIG),

    Microblaze (MB),

    Xilinx I2C (XIIC),

    mailbox (MAILBOX),

    Internal Configuration Access Port (ICAP),

    Streaming DMA (STR_DMA),

    XMC (XMC),

    DNA (DNA)

Debug
~~~~~

Using lspci to check if the driver is loaded successfully. And please check dmesg output if the driver is not loaded.
