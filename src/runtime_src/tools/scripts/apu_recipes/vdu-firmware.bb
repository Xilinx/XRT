SUMMARY = "Firmware for VDU"
DESCRIPTION = "Firmware binaries provider for VDU"
LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://LICENSE.md;md5=7ab4113b0b7ac6f3bd45429a8a6d5a58"

XILINX_VDU_VERSION = "1.0.0"
PV = "${XILINX_VDU_VERSION}-xilinx-${XILINX_RELEASE_VERSION}+git${SRCPV}"

S  = "${WORKDIR}/git"

BRANCH ?= "master"
REPO ?= "git://gitenterprise.xilinx.com/xilinx-vcu/vdu-firmware.git;protocol=https"
SRCREV ?= "${AUTOREV}"

BRANCHARG = "${@['nobranch=1', 'branch=${BRANCH}'][d.getVar('BRANCH', True) != '']}"
SRC_URI   = "${REPO};${BRANCHARG}"

COMPATIBLE_MACHINE = "^$"
COMPATIBLE_MACHINE:versal = "versal"

PACKAGE_ARCH = "${SOC_FAMILY_ARCH}"

do_install() {
    install -Dm 0644 ${S}/${XILINX_VDU_VERSION}/lib/firmware/al5d_b.fw ${D}/lib/firmware/al5d_b.fw
    install -Dm 0644 ${S}/${XILINX_VDU_VERSION}/lib/firmware/al5d.fw ${D}/lib/firmware/al5d.fw
}

# Inhibit warnings about files being stripped
INHIBIT_PACKAGE_DEBUG_SPLIT = "1"
INHIBIT_PACKAGE_STRIP = "1"
FILES:${PN} = "/lib/firmware/*"

# These libraries shouldn't get installed in world builds unless something
# explicitly depends upon them.
EXCLUDE_FROM_WORLD = "1"

INSANE_SKIP:${PN} = "ldflags"
