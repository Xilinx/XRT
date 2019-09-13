.. _cloud_vendor_support.rst:


MSD/MPD and Plugins
*******************

Overview
========

When FPGAs are deployed at cloud vendors, either VM based (IaaS) or container based (PaaS), there are
some common concerns need to be addressed.

1. MGMT PF and USER PF of the FPGA are separated
 
   Cloud vendors own the MGMT PF, while users own the USER PF. Any operations by the user on USER PF 
   should not damage or compromise the operation of MGMT PF.

2. xclbin files needs to be protected

   Some of the xclbin files are provided by third-party ISVs -- they don't want to the users access their
   xclbin files, but use them indirectly. That is, the xclbin files in user VM or container are not the 
   real ones that are running on the cards. Instead, they are fake one with the BITSTREAM section stripped.
   xclbin download on the fake xclbin files in VM should result in the real one being programed without any
   user perceiving

3. Cloud vendors have more control on the xclbin download process

   Download xclbin involves the talking between VMs or containers and the hosts. Cloud vendors have their
   own ways they trust to do this.

XRT addresses all of these concerns

Mailbox, Message Service Daemon(MSD) and Message Proxy Daemon(MPD)
==================================================================

The following picture illustrates how a xclbin file is downloaded on baremetal machine

.. image:: mailbox-msd-mpd-architecture.svg
   :align: center

As shown by the red arrows in the picture, the flow of the download in this case is as following:

1. User requests xclbin download from ‘xbutil program’ cmdline or OpenCL API 
2. Shim layer issues ioctl to xocl driver
3. Xocl driver subdev icap sends the req to subdev mailbox (the xclbin file is in host memory, the req  size is very small)
4. Mailbox transfers the req to the peer in xclmgmt through hw mailbox
5. Xclmgmt driver subdev mailbox forwards the req to the  subdev icap
6. Xclmgmt driver subdev icap receives the req, gets xclbin file from memory, and programs the icap
7. Response is sent back to the user following the reverse way

**Note:** *This model also works for containers running on top of baremetal if no special requirement, ie, xclbin protection,
is needed*

XRT has its xclmgmt and xocl driver separated. Mailbox is communication channel between the 2 drivers, with
which the user can do some management works with USER PF in VM, ie. download xclbin. However, HW mailbox by
design has very low bandwidth, and that makes transfer of a hundred Mega Byte xclbin file very slow. SW mailbox
is the complementary to the mailbox framework to overcome this, and it also helps as well for cases where there
are no working HW mailbox

SW mailbox relies on MSD/MPD. MSD resides in userspace of the machine(ie. Host) where xclmgmt driver is installed,
while MPD resides in userspace of the machine(ie. VM) where xocl driver is installed. They talk to the mailbox subdev
in the corresponding driver. MSD/MPD may be connected through external networking, eg. Ethernet, and make the download
xclbin faster

**Note:** *In order to use SW mailbox, the cloud vendor has to setup the networking connection between Host and VM.
And this model provides a faster xclbin download, but doesn't provide xclbin protection*

Once the networking connection is setup, the following configurations are also required

.. code-block:: bash

        # In host, make sure the IP address configured is the one VM talks to
        Host>$ sudo xbmgmt config --show

        # If the IP address is not correct, change it by running
        Host>$ sudo xbmgmt config --daemon --host <host-or-ip>

        # Start MSD in host
        Host>$ sudo systemctl start msd

        # Start MPD in VM
        VM>$ sudo systemctl start mpd

The flow of downloading xclbin through SW mailbox and MSD/MPD is illustrated as below:

.. image:: sw-mailbox-msd-mpd-download.svg
   :align: center

As shown by the green arrows in the picture, the flow of the download in this case is as following:

1. User requests xclbin download from ‘xbutil program’ cmdline or OpenCL API 
2. Shim layer issues ioctl to xocl driver
3. Xocl driver subdev icap sends the req to subdev mailbox 
4. Mailbox transfers the req  and xclbin file to MPD as mailbox message
5. MPD forwards the mailbox message to MSD
6. MSD transfers the mailbox message to xclmgmt driver subdev mailbox
7. Xclmgmt driver subdev mailbox forwards the req to the  subdev icap
8. Xclmgmt driver subdev icap receives the req, and programs the icap
9. Response is sent back to the user following the reverse way


