#!/bin/bash

bold=$(tput bold)
normal=$(tput sgr0)
red=$(tput setaf 1)

ABS_PATH=$(pwd)
XRT_REPO_DIR=`readlink -f ${ABS_PATH}/..`
SETTINGS_FILE="$ABS_PATH/yocto.build"

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
    echo "          -clean, clean                   Remove build directories, Specify architecture"
    echo "          -aarch [vek385|vrk160]          Specify architecture (required)"
    echo ""
}

usage_and_exit()
{
    usage
    exit $1
}

set_architecture_params()
{
    case $ARCH in
        vek385)
            yocto_path="$ABS_PATH/yocto/edf/vek385"
            MACHINE="amd-cortexa78-mali-common"
            RPM_ARCH_DIR="amd_cortexa78_mali_common"
            ;;
        vrk160)
            yocto_path="$ABS_PATH/yocto/edf/vrk160"
            MACHINE="amd-cortexa72-common"
            RPM_ARCH_DIR="amd_cortexa72_common"
            ;;
        *)
            error "Invalid architecture: $ARCH. Supported architectures: vek385, vrk160"
            ;;
    esac
}

install_repo()
{
    echo "Installing repo...."
    curl https://storage.googleapis.com/git-repo-downloads/repo > repo
    chmod a+x repo
    mkdir -p "$HOME/bin"
    mv repo "$HOME/bin/"
    export PATH="$HOME/bin:$PATH"
}

install_recipes()
{
    META_USER_PATH=$yocto_path/sources/meta-xilinx/meta-xilinx-core
    SAVED_OPTIONS_LOCAL=$(set +o)
    set +e
    mkdir -p ${META_USER_PATH}/recipes-xrt/xrt
    mkdir -p ${META_USER_PATH}/recipes-xrt/zocl
    XRT_BB=${META_USER_PATH}/recipes-xrt/xrt/xrt_%.bbappend
    ZOCL_BB=${META_USER_PATH}/recipes-xrt/zocl/zocl_%.bbappend
    grep "inherit externalsrc" $XRT_BB
    if [ $? != 0 ]; then
        echo "inherit externalsrc" > $XRT_BB
        echo "EXTERNALSRC = \"$XRT_REPO_DIR/src\"" >> $XRT_BB
        echo "EXTRA_OECMAKE += \"-DMY_VITIS=$XILINX_VITIS -DXRT_EDGE=1 -DXRT_YOCTO=1 -DCMAKE_INSTALL_PREFIX=/usr\"" >> $XRT_BB
        echo 'EXTERNALSRC_BUILD = "${WORKDIR}/build"' >> $XRT_BB
	echo 'DEPENDS += " systemtap"' >> $XRT_BB
        echo 'PACKAGE_CLASSES = "package_rpm"' >> $XRT_BB
        echo 'LICENSE = "GPLv2 & Apache-2.0"' >> $XRT_BB
        echo 'LIC_FILES_CHKSUM = "file://../LICENSE;md5=de2c993ac479f02575bcbfb14ef9b485 \' >> $XRT_BB
        echo '                    file://runtime_src/core/edge/drm/zocl/LICENSE;md5=7d040f51aae6ac6208de74e88a3795f8 "' >> $XRT_BB
        echo '' >> $XRT_BB
        echo 'do_install:append() {' >> $XRT_BB
        echo '    if [ -d "${D}${libdir}/dtrace" ]; then' >> $XRT_BB
        echo '        rm -rf ${D}${libdir}/dtrace' >> $XRT_BB
        echo '    fi' >> $XRT_BB
        echo '}' >> $XRT_BB
    fi

    grep "inherit externalsrc" $ZOCL_BB
    if [ $? != 0 ]; then
        echo "inherit externalsrc" > $ZOCL_BB
        echo "EXTERNALSRC = \"$XRT_REPO_DIR/src/runtime_src/core/edge/drm/zocl\"" >> $ZOCL_BB
        echo "EXTERNALSRC_BUILD = \"$XRT_REPO_DIR/src/runtime_src/core/edge/drm/zocl\"" >> $ZOCL_BB
        echo 'PACKAGE_CLASSES = "package_rpm"' >> $ZOCL_BB
        echo 'LICENSE = "GPLv2 & Apache-2.0"' >> $ZOCL_BB
        echo 'LIC_FILES_CHKSUM = "file://LICENSE;md5=7d040f51aae6ac6208de74e88a3795f8"' >> $ZOCL_BB
        if [[ ! -z $XRT_VERSION_PATCH ]]; then
            echo "EXTRA_OEMAKE += \"XRT_VERSION_PATCH=$XRT_VERSION_PATCH\"" >> $ZOCL_BB
        fi
    fi
    eval "$SAVED_OPTIONS_LOCAL"
}

