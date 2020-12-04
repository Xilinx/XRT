.. _xbmgmt.rst:

xbmgmt
------

Xilinx® Board Management (xbmgmt) utility is a standalone command line tool that is included
with the XRT installation package. The ``xbmgmt`` command supports both Alveo Data Center 
accelerator cards, and embedded processor-based platforms.

This utility is used for card installation and administration, and requires sudo privileges when
running it. The ``xbmgmt`` supported tasks include flashing the card firmware, and scanning the
current device configuration.

Flashing a card
~~~~~~~~~~~~~~~~
1. ``xbmgmt flash --update``
2. Cold reboot
3. Run ``xbmgmt flash --scan`` to verify that the card is flashed correctly

Downloading shell on a card
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Note: On Linux, please run the following with ``--new`` option
1. ``xbmgmt program --partition <path/to/partition.xsabin> --device 0000:00:00.0``
2. Run ``xbmgmt status`` to verify the partition download


For more details please refer `Vitis Application Acceleration Development Flow Documentation <https://www.xilinx.com/html_docs/xilinx2020_2/vitis_doc/xbmgmtutility.html#utg1569948694132>`_
