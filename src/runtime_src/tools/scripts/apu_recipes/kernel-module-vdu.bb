SUMMARY = "Linux kernel module for Video Decode Unit"
DESCRIPTION = "Out-of-tree VDU decoder common kernel modules provider for Telluride"
SECTION = "kernel/modules"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE.md;md5=eb723b61539feef013de476e68b5c50a"

XILINX_VDU_VERSION = "1.0.0"
PV = "${XILINX_VDU_VERSION}-xilinx-${XILINX_RELEASE_VERSION}+git${SRCPV}"

S = "${WORKDIR}/git"

BRANCH ?= "master"
REPO ?= "git://gitenterprise.xilinx.com/xilinx-vcu/vdu-modules.git;protocol=https"
SRCREV ?= "${AUTOREV}"

BRANCHARG = "${@['nobranch=1', 'branch=${BRANCH}'][d.getVar('BRANCH', True) != '']}"
SRC_URI = "${REPO};${BRANCHARG}"

inherit module

EXTRA_OEMAKE += "O=${STAGING_KERNEL_BUILDDIR}"

RDEPENDS:${PN} = "vdu-firmware"

COMPATIBLE_MACHINE = "^$"
COMPATIBLE_MACHINE:versal = "versal"

PACKAGE_ARCH = "${SOC_FAMILY_ARCH}"
