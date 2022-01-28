#!/bin/bash

bold=$(tput bold)
normal=$(tput sgr0)
red=$(tput setaf 1)

error()
{
    echo "ERROR: $1" 1>&2
    usage_and_exit 1
}

usage()
{
    echo "Usage: $PROGRAM [options] "
    echo "  options:"
    echo "          -help                           Print this usage"
    echo "          -aarch                          Architecture <aarch32/aarch64/versal>"
    echo "          -cache                          path to sstate-cache"
    echo "          -setup                          setup file to use"
    echo "          -clean, clean                   Remove build directories"
    echo "          -full, full                     Full Petalinux build which builds images along with XRT RPMs"
    echo ""
}

usage_and_exit()
{
    usage
    exit $1
}

# --- Internal funtions ---
install_recipes()
{
    META_USER_PATH=$1

    SAVED_OPTIONS_LOCAL=$(set +o)
    set +e
    mkdir -p ${META_USER_PATH}/recipes-xrt/xrt
    mkdir -p ${META_USER_PATH}/recipes-xrt/zocl
    XRT_BB=${META_USER_PATH}/recipes-xrt/xrt/xrt_git.bbappend
    ZOCL_BB=${META_USER_PATH}/recipes-xrt/zocl/zocl_git.bbappend
    grep "inherit externalsrc" $XRT_BB
    if [ $? != 0 ]; then
        echo "inherit externalsrc" > $XRT_BB
        echo "EXTERNALSRC = \"$XRT_REPO_DIR/src\"" >> $XRT_BB
        echo 'EXTERNALSRC_BUILD = "${WORKDIR}/build"' >> $XRT_BB
        echo "FILES:\${PN} += \"\${libdir}/ps_kernels_lib\"" >> $XRT_BB
        echo 'PACKAGE_CLASSES = "package_rpm"' >> $XRT_BB
        echo 'LICENSE = "GPLv2 & Apache-2.0"' >> $XRT_BB
        echo 'LIC_FILES_CHKSUM = "file://../LICENSE;md5=da5408f748bce8a9851dac18e66f4bcf \' >> $XRT_BB
        echo '                    file://runtime_src/core/edge/drm/zocl/LICENSE;md5=7d040f51aae6ac6208de74e88a3795f8 "' >> $XRT_BB
    fi

    grep "inherit externalsrc" $ZOCL_BB
    if [ $? != 0 ]; then
        echo "inherit externalsrc" > $ZOCL_BB
        echo "EXTERNALSRC = \"$XRT_REPO_DIR/src/runtime_src/core/edge/drm/zocl\"" >> $ZOCL_BB
        echo "EXTERNALSRC_BUILD = \"$XRT_REPO_DIR/src/runtime_src/core/edge/drm/zocl\"" >> $ZOCL_BB
        echo 'PACKAGE_CLASSES = "package_rpm"' >> $ZOCL_BB
        echo 'LICENSE = "GPLv2 & Apache-2.0"' >> $ZOCL_BB
        echo 'LIC_FILES_CHKSUM = "file://LICENSE;md5=7d040f51aae6ac6208de74e88a3795f8"' >> $ZOCL_BB
        echo 'pkg_postinst_ontarget_${PN}() {' >> $ZOCL_BB
        echo '  #!/bin/sh' >> $ZOCL_BB
        echo '  echo "Unloading old XRT Linux kernel modules"' >> $ZOCL_BB
        echo '  ( rmmod zocl || true ) > /dev/null 2>&1' >> $ZOCL_BB
        echo '  echo "Loading new XRT Linux kernel modules"' >> $ZOCL_BB
        echo '  modprobe zocl' >> $ZOCL_BB
        echo '}' >> $ZOCL_BB
    fi
    eval "$SAVED_OPTIONS_LOCAL"
}

