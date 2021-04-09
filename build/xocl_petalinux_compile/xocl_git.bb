SUMMARY  = "Xilinx Runtime(XRT) driver module"
DESCRIPTION = "Xilinx Runtime driver module provides memory management and compute unit schedule"

LICENSE = "GPLv2 & Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

BRANCH ?= "master"
REPO ?= "git://github.com/Xilinx/XRT.git;protocol=https"
BRANCHARG = "${@['nobranch=1', 'branch=${BRANCH}'][d.getVar('BRANCH', True) != '']}"
SRC_URI = "${REPO};${BRANCHARG}"

PV = "202110.2.11.0"
SRCREV ?= "${AUTOREV}"

S = "${WORKDIR}/git/src/runtime_src/core/pcie/driver/linux/xocl"

inherit module

pkg_postinst_ontarget_${PN}() {
  #!/bin/sh
  echo "Unloading old XRT Linux kernel modules"
  ( rmmod xocl || true ) > /dev/null 2>&1
  echo "Loading new XRT Linux kernel modules"
  modprobe xocl
}

