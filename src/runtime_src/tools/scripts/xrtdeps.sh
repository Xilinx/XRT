#!/bin/bash

FLAVOR=`grep '^ID=' /etc/os-release | awk -F= '{print $2}' | tr -d '"'`
VERSION=`grep '^VERSION_ID=' /etc/os-release | awk -F= '{print $2}' | tr -d '"'`
ARCH=`uname -m`
SUDO=${SUDO:-sudo}

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

#UB_LIST=()
#RH_LIST=()

rh_package_list()
{
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
     json-glib-devel \
     libdrm-devel \
     libjpeg-turbo-devel \
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
     redhat-lsb \
     rpm-build \
     strace \
     unzip \
     zlib-static \
     libcurl-devel \
     openssl-devel \
    )

    # Centos8
    if [ $VERSION == 8 ]; then

        RH_LIST+=(\
         systemd-devel \
         python3 \
         python3-pip \
         systemd-devel \
        )

    else

        RH_LIST+=(\
         libpng12-devel \
         libudev-devel \
         kernel-devel-$(uname -r) \
         kernel-headers-$(uname -r) \
         openssl-static \
         protobuf-static \
         python \
         python-pip \
        )

    fi

    #dmidecode is only applicable for x86_64
    if [ $ARCH == "x86_64" ]; then
	RH_LIST+=( dmidecode )
    fi
}

ub_package_list()
{
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
     libjson-glib-dev \
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
     python-pip \
     pciutils \
     pkg-config \
     protobuf-compiler \
     python3-sphinx \
     python3-sphinx-rtd-theme \
     sphinx-common \
     strace \
     unzip \
     uuid-dev \
     libcurl4-openssl-dev \
     libssl-dev \
     libudev-dev \
     libsystemd-dev \
    )

    if [[ $docker == 0 ]]; then
        UB_LIST+=(linux-headers-$(uname -r))
    fi

    #dmidecode is only applicable for x86_64
    if [ $ARCH == "x86_64" ]; then
	UB_LIST+=( dmidecode )
    fi

    # Use GCC8 on ARM64 Ubuntu as GCC7 randomly crashes with Internal Compiler Error on
    # Travis CI ARM64 platform
    if [ $ARCH == "aarch64" ]; then
        UB_LIST+=( gcc-8 )
        UB_LIST+=( g++-8 )
    fi

}

update_package_list()
{
    if [ $FLAVOR == "ubuntu" ] || [ $FLAVOR == "debian" ]; then
        ub_package_list
    elif [ $FLAVOR == "centos" ] || [ $FLAVOR == "rhel" ]; then
        rh_package_list
    else
        echo "unknown OS flavor $FLAVOR"
        exit 1
    fi
}

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

    if [ $FLAVOR == "centos" ] || [ $FLAVOR == "rhel" ] || [ $FLAVOR == "amzn" ]; then
        rpm -q "${RH_LIST[@]}"
        if [ $? == 0 ] ; then
            # Validate we have OpenCL 2.X headers installed
            rpm -q -i opencl-headers | grep '^Version' | grep ': 2\.'
        fi
    fi
}

prep_ubuntu()
{
    echo "Preparing ubuntu ..."
}

prep_centos7()
{
    if [ $docker == 0 ]; then 
        echo "Enabling CentOS SCL repository..."
        ${SUDO} yum --enablerepo=extras install -y centos-release-scl
    fi
}

prep_rhel7()
{
    echo "Enabling RHEL SCL repository..."
    ${SUDO} yum-config-manager --enable rhel-server-rhscl-7-rpms
}

prep_centos8()
{
    echo "Enabling PowerTools repo for CentOS8 ..."
    ${SUDO} yum install -y dnf-plugins-core
    ${SUDO} yum config-manager --set-enabled PowerTools
    ${SUDO} yum config-manager --set-enabled AppStream
}

prep_centos()
{
    echo "Enabling EPEL repository..."
    ${SUDO} yum install -y epel-release
    echo "Installing cmake3 from EPEL repository..."
    ${SUDO} yum install -y cmake3

    if [ $VERSION == 8 ]; then
        prep_centos8
    else
        prep_centos7
    fi
}

prep_rhel()
{
    echo "Enabling EPEL repository..."
    rpm -q --quiet epel-release
    if [ $? != 0 ]; then
	${SUDO} yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
	${SUDO} yum check-update
    fi

    if [ $VERSION == 8 ]; then
        echo "RHEL8 not implemented yet"
        exit 1;
    else
        prep_rhel7
    fi
}

install()
{
    if [ $FLAVOR == "ubuntu" ] || [ $FLAVOR == "debian" ]; then
        prep_ubuntu

        echo "Installing packages..."
        ${SUDO} apt install -y "${UB_LIST[@]}"
    fi

    # Enable EPEL on CentOS/RHEL
    if [ $FLAVOR == "centos" ]; then
        prep_centos
    elif [ $FLAVOR == "rhel" ]; then
        prep_rhel
    fi

    if [ $FLAVOR == "rhel" ] || [ $FLAVOR == "centos" ] || [ $FLAVOR == "amzn" ]; then
        echo "Installing RHEL/CentOS packages..."
        ${SUDO} yum install -y "${RH_LIST[@]}"
	if [ $ARCH == "ppc64le" ]; then
            ${SUDO} yum install -y devtoolset-7
	elif [ $VERSION -lt "8" ]; then
            ${SUDO} yum install -y devtoolset-6
	fi
    fi
}

update_package_list

if [ $validate == 1 ]; then
    validate
else
    install
fi