config_versal_project()
{
    # remove following unused packages from rootfs sothat its size would fit in QSPI

    sed -i 's/^CONFIG_packagegroup-petalinux-opencv.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_packagegroup-petalinux-jupyter.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_kernel-devsrc.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_xrt-dev.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_e2fsprogs-mke2fs.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_tcl.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_opencl-clhpp-dev.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_opencl-headers.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_libstdcPLUSPLUS.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_resize-part.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_packagegroup-petalinux-x11.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_imagefeature-hwcodecs.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_htop.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_iperf3.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_meson.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_imagefeature-ssh-server-dropbear.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_imagefeature-package-management.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_imagefeature-debug-tweaks.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_dnf.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_python3.*//g' project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_package-feed-uris.*//g' project-spec/configs/rootfs_config

    # Configure u-boot to pick dtb from address 0x40000
    UBOOT_USER_SCRIPT=u-boot_custom.cfg
    echo "CONFIG_XILINX_OF_BOARD_DTB_ADDR=0x40000" > project-spec/meta-user/recipes-bsp/u-boot/files/$UBOOT_USER_SCRIPT
    echo "SRC_URI += \"file://${UBOOT_USER_SCRIPT}\"" >> project-spec/meta-user/recipes-bsp/u-boot/u-boot-xlnx_%.bbappend

    # Configure kernel
    echo "CONFIG_SUSPEND=n" >> project-spec/meta-user/recipes-kernel/linux/linux-xlnx/bsp.cfg
    echo "CONFIG_PM=n" >> project-spec/meta-user/recipes-kernel/linux/linux-xlnx/bsp.cfg
    echo "CONFIG_SPI=n" >> project-spec/meta-user/recipes-kernel/linux/linux-xlnx/bsp.cfg
    echo "CONFIG_DRM_XLNX_DSI=n" >> project-spec/meta-user/recipes-kernel/linux/linux-xlnx/bsp.cfg

    # Configure inittab for getty
    INIT_TAB_FILE=project-spec/meta-user/recipes-core/sysvinit/sysvinit-inittab_%.bbappend
    if [ ! -d $(dirname "$INIT_TAB_FILE") ]; then
        mkdir -p $(dirname "$INIT_TAB_FILE")
    fi
cat << EOF > $INIT_TAB_FILE
do_install:append(){
  echo "UL0:12345:respawn:/bin/start_getty 115200 ttyUL0 vt102" >> \${D}\${sysconfdir}/inittab
}
EOF

    # Create startup script to write to sysfs entry to indicate apu booted
    SERVICE_FILE=project-spec/meta-user/recipes-apps/apu-boot/files/apu-boot.service
    BB_FILE=project-spec/meta-user/recipes-apps/apu-boot/apuboot.bb
    INIT_SCRIPT=project-spec/meta-user/recipes-apps/apu-boot/files/apu-boot

    petalinux-create -t apps --template install -n apu-boot --enable

    # Create service file
cat << EOF > $SERVICE_FILE
[Unit]
Description=apu-boot
 
[Service]
ExecStart=/usr/bin/apu-boot
StandardOutput=journal+console
 
[Install]
WantedBy=multi-user.target
EOF

    # Create script to check apu boot
    /bin/rm -rf $INIT_SCRIPT
cat << EOF > $INIT_SCRIPT
#!/bin/sh
if [ -e /sys/bus/platform/devices/rpu-channel/ready ]; then
	echo 1 > /sys/bus/platform/devices/rpu-channel/ready
fi
EOF

    # Recreate bb file
    /bin/rm -rf $BB_FILE
cat << EOF > $BB_FILE
SUMMARY = "apu-boot application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
 
LIC_FILES_CHKSUM = "file://\${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
 
SRC_URI = "file://apu-boot \
        file://apu-boot.service \
"
 
S = "\${WORKDIR}"
 
FILESEXTRAPATHS:prepend := "\${THISDIR}/files:"
 
inherit update-rc.d systemd
 
INITSCRIPT_NAME = "apu-boot"
INITSCRIPT_PARAMS = "start 99 S ."
 
SYSTEMD_PACKAGES = "\${PN}"
SYSTEMD_SERVICE:\${PN} = "apu-boot.service"
SYSTEMD_AUTO_ENABLE:\${PN}="enable"
 
do_install() {
        if \${@bb.utils.contains('DISTRO_FEATURES', 'sysvinit', 'true', 'false', d)}; then
                install -d \${D}\${sysconfdir}/init.d/
                install -m 0755 \${WORKDIR}/apu-boot \${D}\${sysconfdir}/init.d/
        fi
 
        install -d \${D}\${bindir}
        install -m 0755 \${WORKDIR}/apu-boot \${D}\${bindir}/
        install -d \${D}\${systemd_system_unitdir}
        install -m 0644 \${WORKDIR}/apu-boot.service \${D}\${systemd_system_unitdir}
}
 
FILES:\${PN} += "\${@bb.utils.contains('DISTRO_FEATURES','sysvinit','\${sysconfdir}/*', '', d)}"
EOF
 
}

