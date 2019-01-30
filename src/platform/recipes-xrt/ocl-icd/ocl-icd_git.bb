# Recipe created by recipetool

# The LICENSE is BSD 2-Clause "Simplified" License see https://github.com/OCL-dev/ocl-icd/blob/master/COPYING
# This license has also been called the "Simplified BSD License" and the "FreeBSD License". Ref https://opensource.org/licenses/BSD-2-Clause
LICENSE = "FreeBSD"
LIC_FILES_CHKSUM = "file://COPYING;md5=232257bbf7320320725ca9529d3782ab"

SRC_URI = "git://github.com/OCL-dev/ocl-icd.git;protocol=https"

# Modify these as desired
PV = "2.2.12+git${SRCPV}"
SRCREV = "8bf11fd50d447511d1d54717b58813b18e92368e"

S = "${WORKDIR}/git"

# NOTE: the following prog dependencies are unknown, ignoring: xmlto asciidoc a2x
DEPENDS = "ruby-native"

# NOTE: if this software is not capable of being built in a separate build directory
# from the source, you should replace autotools with autotools-brokensep in the
# inherit line
inherit autotools

# Specify any options you want to pass to the configure script using EXTRA_OECONF:
EXTRA_OECONF = ""

