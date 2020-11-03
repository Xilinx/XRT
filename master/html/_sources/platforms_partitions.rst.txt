.. _platform_partitions.rst:


=================================
 Alveo™ Platform Loading Overview
=================================

Alveo platforms are architected as two logical FPGA partitions: *shell* and *role*. In 1RP platforms like U30 and U50 shell contains only one fixed physical partition called BLP. In 2RP platforms shell contains a fixed physical partition called BLP and a loadable physical partition called PLP. For both class of platforms the role partition is made of physical partition called ULP that can be loaded by end user. Every physical partition has two interface UUIDs: parent UUID and child UUID.

.. note::
   Partition compatibility matching is key part of Alveo platforms and XRT. The partitions have child and parent relationship. A loaded partition exposes child partition UUID to advertise its compatibility requirement for child partition. When loading a child partition the management driver matches parent UUID of the child partition against child UUID exported by the parent. Parent and child partition UUIDs are stored in the xclbin (for ULP) or xsabin (for BLP and ULP).


BLP
===

.. figure:: XSA-shell-partitions-1RP.svg
    :figclass: align-center

    Alveo shell partitions and loading for 1RP platform

BLP partition is loaded from flash at system boot time. It establishes the PCIe link and exposes two physical functions to the BIOS. After OS boot xclmgmt driver attaches to BLP physical function 0 and then looks for VSEC in PCIe extended config space. Using VSEC it determines the logic UUID of BLP and uses the UUID to load matching *xsabin* from Linux firmware directory. The xsabin contains metadata to discover peripherals that are part of BLP and firmware(s) for embedded processors in BLP. In 1RP platforms *all* the shell peripherals are present in the BLP. In 2RP platforms only a small set of peripherals such as ICAP, QSPI controller, AXI Firewalls etc. are present in BLP.

BLP exports unique interface UUID which is used to match the next stage of the platform. In 1RP platforms when loading ULP, the management driver reads the parent interface UUID stored in the ULP xclbin and matches it with child UUID exported by BLP to determine if xclbin -- which describes the role -- the role is compatible with the BLP.

In 2RP platforms when loading PLP, the management driver reads the parent interface UUID stored in the PLP xsabin and matches it with child UUID exported by BLP to determine if xsabin -- which describes the PLP -- is compatible with the BLP.

PLP
===

.. figure:: XSA-shell-partitions-2RP.svg
    :figclass: align-center

    Alveo shell partitions and loading for 2RP platform

PLP partition is present in 2RP platforms and contains additional shell components such as PCIe DMA engine, AXI Firewalls, Address Re-mapper, ERT, etc. PLP partition is explicitly loaded by system administrator using ``xbmgmt partition`` command. After PLP partition is loaded, xclmgmt driver sends ``XCL_MAILBOX_REQ_MGMT_STATE`` to the xocl driver via mailbox to indicate shell changed. xocl driver then requests for metadata via ``XCL_MAILBOX_REQ_PEER_DATA`` opcode. xclmgmt driver responds by sending information about shell components such as XDMA that should be managed by xocl driver.

ULP
===

ULP partition contains user compiled components like acceleration engines/compute kernels, Memory subsystem, etc. ULP is loaded by the end user using ``xbutil program`` command.

.. note::
   Refer to :ref:`mailbox.proto.rst` for details on inter driver interaction.