clean=0
ARCH=""
while [ $# -gt 0 ]; do
    case $1 in
        -help )
            usage_and_exit 0
            ;;
        -clean | clean )
            clean=1
            ;;
        -aarch )
            shift
            ARCH="$1"
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

if [[ $clean == 1 ]]; then
    set_architecture_params
    echo "Cleaning architecture: $ARCH"
    echo "/bin/rm -rf $yocto_path"
    /bin/rm -rf "$yocto_path"
    /bin/rm -rf $ABS_PATH/.repo
    exit 0       
fi

# Check if architecture is specified for build
if [[ -z "$ARCH" ]]; then
    error "Architecture not specified. Use -aarch [vek385|vrk160]"
fi

# Set architecture-specific parameters
set_architecture_params

if [ -f $SETTINGS_FILE  ]; then
    echo "source YOCTO Manifest from local file"
    source $SETTINGS_FILE
fi

if [[ -z ${XILINX_VITIS:+x} ]] || [[ ! -d ${XILINX_VITIS} ]]; then
   echo "XILINX_VITIS is not available. Please source Vitis"
   exit 1
fi

#Check if repo is installed and get its version
if ! command -v repo &> /dev/null; then
    echo "Repo command not found. Installing repo..."
    install_repo
elif [[ $(repo --version 2>&1 | grep -oP 'repo launcher version \K[0-9.]+') < 2.5 ]]; then
    echo "Repo version is less than 2.5. Reinstalling repo..."
    install_repo
fi

if [ -f "$yocto_path/internal-edf-init-build-env" ]; then
    cd $yocto_path
    source internal-edf-init-build-env
else
    git submodule update --init --recursive --force
    mkdir -p $yocto_path
    cd $yocto_path

    echo "repo init -u $REPO_URL -b $BRANCH -m $MANIFEST_PATH/$MANIFEST_FILE"
    yes ""| repo init -u $REPO_URL -b $BRANCH -m $MANIFEST_PATH/$MANIFEST_FILE

    repo sync
    source internal-edf-init-build-env
    install_recipes
fi

if MACHINE=$MACHINE bitbake xrt; then
    echo "bitbake xrt succeeded."

    rm -rf "$yocto_path/rpms"
    mkdir -p "$yocto_path/rpms"

    cp -rf "$yocto_path/build/tmp/deploy/rpm/cortexa72_cortexa53/xrt-"* "$yocto_path/rpms/"
    cp -rf "$yocto_path/build/tmp/deploy/rpm/$RPM_ARCH_DIR/zocl-"* "$yocto_path/rpms/"
    cp -rf "$yocto_path/build/tmp/deploy/rpm/$RPM_ARCH_DIR/kernel-"* "$yocto_path/rpms/"

    cd $yocto_path/rpms
    echo "Creating $yocto_path/rpms/install_xrt.sh"
    xrt_dbg=`ls xrt-dbg* zocl-dbg*`

    rpm_list=$(ls *.rpm)
    for dbg in $xrt_dbg; do
            rpm_list=$(echo "$rpm_list" | sed -e "s|$dbg||g")
    done
    # Remove any empty entries and extra spaces
    final_rpms=$(echo $rpm_list | xargs)

    echo dnf --disablerepo=\"*\" install -y $final_rpms > $yocto_path/rpms/install_xrt.sh
    echo dnf --disablerepo=\"*\" reinstall -y $final_rpms > $yocto_path/rpms/reinstall_xrt.sh

    echo "RPMs copied to $yocto_path/rpms"
    cd -
else
    echo "bitbake xrt failed"
fi
