SUMMARY  = "Host API C++ bindings"
DESCRIPTION = "OpenCL compute API headers C++ bindings from Khronos Group"
LICENSE  = "Khronos"
LIC_FILES_CHKSUM = "file://LICENSE.txt;md5=7e4a01f0c56b39419aa287361a82df00"
SECTION = "base"

S = "${WORKDIR}/git"
SRCREV = "806646c283aad2db96212063478f0fb06abf0882"
SRC_URI = "git://github.com/KhronosGroup/OpenCL-CLHPP.git"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

ALLOW_EMPTY_${PN} = "1"

do_install () {
	install -d ${D}${includedir}/CL/
	install -m 0644 ${S}/input_cl2.hpp ${D}${includedir}/CL/cl2.hpp
}
