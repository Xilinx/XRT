#!/bin/bash

set -e

usage()
{
    echo "Usage: xrtdeps.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[-validate]                Validate that required packages are installed"

    exit 1
}

validate=0

while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        -validate)
            validate=1
            shift
            ;;
        *)
            echo "unknown option"
            usage
            ;;
    esac
done

# Script to install XRT dependencies
# Note all packages listed here are required for XRT. Some of them like jpeg, png, tiff, etc are used by applications
RH_LIST=(\
     boost-devel \
     boost-filesystem \
     boost-static \
     cmake \
     compat-libtiff3 \
     cppcheck \
     curl \
     dkms \
     dmidecode \
     gcc \
     gcc-c++ \
     gdb \
     git \
     gnuplot \
     gnutls-devel \
     kernel-devel \
     kernel-headers \
#     kernel-headers-$(uname -r) \
     libdrm-devel \
     libjpeg-turbo-devel \
     libpng12-devel \
     libstdc++-static \
     libtiff-devel \
     libuuid-devel \
     lm_sensors \
     make \
     ncurses-devel \
     ocl-icd \
     ocl-icd-devel \
     opencl-headers \
     opencv \
     pciutils \
     perl \
     pkgconfig \
     protobuf-devel \
     protobuf-compiler \
     protobuf-static \
     python \
     redhat-lsb \
     rpm-build \
     strace \
     unzip \
     )

UB_LIST=(\
     cmake \
     cppcheck \
     curl \
     dkms \
     dmidecode \
     g++ \
     gcc \
     gdb \
     git \
     gnuplot \
     libboost-dev \
     libboost-filesystem-dev \
     libdrm-dev \
     libjpeg-dev \
     libncurses5-dev \
     libopencv-core-dev \
     libpng-dev \
     libprotoc-dev \
     libtiff5-dev \
     linux-headers-$(uname -r) \
     linux-libc-dev \
     lm-sensors \
     lsb \
     make \
     ocl-icd-libopencl1 \
     opencl-headers \
     ocl-icd-opencl-dev \
     perl \
     python \
     pciutils \
     pkg-config \
     protobuf-compiler \
     python3-sphinx \
     python3-sphinx-rtd-theme \
     sphinx-common \
     strace \
     unzip \
     uuid-dev \
)

FLAVOR=`grep '^ID=' /etc/os-release | awk -F= '{print $2}'`
FLAVOR=`echo $FLAVOR | tr -d '"'`

validate()
{
    if [ $FLAVOR == "ubuntu" ]; then
        apt -qq list "${UB_LIST[@]}"
        if [ $? == 0 ] ; then
	    # Validate we have OpenCL 2.X headers installed
            dpkg-query -s opencl-headers | grep '^Version: 2\.'
        fi
    fi

    if [ $FLAVOR == "centos" ] || [ $FLAVOR == "rhel" ] ; then
        rpm -q "${RH_LIST[@]}"
        if [ $? == 0 ] ; then
            # Validate we have OpenCL 2.X headers installed
            rpm -q -i opencl-headers | grep '^Version' | grep ': 2\.'
        fi
    fi
}

install()
{
    if [ $FLAVOR == "ubuntu" ]; then
        echo "Installing Ubuntu packages..."
        sudo apt install -y "${UB_LIST[@]}"
    fi

    # Enable EPEL on CentOS/RHEL
    if [ $FLAVOR == "centos" ]; then
        echo "Enabling EPEL repository..."
        sudo yum install epel-release
    elif [ $FLAVOR == "rhel" ]; then
        echo "Enabling EPEL repository..."
        rpm -q --quiet epel-release
        if [ $? != 0 ]; then
	    sudo yum install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
	    sudo yum check-update
        fi
    fi

    # Enable GCC 6 compiler set on RHEL/CentOS 7.X
    if [ $FLAVOR == "rhel" ]; then
        echo "Enabling RHEL SCL repository..."
        sudo yum-config-manager --enable rhel-server-rhscl-7-rpms
    elif [ $FLAVOR == "centos" ]; then
        echo "Enabling CentOS SCL repository..."
        sudo yum install centos-release-scl
    fi

    if [ $FLAVOR == "rhel" ] || [ $FLAVOR == "centos" ]; then
        echo "Installing RHEL/CentOS packages..."
        sudo yum install -y "${RH_LIST[@]}"
        sudo yum install devtoolset-6
    fi
}

if [ $validate == 1 ]; then
    validate
else
    install
fi