# --- End internal functions

SAVED_OPTIONS=$(set +o)

# Don't print all commands
set +x
# Error on non-zero exit code, by default:
set -e

# Get real script by read symbol link
THIS_SCRIPT=`readlink -f ${BASH_SOURCE[0]}`

THIS_SCRIPT_DIR="$( cd "$( dirname "${THIS_SCRIPT}" )" >/dev/null 2>&1 && pwd )"

PROGRAM=`basename $0`
CONFIG_FILE=""
PETA_BSP=""
PROJ_NAME=""
PLATFROM=""
XRT_REPO_DIR=`readlink -f ${THIS_SCRIPT_DIR}/..`
clean=0
full=0
SSTATE_CACHE=""
SETTINGS_FILE="petalinux.build"
while [ $# -gt 0 ]; do
	case $1 in
		-help )
			usage_and_exit 0
			;;
		-aarch )
			shift
			AARCH=$1
			;;
		-setup )
			shift
			SETTINGS_FILE=$1
			;;
		-clean | clean )
			clean=1
			;;
		-full | full )
			full=1
			;;
		-cache )
                        shift
                        SSTATE_CACHE=$1
                        ;;
		--* | -* )
			error "Unregognized option: $1"
			;;
		* )
			error "Unregognized option: $1"
			;;
	esac
	shift
done

aarch64_dir="aarch64"
aarch32_dir="aarch32"
versal_dir="versal"
YOCTO_MACHINE=""

if [[ $clean == 1 ]]; then
    echo $PWD
    echo "/bin/rm -rf $aarch64_dir $aarch32_dir $versal_dir"
    /bin/rm -rf $aarch64_dir $aarch32_dir $versal_dir
    exit 0
fi

# we pick Petalinux BSP
if [ -f $SETTINGS_FILE ]; then
    source $SETTINGS_FILE
fi
source $PETALINUX/settings.sh 

if [[ $AARCH = $aarch64_dir ]]; then
    if [[ -f $PETALINUX/../../bsp/release/zynqmp-common-v$PETALINUX_VER-final.bsp ]]; then
    PETA_BSP="$PETALINUX/../../bsp/release/zynqmp-common-v$PETALINUX_VER-final.bsp"
    elif [[ -f $PETALINUX/../../bsp/internal/zynqmp/zynqmp-common-v$PETALINUX_VER-final.bsp ]]; then
    PETA_BSP="$PETALINUX/../../bsp/internal/zynqmp/zynqmp-common-v$PETALINUX_VER-final.bsp"
    else
    PETA_BSP="$PETALINUX/../../bsp/internal/zynqmp-common-v$PETALINUX_VER-final.bsp"
    fi
    YOCTO_MACHINE="zynqmp-generic"
elif [[ $AARCH = $aarch32_dir ]]; then
    if [[ -f $PETALINUX/../../bsp/release/zynq-rootfs-common-v$PETALINUX_VER-final.bsp ]]; then
    PETA_BSP="$PETALINUX/../../bsp/release/zynq-rootfs-common-v$PETALINUX_VER-final.bsp"
    elif [[ -f $PETALINUX/../../bsp/internal/zynq/zynq-rootfs-common-v$PETALINUX_VER-final.bsp ]]; then
    PETA_BSP="$PETALINUX/../../bsp/internal/zynq/zynq-rootfs-common-v$PETALINUX_VER-final.bsp"
    else
    PETA_BSP="$PETALINUX/../../bsp/internal/zynq-rootfs-common-v$PETALINUX_VER-final.bsp"
    fi
    YOCTO_MACHINE="zynq-generic"
