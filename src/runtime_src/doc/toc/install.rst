.. _install.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.

XRT Installation
----------------

Install XRT Pre-requests on Deployment Server
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XRT requires EPEL to install dependencies during installation process. Please use the folloing steps to install EPEL on your system if it hasn't been installed. 

.. Warning:: If it's on the XRT build server, EPEL should have been installed by ``xrtdeps.sh``. This step can be skipped.

Steps for RHEL 7.x::

	sudo yum-config-manager --enable rhel-7-server-optional-rpms
	sudo yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm

Steps for RHEL 8.x::

	sudo subscription-manager repos --enable "codeready-builder-for-rhel-8-x86_64-rpms"
	sudo yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm

Steps for CENTOS 7.x::

	yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm

Steps for CENTOS 8.x::

	yum config-manager --set-enabled PowerTools
	yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm
	yum config-manager --set-enabled AppStream


Install XRT Software Stack
~~~~~~~~~~~~~~~~~~~~~~~~~~

After XRT installation packages (DEB or RPM) are downloaded from Xilinx website or built from source, please install it with the following command

Steps for RHEL/CentOS::

    sudo yum install xrt_<version>.rpm

Steps for Ubuntu::

    sudo apt install xrt_<version>.deb

Steps to reinstall XRT on RHEL/CentOS::

    sudo yum reinstall ./xrt_<version>.rpm

Steps to reinstall XRT on Ubuntu::

    sudo apt install --reinstall ./xrt_<version>.deb

.. Warning::

    1. If the XRT package is built locally, please make sure ERT firmware ``sched*.bin`` is built properly during build process and installed to ``/lib/firmware/xilinx`` after running the XRT installation command.

    2. Secure boot enabled machines: Need to configure system to properly load DKMS modules.
       Please follow method-1 from following page. You do not need to disable secure boot. First time DKMS compiles XRT (or any other third party) driver it will generate a MOK key that needs to be registered with BIOS.

       https://wiki.ubuntu.com/UEFI/SecureBoot/DKMS
