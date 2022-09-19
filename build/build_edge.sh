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
    echo "          -archiver                       Generate archiver of the project. This is needed to generate LICENSE"
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
        echo 'EXTRA_OECMAKE:append:versal += "-DXRT_LIBDFX=true"' >> $XRT_BB
        echo 'EXTRA_OECMAKE:append:zynqmp += "-DXRT_LIBDFX=true"' >> $XRT_BB
        echo 'DEPENDS += "rapidjson"' >> $XRT_BB
        echo 'DEPENDS:append:versal += "libdfx"' >> $XRT_BB
        echo 'DEPENDS:append:zynqmp += "libdfx"' >> $XRT_BB
        echo "FILES:\${PN} += \"\${libdir}/ps_kernels_lib \ " >> $XRT_BB
        echo "                 \${datadir}\"" >> $XRT_BB
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

enable_vdu()
{
    echo "IMAGE_INSTALL:append = \" libvdu-ctrlsw kernel-module-vdu vdu-firmware\""  >> build/conf/local.conf
}

config_versal_project()
{
    VERSAL_PROJECT_DIR=$1
    APU_RECIPES_DIR=$XRT_REPO_DIR/src/runtime_src/tools/scripts/apu_recipes

    sed -i 's/^CONFIG_packagegroup-petalinux-opencv.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_packagegroup-petalinux-jupyter.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_xrt-dev.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_tcl.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_opencl-clhpp-dev.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_opencl-headers.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_libstdcPLUSPLUS.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_packagegroup-petalinux-x11.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_iperf3.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_python3.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_package-feed-uris.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_dnf.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_kernel-devsrc.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_e2fsprogs-mke2fs.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_resize-part.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_imagefeature-hwcodecs.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_htop.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_meson.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_imagefeature-ssh-server-dropbear.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_imagefeature-package-management.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_imagefeature-debug-tweaks.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_gdb.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_valgrind.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/^CONFIG_packagegroup-core-ssh-dropbear.*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config



    # Add necessary rootfs configs
    sed -i 's/.*CONFIG_openssh-sftp-server is.*/CONFIG_openssh-sftp-server=y/g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/.*CONFIG_strace is.*/CONFIG_strace=y/g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/.*CONFIG_perf is.*/CONFIG_perf=y/g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/.*CONFIG_vim is.*/CONFIG_vim=y/g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/.*CONFIG_lrzsz is.*/CONFIG_lrzsz=y/g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/.*CONFIG_ldd is.*/CONFIG_ldd=y/g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/.*CONFIG_binutils is.*/CONFIG_binutils=y/g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/.*CONFIG_ai-engine-driver is.*/CONFIG_ai-engine-driver=y/g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/.*CONFIG_ADD_EXTRA_USERS is.*/CONFIG_ADD_EXTRA_USERS="petalinux:petalinux;"/g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    sed -i 's/.*CONFIG_ROOTFS_ROOT_PASSWD=\"root\".*//g' $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config
    echo "CONFIG_ROOTFS_ROOT_PASSWD=\"root\"" >> $VERSAL_PROJECT_DIR/project-spec/configs/rootfs_config

    # Configure u-boot to pick dtb from address 0x40000
    UBOOT_USER_SCRIPT=$APU_RECIPES_DIR/u-boot_custom.cfg
    cp $UBOOT_USER_SCRIPT $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-bsp/u-boot/files
    echo "SRC_URI += \"file://u-boot_custom.cfg\"" >> $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-bsp/u-boot/u-boot-xlnx_%.bbappend

    # Configure kernel
    echo "CONFIG_SUSPEND=n" >> $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-kernel/linux/linux-xlnx/bsp.cfg
    echo "CONFIG_PM=n" >> $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-kernel/linux/linux-xlnx/bsp.cfg
    echo "CONFIG_SPI=n" >> $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-kernel/linux/linux-xlnx/bsp.cfg
    echo "CONFIG_DRM_XLNX_DSI=n" >> $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-kernel/linux/linux-xlnx/bsp.cfg
    echo "CONFIG_CMA_DEBUGFS=y" >> $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-kernel/linux/linux-xlnx/bsp.cfg

    # Configure inittab for getty
    INIT_TAB_FILE=$APU_RECIPES_DIR/sysvinit-inittab_%.bbappend
    if [ ! -d $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-core/sysvinit ]; then
        mkdir -p $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-core/sysvinit
    fi
    cp $INIT_TAB_FILE $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-core/sysvinit

    # Create startup script to write to sysfs entry to indicate apu booted
    SERVICE_FILE=$APU_RECIPES_DIR/apu-boot.service
    BB_FILE=$APU_RECIPES_DIR/apu-boot.bb
    INIT_SCRIPT=$APU_RECIPES_DIR/apu-boot

    if [ ! -d $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/apu-boot ]; then
        $PETA_BIN/petalinux-config --silentconfig
        $PETA_BIN/petalinux-create -t apps --template install -n apu-boot --enable
    fi

    cp $SERVICE_FILE $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/apu-boot/files
    cp $BB_FILE $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/apu-boot
    cp $INIT_SCRIPT $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/apu-boot/files

    # Create startup script to start skd
    SERVICE_FILE=$APU_RECIPES_DIR/skd.service
    BB_FILE=$APU_RECIPES_DIR/skd.bb
    INIT_SCRIPT=$APU_RECIPES_DIR/skd.sh

    if [ ! -d $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/skd ]; then
        $PETA_BIN/petalinux-config --silentconfig
        $PETA_BIN/petalinux-create -t apps --template install -n skd --enable
    fi

    cp $SERVICE_FILE $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/skd/files
    cp $BB_FILE $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/skd
    cp $INIT_SCRIPT $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/skd/files
    
    # Create daemon to modprobe/rmmod vdu modules after xclbin load
    # This daemon probes vdu drivers on first xclbin load and exit
    SERVICE_FILE=$APU_RECIPES_DIR/vdu-init.service
    BB_FILE=$APU_RECIPES_DIR/vdu-init.bb
    INIT_SCRIPT=$APU_RECIPES_DIR/vdu-init

    if [ ! -d $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/vdu-init ]; then
        $PETA_BIN/petalinux-config --silentconfig
        $PETA_BIN/petalinux-create -t apps --template install -n vdu-init --enable
    fi

    cp $SERVICE_FILE $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/vdu-init/files
    cp $BB_FILE $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/vdu-init
    cp $INIT_SCRIPT $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/vdu-init/files
    
    # Generate vdu modules and add them to apu package
    
    # Enable VDU kernel module 
    VDU_MOD_BB_FILE=$APU_RECIPES_DIR/kernel-module-vdu.bb
    if [ ! -d $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/kernel-module-vdu ]; then
        $PETA_BIN/petalinux-config --silentconfig
        $PETA_BIN/petalinux-create -t apps --template install -n kernel-module-vdu --enable
    fi
    cp -rf $VDU_MOD_BB_FILE $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/kernel-module-vdu/
    
    # Enable VDU firmware 
    VDU_FIRMWARE_BB_FILE=$APU_RECIPES_DIR/vdu-firmware.bb
    if [ ! -d $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/vdu-firmware ]; then
        $PETA_BIN/petalinux-config --silentconfig
        $PETA_BIN/petalinux-create -t apps --template install -n vdu-firmware --enable
    fi
    cp -rf $VDU_FIRMWARE_BB_FILE $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/vdu-firmware/
    
    # Enable VDU control software library 
    # This is not required as PS Kernels statically linking with control software, Enabling this to debug standalone control software 
    VDU_LIBRARY_BB_FILE=$APU_RECIPES_DIR/libvdu-ctrlsw.bb
    if [ ! -d $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/libvdu-ctrlsw ]; then
        $PETA_BIN/petalinux-config --silentconfig
        $PETA_BIN/petalinux-create -t apps --template install -n libvdu-ctrlsw --enable
    fi
    cp -rf $VDU_LIBRARY_BB_FILE $VERSAL_PROJECT_DIR/project-spec/meta-user/recipes-apps/libvdu-ctrlsw/

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
archiver=0
gen_sysroot=1
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
		-archiver | archiver )
			archiver=1
			;;
		-sysroot | sysroot )
			gen_sysroot=1
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