elif [[ $AARCH = $versal_dir ]]; then
    if [[ -f $PETALINUX/../../bsp/release/versal-rootfs-common-v$PETALINUX_VER-final.bsp ]]; then
    PETA_BSP="$PETALINUX/../../bsp/release/versal-rootfs-common-v$PETALINUX_VER-final.bsp"
    elif [[ -f $PETALINUX/../../bsp/internal/versal/versal-rootfs-common-v$PETALINUX_VER-final.bsp ]]; then
    PETA_BSP="$PETALINUX/../../bsp/internal/versal/versal-rootfs-common-v$PETALINUX_VER-final.bsp"
    else
    PETA_BSP="$PETALINUX/../../bsp/internal/versal-rootfs-common-v$PETALINUX_VER-final.bsp"
    fi
    YOCTO_MACHINE="versal-generic"
else
    error "$AARCH not exist"
fi

# Sanity Check

if [ ! -f $PETA_BSP ]; then
    error "$PETA_BSP not accessible"
fi

if [ ! -d $SSTATE_CACHE ]; then
    error "SSTATE_CACHE= not accessible"
fi

# Sanity check done

PETA_CONFIG_OPT="--silentconfig"
ORIGINAL_DIR=`pwd`
PETA_BIN="$PETALINUX/tools/common/petalinux/bin"

echo "** START [${BASH_SOURCE[0]}] **"
echo " PETALINUX: $PETALINUX"
echo ""

PETALINUX_NAME=$AARCH
echo " * Create PetaLinux from BSP (-s $PETA_BSP)"
PETA_CREATE_OPT="-s $PETA_BSP"

if [ ! -d $PETALINUX_NAME ]; then
    echo " * Create PetaLinux Project: $PETALINUX_NAME"
    echo "[CMD]: petalinux-create -t project -n $PETALINUX_NAME $PETA_CREATE_OPT"
    $PETA_BIN/petalinux-create -t project -n $PETALINUX_NAME $PETA_CREATE_OPT
    cd ${PETALINUX_NAME}/project-spec/meta-user/
    install_recipes .
else
  echo "$red $bold INFO: Project Already exists on Disk. Running incremental build $normal"
fi

cd $ORIGINAL_DIR/$PETALINUX_NAME

#cleanup existing files in incremental build
/bin/rm -rf *.rpm
/bin/rm -rf install_xrt.sh
/bin/rm -rf reinstall_xrt.sh
/bin/rm -rf rpms

echo "CONFIG_YOCTO_MACHINE_NAME=\"${YOCTO_MACHINE}\""
echo "CONFIG_YOCTO_MACHINE_NAME=\"${YOCTO_MACHINE}\"" >> project-spec/configs/config 


if [ ! -z $SSTATE_CACHE ] && [ -d $SSTATE_CACHE ]; then
    echo "SSTATE-CACHE:${SSTATE_CACHE} added"
    echo "CONFIG_YOCTO_LOCAL_SSTATE_FEEDS_URL=\"${SSTATE_CACHE}\"" >> project-spec/configs/config
else
    echo "SSTATE-CACHE:${SSTATE_CACHE} not present"
fi

# Build package
echo " * Performing PetaLinux Build (from: ${PWD})"
#Run a full build if -full option is provided
if [[ $full == 1 ]]; then
  if [[ $AARCH = $versal_dir ]]; then
    # configure the project with appropriate options
    config_versal_project
  fi

  echo "[CMD]: petalinux-config -c kernel --silentconfig"
  $PETA_BIN/petalinux-config -c kernel --silentconfig
  echo "[CMD]: petalinux-config -c rootfs --silentconfig"
  $PETA_BIN/petalinux-config -c rootfs --silentconfig
  echo "[CMD]: petalinux-build"
  $PETA_BIN/petalinux-build
