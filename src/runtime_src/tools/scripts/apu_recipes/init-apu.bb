SUMMARY = "Init apu service"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
 
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
 
SRC_URI = "file://init-apu file://init-apu.service "
 
S = "${WORKDIR}"
 
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
 
inherit update-rc.d systemd
 
INITSCRIPT_NAME = "init-apu"
INITSCRIPT_PARAMS = "start 99 S ."

SYSTEMD_PACKAGES = "${PN}"
SYSTEMD_SERVICE:${PN} = "init-apu.service"
SYSTEMD_AUTO_ENABLE:${PN}="enable"
 
do_install() {
        if ${@bb.utils.contains('DISTRO_FEATURES', 'sysvinit', 'true', 'false', d)}; then
                install -d ${D}${sysconfdir}/init.d/
                install -m 0755 ${WORKDIR}/init-apu ${D}${sysconfdir}/init.d/
        fi

	install -d ${D}${bindir}
	install -m 0755 ${WORKDIR}/init-apu ${D}${bindir}/
        install -d ${D}${systemd_system_unitdir}
        install -m 0644 ${WORKDIR}/init-apu.service ${D}${systemd_system_unitdir}
}
 
FILES:${PN} += "${@bb.utils.contains('DISTRO_FEATURES','sysvinit','${sysconfdir}/*', '', d)}"
