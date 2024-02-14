inherit externalsrc
EXTERNALSRC = "/scratch/predutta/XRT_clk_2/XRT_cr_clck_2/src"
EXTRA_OECMAKE += "-DMY_VITIS=/proj/xbuilds/SWIP/2023.2_1013_2256/installs/lin64/Vitis/2023.2"
EXTERNALSRC_BUILD = "${WORKDIR}/build"
DEPENDS += " systemtap"
PACKAGE_CLASSES = "package_rpm"
LICENSE = "GPLv2 & Apache-2.0"
LIC_FILES_CHKSUM = "file://../LICENSE;md5=de2c993ac479f02575bcbfb14ef9b485 \
                    file://runtime_src/core/edge/drm/zocl/LICENSE;md5=7d040f51aae6ac6208de74e88a3795f8 "
