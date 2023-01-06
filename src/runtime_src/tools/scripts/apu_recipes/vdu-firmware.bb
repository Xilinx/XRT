SUMMARY = "Firmware for VDU"
DESCRIPTION = "Firmware binaries provider for VDU"
LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://LICENSE.md;md5=9a0e309e0fae4f23561b63b72f69e77f"

XILINX_VDU_VERSION = "1.0.0"
PV = "${XILINX_VDU_VERSION}-xilinx-${XILINX_RELEASE_VERSION}+git${SRCPV}"

S  = "${WORKDIR}/git"

inherit autotools features_check

REQUIRED_MACHINE_FEATURES = "vdu"

BRANCH ?= "master"
REPO ?= "git://gitenterprise.xilinx.com/xilinx-vcu/vdu-firmware.git;protocol=https"
SRCREV ?= "baf73ff6e31fec2aea457e401fc46504c4bdbe5e"

BRANCHARG = "${@['nobranch=1', 'branch=${BRANCH}'][d.getVar('BRANCH', True) != '']}"
SRC_URI   = "${REPO};${BRANCHARG}"

COMPATIBLE_MACHINE = "^$"
COMPATIBLE_MACHINE:versal-ai-core = "versal-ai-core"
COMPATIBLE_MACHINE:versal-ai-edge = "versal-ai-edge"

PACKAGE_ARCH = "${SOC_FAMILY_ARCH}"
EXTRA_OEMAKE +="INSTALL_PATH=${D}/lib/firmware"

do_compile[noexec] = "1"
do_install[dirs] = "${S}"

# Inhibit warnings about files being stripped
INHIBIT_PACKAGE_DEBUG_SPLIT = "1"
INHIBIT_PACKAGE_STRIP = "1"
FILES:${PN} = "/lib/firmware/*"


# These libraries shouldn't get installed in world builds unless something
# explicitly depends upon them.
EXCLUDE_FROM_WORLD = "1"

INSANE_SKIP:${PN} = "ldflags"