else
#Run just xrt build if -full option is not provided
  echo "[CMD]: petalinux-build -c zocl"
  $PETA_BIN/petalinux-build -c zocl
  echo "[CMD]: petalinux-build -c xrt"
  $PETA_BIN/petalinux-build -c xrt
fi

if [ $? != 0 ]; then
   error "XRT build failed"
fi

# zocl is now part of xrt depenendencies. No need to build zocl from petalinux 2021.1+
#echo "[CMD]: petalinux-build -c zocl"
#$PETA_BIN/petalinux-build -c zocl
#if [ $? != 0 ]; then
#   error "ZOCL build failed"
#fi

echo "Copying rpms in $ORIGINAL_DIR/$PETALINUX_NAME"
if [ ! -d build/tmp/deploy/rpm ]; then
  tmp_path=$(cat project-spec/configs/config | grep CONFIG_TMP_DIR_LOCATION \
	| awk -F'=' '{print $2}' |  sed -e 's/^"//' -e 's/"$//')
  cp -v ${tmp_path}/deploy/rpm/*/xrt* $ORIGINAL_DIR/$PETALINUX_NAME/.
  cp -v ${tmp_path}/deploy/rpm/${PLATFORM_NAME}*/*zocl* $ORIGINAL_DIR/$PETALINUX_NAME/.
else
  cp -v build/tmp/deploy/rpm/${PLATFORM_NAME}*/*zocl* $ORIGINAL_DIR/$PETALINUX_NAME/.
  cp -v build/tmp/deploy/rpm/*/xrt* $ORIGINAL_DIR/$PETALINUX_NAME/.
fi

#copying rpms into rpms folder
mkdir -p $ORIGINAL_DIR/$PETALINUX_NAME/rpms
cp -v $ORIGINAL_DIR/$PETALINUX_NAME/xrt* $ORIGINAL_DIR/$PETALINUX_NAME/rpms/.
cp -v $ORIGINAL_DIR/$PETALINUX_NAME/zocl* $ORIGINAL_DIR/$PETALINUX_NAME/rpms/.
cp -v $ORIGINAL_DIR/$PETALINUX_NAME/kernel* $ORIGINAL_DIR/$PETALINUX_NAME/rpms/.

echo "Creating $ORIGINAL_DIR/$PETALINUX_NAME/rpm.txt"
echo `ls xrt-dev*` > $ORIGINAL_DIR/$PETALINUX_NAME/rpm.txt
echo `ls xrt-2*` >> $ORIGINAL_DIR/$PETALINUX_NAME/rpm.txt

echo "Creating $ORIGINAL_DIR/$PETALINUX_NAME/install_xrt.sh"
xrt_dbg=`ls xrt-dbg*`
zocl_dbg=`ls zocl-dbg*`
echo dnf --disablerepo=\"*\" install -y *.rpm | sed -e "s/\<$xrt_dbg\>//g" | sed -e "s/\<$zocl_dbg\>//g" > $ORIGINAL_DIR/$PETALINUX_NAME/install_xrt.sh

echo "Creating $ORIGINAL_DIR/$PETALINUX_NAME/reinstall_xrt.sh"
echo dnf --disablerepo=\"*\" reinstall -y *.rpm | sed -e "s/\<$xrt_dbg\>//g" | sed -e "s/\<$zocl_dbg\>//g" > $ORIGINAL_DIR/$PETALINUX_NAME/reinstall_xrt.sh

cp $ORIGINAL_DIR/$PETALINUX_NAME/rpm.txt $ORIGINAL_DIR/$PETALINUX_NAME/rpms/.
cp $ORIGINAL_DIR/$PETALINUX_NAME/install_xrt.sh $ORIGINAL_DIR/$PETALINUX_NAME/rpms/.
cp $ORIGINAL_DIR/$PETALINUX_NAME/reinstall_xrt.sh $ORIGINAL_DIR/$PETALINUX_NAME/rpms/.
cd $ORIGINAL_DIR

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** COMPLETE [${BASH_SOURCE[0]}] **"
echo ""
