XRT ChangeLog
-------------

2.1.0 (201830.2.1 unreleased)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Added
.....

* xbutil can now generate output in JSON format for easy parsing by other tools. Use ``xbutil dump`` to generate JSON output on stdout.
* Initial support for PCIe peer-to-peer transactions has been added. Please consult https://xilinx.github.io/XRT/2018.3/html/p2p.html for details.
* 64-bit BARs in Alveo shells are natively supported.
* Initial implementation of XRT logging API, xclLogMsg() for use by XRT clients.
* Initial support for Alveo shell KDMA feature in OpenCL.
* Yocto recipes to build XRT for embedded platforms. Please consult https://xilinx.github.io/XRT/2018.3/html/yocto.html for details.

Fixed
.....

* ``xbutil flash -a`` PROM corruption issue with multiple Alveo boards.
* XRT scheduling bug with multiple boards on AWS F1 when scheduler was serializing board access.
* xocl kernel driver bugs in handling multiple processes accessing the same device.
* PPC64LE build failure.
* Several core QDMA driver fixes.
* xocl scheduler thread now yields correctly when running in polling mode.
* Several Coverity/Fortify code scan fixes.

Deprecated
..........

* XMA plugin API xma_plg_register_write has been marked for deprecation. It will be removed in a future release.
* XMA plugin API xma_plg_register_read has been marked for deprecation. It will be removed in a future release.
