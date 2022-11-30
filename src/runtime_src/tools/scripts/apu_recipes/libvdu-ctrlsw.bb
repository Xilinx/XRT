SUMMARY = "Control Software for VDU"
DESCRIPTION = "Control software libraries, test applications and headers provider for VDU"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE.md;md5=c15ae83ba15c4ab3fa4eb2c22975258f"

XILINX_VDU_VERSION = "1.0.0"
PV = "${XILINX_VDU_VERSION}-xilinx-${XILINX_RELEASE_VERSION}+git${SRCPV}"

BRANCH ?= "master"
REPO   ?= "git://gitenterprise.xilinx.com/xilinx-vcu/vdu-ctrl-sw.git;protocol=https"
SRCREV ?= "${AUTOREV}"

BRANCHARG = "${@['nobranch=1', 'branch=${BRANCH}'][d.getVar('BRANCH', True) != '']}"
SRC_URI = "${REPO};${BRANCHARG}"

S  = "${WORKDIR}/git"

COMPATIBLE_MACHINE = "^$"
COMPATIBLE_MACHINE:versal = "versal"

PACKAGE_ARCH = "${SOC_FAMILY_ARCH}"

RDEPENDS:${PN} = "kernel-module-vdu"

EXTRA_OEMAKE = "CC='${CC}' CXX='${CXX} ${CXXFLAGS}'"

do_install() {
    install -d ${D}${libdir}
    install -d ${D}${includedir}/vdu-ctrl-sw/include

    install -Dm 0755 ${S}/bin/AL_Decoder.exe ${D}/${bindir}/ctrlsw_decoder

    oe_runmake install_headers INSTALL_HDR_PATH=${D}${includedir}/vdu-ctrl-sw/include
    oe_libinstall -C ${S}/bin/ -so liballegro_decode ${D}/${libdir}/
}

# These libraries shouldn't get installed in world builds unless something
# explicitly depends upon them.

EXCLUDE_FROM_WORLD = "1"
