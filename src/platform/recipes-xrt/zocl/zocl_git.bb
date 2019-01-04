# Xilinx Runtime driver module

LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

SRC_URI = "git://github.com/Xilinx/XRT.git;protocol=https"

PV = "2.2.0+git${SRCPV}"
# Since this commit, XRT cmake is yocto friendly
#SRCREV = "e9fa36422b4590d55eccee02b20173cc305e620c"

# Use latest version
SRCREV = "${AUTOREV}"

S = "${WORKDIR}/git/src/runtime_src/driver/zynq/drm/zocl"

inherit module

