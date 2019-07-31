#!/bin/bash

usage()
{
    echo "Usage: xrtdeps.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[-validate]                Validate that required packages are installed"
    echo "[-docker]                  Indicate that script is run within a docker container, disables select packages"

    exit 1
}

validate=0
docker=0

while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        -validate)
            validate=1
            shift
            ;;
        -docker)
            docker=1
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
     boost-program-options \
     boost-static \
     cmake \
     compat-libtiff3 \
     cppcheck \
     curl \
     dkms \
     gcc \
     gcc-c++ \
     gdb \
     git \
     glibc-static \
     gnuplot \
     gnutls-devel \
     gtest-devel \
     kernel-devel-$(uname -r) \
     kernel-headers-$(uname -r) \
     libdrm-devel \
     libjpeg-turbo-devel \
     libpng12-devel \
     libstdc++-static \
     libtiff-devel \
     libuuid-devel \
     libxml2-devel \
     libyaml-devel \
     lm_sensors \
     make \
     ncurses-devel \
     ocl-icd \
     ocl-icd-devel \
     opencl-headers \
     opencv \
     openssl-devel \
     pciutils \
     perl \
     pkgconfig \
     protobuf-devel \
     protobuf-compiler \
     protobuf-static \
     python \
     python-pip \
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
     g++ \
     gcc \
     gdb \
     git \
     gnuplot \
     libboost-dev \
     libboost-filesystem-dev \
     libboost-program-options-dev \
     libdrm-dev \
     libjpeg-dev \
     libgtest-dev \
     libncurses5-dev \
     libopencv-core-dev \
     libpng-dev \
     libprotoc-dev \
     libssl-dev \
     libtiff5-dev \
     libxml2-dev \
     libyaml-dev \
     linux-libc-dev \
     lm-sensors \
     lsb-release \
     make \
     ocl-icd-dev \
     ocl-icd-libopencl1 \
     opencl-headers \
     ocl-icd-opencl-dev \
     perl \
     python \
     pciutils \
     pkg-config \
     protobuf-compiler \
     python-pip \
     python3-sphinx \
     python3-sphinx-rtd-theme \
     sphinx-common \
     strace \
     unzip \
     uuid-dev \
)

if [[ $docker == 0 ]]; then
    #RH_LIST+=(kernel-headers-$(uname -r))
    UB_LIST+=(linux-headers-$(uname -r))
fi

FLAVOR=`grep '^ID=' /etc/os-release | awk -F= '{print $2}'`
FLAVOR=`echo $FLAVOR | tr -d '"'`
ARCH=`uname -m`
SUDO=${SUDO:-sudo}

#dmidecode is only applicable for x86_64
if [ $ARCH == "x86_64" ]; then
    if [ $FLAVOR == "ubuntu" ] || [ $FLAVOR == "debian" ]; then
	UB_LIST+=( dmidecode )
    fi
    if [ $FLAVOR == "centos" ] || [ $FLAVOR == "rhel" ] ; then
	RH_LIST+=( dmidecode )
    fi
fi

validate()
{
    if [ $FLAVOR == "ubuntu" ] || [ $FLAVOR == "debian" ]; then
        #apt -qq list "${UB_LIST[@]}"
        dpkg -l "${UB_LIST[@]}" > /dev/null
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
    if [ $FLAVOR == "ubuntu" ] || [ $FLAVOR == "debian" ]; then
        echo "Installing packages..."
        ${SUDO} apt install -y "${UB_LIST[@]}"
        ${SUDO} -H pip install --upgrade setuptools 
        ${SUDO} -H pip install pyopencl
    fi

    # Enable EPEL on CentOS/RHEL
    if [ $FLAVOR == "centos" ]; then
        echo "Enabling EPEL repository..."
        ${SUDO} yum install -y epel-release
        echo "Installing cmake3 from EPEL repository..."
        ${SUDO} yum install -y cmake3
    elif [ $FLAVOR == "rhel" ]; then
        echo "Enabling EPEL repository..."
        rpm -q --quiet epel-release
        if [ $? != 0 ]; then
	    ${SUDO} yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
	    ${SUDO} yum check-update
        fi
    fi

    # Enable GCC 6 compiler set on RHEL/CentOS 7.X
    if [ $FLAVOR == "rhel" ]; then
        echo "Enabling RHEL SCL repository..."
        ${SUDO} yum-config-manager --enable rhel-server-rhscl-7-rpms
    elif [ $FLAVOR == "centos" ]; then
        echo "Enabling CentOS SCL repository..."
        ${SUDO} yum --enablerepo=extras install -y centos-release-scl
    fi

    if [ $FLAVOR == "rhel" ] || [ $FLAVOR == "centos" ]; then
        echo "Installing RHEL/CentOS packages..."
        ${SUDO} yum install -y "${RH_LIST[@]}"
        ${SUDO} yum install -y devtoolset-6
        ${SUDO} -H pip install pyopencl
        ${SUDO} pip install --ignore-installed numpy==1.8
    fi
}

if [ $validate == 1 ]; then
    validate
else
    install
fi
