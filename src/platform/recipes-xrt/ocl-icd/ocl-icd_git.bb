SUMMARY = "OpenCL ICD library"
DESCRIPTION = "Open Source alternative to vendor specific OpenCL ICD loaders."

# The LICENSE is BSD 2-Clause "Simplified" License
LICENSE = "BSD-2-Clause"
LIC_FILES_CHKSUM = "file://COPYING;md5=232257bbf7320320725ca9529d3782ab"

SRC_URI = "git://github.com/OCL-dev/ocl-icd.git;protocol=https"

PV = "2.2.12+git${SRCPV}"
SRCREV = "8bf11fd50d447511d1d54717b58813b18e92368e"

S = "${WORKDIR}/git"

inherit autotools

DEPENDS = "ruby-native"

BBCLASSEXTEND = "native"