#Uncomment the following 2 lines to change TMP_DIR location
#echo "CONFIG_TMP_DIR_LOCATION=\"/scratch/${USER}/petalinux-top/$PETALINUX_VER\""
#echo "CONFIG_TMP_DIR_LOCATION=\"/scratch/${USER}/petalinux-top/$PETALINUX_VER\"" >> project-spec/configs/config 

# Build package
echo " * Performing PetaLinux Build (from: ${PWD})"
#Run a full build if -full option is provided
if [[ $full == 1 ]]; then
  if [[ $AARCH = $versal_dir ]]; then
    # configure the project with appropriate options
    config_versal_project .
    enable_vdu
  fi

  echo "[CMD]: petalinux-config -c kernel --silentconfig"
  $PETA_BIN/petalinux-config -c kernel --silentconfig
  echo "[CMD]: petalinux-config -c rootfs --silentconfig"
  $PETA_BIN/petalinux-config -c rootfs --silentconfig
  echo "[CMD]: petalinux-build"
  $PETA_BIN/petalinux-build
  if [[ $gen_sysroot == 1 ]]; then
        $PETA_BIN/petalinux-build --sdk
        echo "Run $ORIGINAL_DIR/$PETALINUX_NAME/images/linux/sdk.sh to generate the syroot"
  fi
else
#Run just xrt build if -full option is not provided
  echo "[CMD]: petalinux-build -c zocl"
  $PETA_BIN/petalinux-build -c zocl
  echo "[CMD]: petalinux-build -c xrt"
  $PETA_BIN/petalinux-build -c xrt
fi

if [[ $archiver == 1 ]]; then
  $PETA_BIN/petalinux-build --archiver
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

if [[ $full == 1 ]]; then
  mkdir -p $ORIGINAL_DIR/$PETALINUX_NAME/apu_packages
  export PATH=$PETALINUX/../../tool/petalinux-v$PETALINUX_VER-final/components/yocto/buildtools/sysroots/x86_64-petalinux-linux/usr/bin:$PATH
  $XRT_REPO_DIR/src/runtime_src/tools/scripts/pkgapu.sh -output $ORIGINAL_DIR/$PETALINUX_NAME/apu_packages -images $ORIGINAL_DIR/$PETALINUX_NAME/images/linux/ -idcode "0x14ca8093" -package-name xrt-apu-vck5000
  $XRT_REPO_DIR/src/runtime_src/tools/scripts/pkgapu.sh -output $ORIGINAL_DIR/$PETALINUX_NAME/apu_packages -images $ORIGINAL_DIR/$PETALINUX_NAME/images/linux/ -idcode "0x04cd0093" -package-name xrt-apu
  
fi

cd $ORIGINAL_DIR

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** COMPLETE [${BASH_SOURCE[0]}] **"
echo ""
