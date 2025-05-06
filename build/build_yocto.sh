#!/bin/bash
bold=$(tput bold)
normal=$(tput sgr0)
red=$(tput setaf 1)

ABS_PATH=$(pwd)
yocto_path="$ABS_PATH/yocto/edf"
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
    echo "          -clean, clean                   Remove build directories"
    echo ""
}

usage_and_exit()
{
    usage
    exit $1
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
        echo "EXTRA_OECMAKE += \"-DMY_VITIS=$XILINX_VITIS\"" >> $XRT_BB
        echo 'EXTERNALSRC_BUILD = "${WORKDIR}/build"' >> $XRT_BB
	echo 'DEPENDS += " systemtap"' >> $XRT_BB
        echo 'PACKAGE_CLASSES = "package_rpm"' >> $XRT_BB
        echo 'LICENSE = "GPLv2 & Apache-2.0"' >> $XRT_BB
        echo 'LIC_FILES_CHKSUM = "file://../LICENSE;md5=de2c993ac479f02575bcbfb14ef9b485 \' >> $XRT_BB
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
        if [[ ! -z $XRT_VERSION_PATCH ]]; then
            echo "EXTRA_OEMAKE += \"XRT_VERSION_PATCH=$XRT_VERSION_PATCH\"" >> $ZOCL_BB
        fi
    fi
    eval "$SAVED_OPTIONS_LOCAL"
}

clean=0
while [ $# -gt 0 ]; do
	case $1 in
		-help )
			usage_and_exit 0
			;;
		-clean | clean )
			clean=1
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
    echo "/bin/rm -rf $ABS_PATH/yocto"
    /bin/rm -rf $ABS_PATH/yocto
    /bin/rm -rf $ABS_PATH/.repo
    exit 0
fi

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

if [ -d "yocto/edf" ]; then
    cd yocto/edf
    source basecamp-init-build-env
else
    git submodule update --init --recursive --force
    mkdir -p yocto/edf
    cd yocto/edf

    echo "repo init -u $REPO_URL -b $BRANCH -m $MANIFEST_PATH/$MANIFEST_FILE"
    repo init -u $REPO_URL -b $BRANCH -m $MANIFEST_PATH/$MANIFEST_FILE

    repo sync

    export TEMPLATECONF=$yocto_path/sources/meta-basecamp/conf/templates/default

    source basecamp-init-build-env

    CONF_FILE=$yocto_path/build/conf/local.conf

    sed -i '/^# Source and Sstate mirror settings/a\
# Use optional internal AMD Xilinx gitenterprise support\
include conf/distro/include/xilinx-mirrors.conf' "$CONF_FILE"

    sed -i "s|^SOURCE_MIRROR_URL = .*|SOURCE_MIRROR_URL = \"file://$MANIFEST_PATH/downloads\"|" "$CONF_FILE"
    sed -i "/^SSTATE_MIRRORS = \" \\\.*$/,/^$/c\
SSTATE_MIRRORS = \"file://.* file://$MANIFEST_PATH/sstate-cache/PATH\"" "$CONF_FILE"

    bitbake-layers add-layer $yocto_path/sources/meta-xilinx-internal
    bitbake-layers add-layer $yocto_path/sources/meta-xilinx-internal/meta-xilinx-restricted-vek280-poc/
    bitbake-layers add-layer $yocto_path/sources/meta-xilinx-restricted/meta-xilinx-restricted-ea/meta-xilinx-restricted-vek385/

    install_recipes
fi

MACHINE=versal2-common bitbake xrt
