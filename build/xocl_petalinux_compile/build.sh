#!/bin/bash

set -e

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
    echo "          -cache                          path to sstate-cache[optional]"
    echo "          -setup                          setup file to use[optional]"
    echo "          -clean, clean                   Remove build directories"
    echo ""
}

usage_and_exit()
{
    usage
    exit $1
}

PROGRAM=`basename $0`
OSDIST=`grep '^ID=' /etc/os-release | awk -F= '{print $2}' | tr -d '"'`
BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
CMAKE=cmake
CORE=`grep -c ^processor /proc/cpuinfo`
jcore=$CORE
CONFIG_FILE=""
PETA_BSP=""
PROJ_NAME=""
PLATFROM=""
clean=0
SSTATE_CACHE=""
SETTINGS_FILE="petalinux.build"


if [[ $OSDIST == "centos" ]] || [[ $OSDIST == "amzn" ]] || [[ $OSDIST == "rhel" ]] || [[ $OSDIST == "fedora" ]]; then
    CMAKE=cmake3
    if [[ ! -x "$(command -v $CMAKE)" ]]; then
        echo "$CMAKE is not installed, please install cmake3 by running following commands..."
        echo "sudo yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm"
        echo "sudo yum install -y cmake3"
        exit 1
    fi
fi

release_dir="cmake_files"


# --- Internal funtions ---
install_recipes()
{
    META_USER_PATH=$1

    SAVED_OPTIONS_LOCAL=$(set +o)
    set +e
    mkdir -p ${META_USER_PATH}/recipes-xrt/xocl
    XOCL_BB=${META_USER_PATH}/recipes-xrt/xocl/xocl_git.bb
    cp $BUILDDIR/xocl_git.bb $XOCL_BB
    grep "inherit externalsrc" $XOCL_BB
    if [ $? != 0 ]; then
cat << EOF >> $XOCL_BB
inherit externalsrc
EXTERNALSRC = "/scratch/XRT/build/xocl_petalinux_compile/driver_code/driver/xocl"
EXTERNALSRC_BUILD = "/scratch/XRT/build/xocl_petalinux_compile/driver_code/driver/xocl"
PACKAGE_CLASSES = "package_rpm"
PV = "202110.2.11.0"
LICENSE = "GPLv2 & Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"
do_install() {
    install -d \${D}\${base_libdir}/modules/\${KERNEL_VERSION}/extra
    install -d \${D}\${sysconfdir}/udev/rules.d
    install -m 555 -g root -o root \${B}/userpf/xocl.ko \${D}\${base_libdir}/modules/\${KERNEL_VERSION}/extra/xocl.ko
    install -m 555 -g root -o root \${B}/mgmtpf/xclmgmt.ko \${D}\${base_libdir}/modules/\${KERNEL_VERSION}/extra/xclmgmt.ko
    install -m 644 -g root -o root \${B}/userpf/99-xocl.rules \${D}\${sysconfdir}/udev/rules.d/99-xocl.rules
    install -m 644 -g root -o root \${B}/mgmtpf/99-xclmgmt.rules \${D}\${sysconfdir}/udev/rules.d/99-xclmgmt.rules
}
FILES_\${PN} += "\${base_libdir}/modules/\${KERNEL_VERSION}/extra/xocl.ko"
FILES_\${PN} += "\${base_libdir}/modules/\${KERNEL_VERSION}/extra/xclmgmt.ko"
FILES_\${PN} += "\${sysconfdir}/udev/rules.d/99-xocl.rules"
FILES_\${PN} += "\${sysconfdir}/udev/rules.d/99-xclmgmt.rules"
pkg_postinst_\${PN}() {
  #!/bin/sh -e
  echo "Unloading old XRT Linux kernel modules"
  ( rmmod xocl || true ) > /dev/null 2>&1
  ( rmmod xclmgmt || true ) > /dev/null 2>&1
  echo "Loading new XRT Linux kernel modules"
  udevadm control --reload-rules
  modprobe xocl
  modprobe xclmgmt
  udevadm trigger
}
EOF
    fi
    eval "$SAVED_OPTIONS_LOCAL"
}