The mailbox(HW&SW) and MSD/MPD framework perfectly addresses the 1st concern mentioned above

Enhancement to MPD
==================

MSD/MPD are mailbox message centric. They focus on the delivering of the mailbox message and don't interpret them.
In order to protect the xclbin file, in which case users feed fake xclbin files to xocl then plugins get real ones
and re-feed to xclmgmt, MSD/MPD have to interpret and understand the download xclbin message. An enhancement to MPD
interprets the mailbox message and calls into vendor specific plugin to download the xclbin

The input to the plugin is the xclbin file fed by the user in VM or container -- it may be a fake xclbin file. The
plugin calls cloud vendor specific APIs to do the real download. It is the cloud vendor responsibility to,

1. Save the real xclbin files in a dedicated database
2. Retrieve the real xclbin from fake one
3. Ascertain the legality of the download itself
4. Talk to the MGMT PF (xclmgmt driver) to download the real xclbin

**Note:** *In this model, the cloud vendor APIs don't know anything about mailbox. They talk to ICAP through ioctl directly. So
MSD is not being used*

The flow of downloading protected xclbin through plugin is illustrated as below:

.. image:: sw-mailbox-mpd-plugin-download.svg
   :align: center

The vendor private part shown in the picture needs to,

1. Provide database to save real xclbin files
2. Provide download API to MPD plugin
3. Check the legality of the download
    i. whether the user is authorized
    ii. whether the xclbin is valid
    iii. whether the FPGA owned by the user
    iv. etc
4. Retrieve the real xclbin
5. Download the retrieved xclbin

The enhancement to the MPD and the plugin address the 2nd and 3rd concerns mentioned above

Example MPD plugin
==================

The example plugin aims at containers running on top of baremetal machines. In this case, both MGMT PF and USER PF are in the same
domain, so plugin can call ioctl on xclmgmt directly to program ICAP after it retrieves the real xclbin. This is the use case
for Nimbix

The plugin is built as shared object -- libcontainer_mpd_plugin.so, and when users install the container pkg, the 'so' file
will be installed at /opt/xilinx/xrt/lib, and a soft link file -- libmpd_plugin.so is created under the same folder
linking to the plugin shared object. MPD tries to dlopen(3) the shared object when it gets started

This delivered container plugin by default just uses the input xclbin file as output(that means no xclbin protection),
show-casing how this plugin is going to be implemented. It does have example code how to save real xclbin, how to retrieve
real xclbin from fake one, and how to download a protected xclbin, as user's reference

This plugin can also be used for internal test on the MPD and mailbox

Example how a ubuntu host of containers configures the plugin

.. code-block:: bash

        # install xrt pkg
        $ sudo apt install /opt/xrt_201920.2.3.0_18.04-xrt.deb

        # install xrt pkg
        $ sudo apt install /opt/xrt_201920.2.3.0_18.04-container.deb

        # config mailbox channel switch
        # this has to be manually configurated to ensure download xclbin going through SW mailbox
        $ sudo echo 0x100 > /sys/bus/pci/devices/0000\:65\:00.0/config_mailbox_channel_switch

        # When cloud vendor (eg. Nimbix) wants to enable its own xclbin protection mechanism, this
        # plugin needs to be rebuilt and the built 'so' needs to be copied to /opt/xilinx/xrt/lib
        # eg
        $ sudo cp libcontainer_mpd_plugin.so /opt/xilinx/xrt/lib
        $ sudo systemctl restart mpd

Summary
=======

With the MSD/MPD framework and MPD enhancement, 

1. Same XRT pkg is installed everywhere, baremetal/IaaS/PaaS/etc. Vendors only need to create/install their
   specific plugins
2. Users have same Xilinx FPGA using experience everywhere -- they don't even know whether they are running
   within baremetal, VM, or containers, they don't know whether the xclbin files they see are real one, fake
   one or any other kind either

The following picture illustrates how XRT is being deployed in different scenarios at cloud vendors

.. image:: xrt-deployment-cloud.svg
   :align: center
