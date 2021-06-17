.. _fpga_device_ready.rst:


FPGA device readiness within user VM
************************************

To make the FPGA in user VM ready to use, the mgmt side has to be ready first. Beside that, depending on how
the FPGA is deployed, there are still other factors to be considered.

With MSD/MPD or Plugins used
============================

In this case, :ref:`mailbox.main.rst` has to be opened from guest VM, and  MPD service must be active in the guest VM.

For MSD/MPD, the MSD service must be active also either from hypervisor or dom0 type VM where the mgmt PF of the FPGA
is assigned.

For Plugins, the vendor specific plugin package has to be installed

Without MSD/MPD or Plugins used
===============================
  
In this case, :ref:`mailbox.main.rst` is still being used, but there is no open to the mailbox subdevice from user,
and MPD service is inactive.
To make the FPGA device ready, the XCL_MAILBOX_REQ_LOAD_XCLBIN opcode has to be disabled in xclmgmt.
System administrator managing the privileged management physical function driver xclmgmt can disable specific opcodes
using xbmgmt utility as follows.
 
.. code-block:: bash

        # In host 
        Host>$ sudo xbmgmt dump --config --output /tmp/config.ini -d bdf

        # Edit the dumped ini file and change the value to key 'mailbox_channel_disable' 
        # mailbox_channel_disable=0x100,
        # where 0x100 is 1 << XCL_MAILBOX_REQ_LOAD_XCLBIN, as defined as below
        # XCL_MAILBOX_REQ_UNKNOWN =             0,
        # XCL_MAILBOX_REQ_TEST_READY =          1,
        # XCL_MAILBOX_REQ_TEST_READ =           2,
        # XCL_MAILBOX_REQ_LOCK_BITSTREAM =      3,
        # XCL_MAILBOX_REQ_UNLOCK_BITSTREAM =    4,
        # XCL_MAILBOX_REQ_HOT_RESET =           5,
        # XCL_MAILBOX_REQ_FIREWALL =            6,
        # XCL_MAILBOX_REQ_LOAD_XCLBIN_KADDR =   7,
        # XCL_MAILBOX_REQ_LOAD_XCLBIN =         8,
        # XCL_MAILBOX_REQ_RECLOCK =             9,
        # XCL_MAILBOX_REQ_PEER_DATA =           10,
        # XCL_MAILBOX_REQ_USER_PROBE =          11,
        # XCL_MAILBOX_REQ_MGMT_STATE =          12,
        # XCL_MAILBOX_REQ_CHG_SHELL =           13,
        # XCL_MAILBOX_REQ_PROGRAM_SHELL =       14,
        # XCL_MAILBOX_REQ_READ_P2P_BAR_ADDR =   15,

        Host>$ vi /tmp/config.ini

        # Load config
        Host>$ xbmgmt advanced --load-conf --input=/tmp/config.ini -d bdf