while [ $# -gt 0 ]; do
  case $1 in
    -help | --help )
      usage_and_exit 0
      ;;
    -aarch | --aarch )
      shift
      AARCH=$1
      ;;
    -setup | --setup )
      shift
      SETTINGS_FILE=$1
      ;;
    -clean | --clean | clean )
      clean=1
      ;;
    -cache | --cache )
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
    echo "/bin/rm -rf $aarch64_dir $aarch32_dir $versal_dir $release_dir driver_code output"
    /bin/rm -rf $BUILDDIR/$aarch64_dir $BUILDDIR/$aarch32_dir $BUILDDIR/$versal_dir $BUILDDIR/$release_dir $BUILDDIR/driver_code $BUILDDIR/output
    exit 0
fi

cd $BUILDDIR
mkdir -p $release_dir
cd $release_dir
echo "$CMAKE -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../"

time $CMAKE -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../
time make -j $jcore install

cd $BUILDDIR

# we pick Petalinux BSP
if [ -f $SETTINGS_FILE ]; then
    source $SETTINGS_FILE
fi
source $PETALINUX/settings.sh

if [[ $AARCH = $aarch64_dir ]]; then
    PETA_BSP="$PETALINUX/../../bsp/internal/zynqmp/zynqmp-common-v$PETALINUX_VER-final.bsp"
    YOCTO_MACHINE="zynqmp-generic"
elif [[ $AARCH = $aarch32_dir ]]; then
    PETA_BSP="$PETALINUX/../../bsp/internal/zynq/zynq-rootfs-common-v$PETALINUX_VER-final.bsp"
    YOCTO_MACHINE="zynq-generic"
elif [[ $AARCH = $versal_dir ]]; then
    PETA_BSP="$PETALINUX/../../bsp/internal/versal/versal-rootfs-common-v$PETALINUX_VER-final.bsp"
    YOCTO_MACHINE="versal-generic"
else
    error "aarch option is not valid, please provide correct aarch value aarch32/aarch64/versal"
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
PETA_BIN="$PETALINUX/tools/common/petalinux/bin"

PETALINUX_NAME=$AARCH
echo " * Create PetaLinux from BSP (-s $PETA_BSP)"
PETA_CREATE_OPT="-s $PETA_BSP"

if [ ! -d $PETALINUX_NAME ]; then
    echo " * Create PetaLinux Project: $PETALINUX_NAME"
    echo "[CMD]: petalinux-create -t project -n $PETALINUX_NAME $PETA_CREATE_OPT"
    $PETA_BIN/petalinux-create -t project -n $PETALINUX_NAME $PETA_CREATE_OPT
else
    echo " * PetaLinux Project already present: $PETALINUX_NAME."
fi

cd ${PETALINUX_NAME}/project-spec/meta-user/
install_recipes .

cd $BUILDDIR/$PETALINUX_NAME

echo "CONFIG_YOCTO_BUILDTOOLS_EXTENDED=y" >> project-spec/configs/config
echo "CONFIG_YOCTO_MACHINE_NAME=\"${YOCTO_MACHINE}\""
echo "CONFIG_YOCTO_MACHINE_NAME=\"${YOCTO_MACHINE}\"" >> project-spec/configs/config


if [ ! -z $SSTATE_CACHE ] && [ -d $SSTATE_CACHE ]; then
    echo "SSTATE-CACHE:${SSTATE_CACHE} added"
    echo "CONFIG_YOCTO_LOCAL_SSTATE_FEEDS_URL=\"${SSTATE_CACHE}\"" >> project-spec/configs/config
else
    echo "SSTATE-CACHE:${SSTATE_CACHE} not present"
fi

echo "[CMD]: Building xocl and xclmgmt using Petalinux"
$PETA_BIN/petalinux-build -c xocl
if [ $? != 0 ]; then
   error "Driver build failed"
fi

echo "Copying rpms..."
mkdir -p $BUILDDIR/output
if [ ! -d build/tmp/deploy/rpm ]; then
  tmp_path=$(cat project-spec/configs/config | grep CONFIG_TMP_DIR_LOCATION \
  | awk -F'=' '{print $2}' |  sed -e 's/^"//' -e 's/"$//')
  cp -v ${tmp_path}/deploy/rpm/*/xocl*.rpm $BUILDDIR/output
else
  cp -v build/tmp/deploy/rpm/*/xocl*.rpm $BUILDDIR/output
fi


echo "** COMPLETE **"
