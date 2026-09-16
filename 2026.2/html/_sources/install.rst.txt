.. _install.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.

XRT Installation
----------------

Install XRT Prerequisites on Deployment Server
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XRT requires EPEL to install dependencies during installation process. Please use the following steps to install EPEL on your system if it hasn't been installed.

.. Warning:: If it's on the XRT build server, EPEL should have been installed by ``xrtdeps.sh``. This step can be skipped.

Steps for RHEL 9.x::

        sudo subscription-manager repos --enable "codeready-builder-for-rhel-9-x86_64-rpms"
        sudo yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm

Steps for RHEL 10.x::

        sudo subscription-manager repos --enable "codeready-builder-for-rhel-10-x86_64-rpms"
        sudo yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-10.noarch.rpm

Steps for AlmaLinux 9.x/Rocky 9.x::

        sudo dnf config-manager --set-enabled crb
        sudo dnf install epel-release

Steps for AlmaLinux 10.x/Rocky 10.x::

        sudo dnf config-manager --set-enabled crb
        sudo dnf install epel-release


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

Starting with 2026.1 XRT comes with separate *deployment* and *development* packages. The deployment package is sufficient to execute a precompiled XRT application. However, the development package is required to compile an XRT application. The development package includes XRT header files, CMake package files for discovery by CMake based application build environments, and XRT dynamic/static libraries for linking.

.. note::

   .deb style XRT development packages have a *-dev* suffix in the package name. .rpm style XRT development packages have a *-devl* suffix in the package name.


.. Warning::

    1. If the XRT package is built locally, please make sure ERT firmware ``sched*.bin`` is built properly during build process and installed to ``/lib/firmware/xilinx`` after running the XRT installation command.

    2. Secure boot enabled machines: Need to configure system to properly load DKMS modules.
       Please follow method-1 from following page. You do not need to disable secure boot. First time DKMS compiles XRT (or any other third party) driver it will generate a MOK key that needs to be registered with BIOS.

       https://wiki.ubuntu.com/UEFI/SecureBoot/DKMS
