#!/bin/bash

# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#

FLAVOR=`grep '^ID=' /etc/os-release | awk -F= '{print $2}' | tr -d '"'`
VERSION=`grep '^VERSION_ID=' /etc/os-release | awk -F= '{print $2}' | tr -d '"'`
MAJOR=${VERSION%.*}
ARCH=`uname -m`

usage()
{
    echo "Usage: xrtdeps.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[-validate]                Validate that required packages are installed"
    echo "[-docker]                  Indicate that script is run within a docker container, disables select packages"
    echo "[-sysroot]                 Indicate that script is run to prepare sysroot, disables select packages"

    exit 1
}

validate=0
docker=0
sysroot=0
ds9=0

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
        -sysroot)
            sysroot=1
            shift
            ;;
        -ds9)
            ds9=1
            shift
            ;;
        *)
            echo "unknown option"
            usage
            ;;
    esac
done

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
     elfutils-devel \
     gcc \
     gcc-c++ \
     gdb \
     git \
     glibc-static \
     gnuplot \
     gnutls-devel \
     gtest-devel \
     json-glib-devel \
     libcurl-devel \
     libdrm-devel \
     libffi-devel \
     libjpeg-turbo-devel \
     libstdc++-static \
     libtiff-devel \
     libuuid-devel \
     libyaml-devel \
     lm_sensors \
     make \
     ncurses-devel \
     ocl-icd \
     ocl-icd-devel \
     opencl-headers \
     openssl-devel \
     pciutils \
     perl \
     pkgconfig \
     protobuf-compiler \
     protobuf-devel \
     python3 \
     python3-pip \
     python3-devel \
     rapidjson-devel \
     rpm-build \
     strace \
     unzip \
     zlib-static \
    )

    if [ $FLAVOR == "amzn" ]; then
        RH_LIST+=(\
        system-lsb-core \
        )
    else
        RH_LIST+=(\
        redhat-lsb \
        )
    fi

    if [ $MAJOR == 8 ]; then

        RH_LIST+=(systemd-devel)

        if [ $FLAVOR == "centos" ]; then
            #fix cmake issue in centos 8.*
            RH_LIST+=(\
            libarchive \
            )
        else
            RH_LIST+=(\
            opencv \
            )
        fi

        if [ $docker == 0 ]; then
            RH_LIST+=(\
             kernel-devel-$(uname -r) \
             kernel-headers-$(uname -r) \
            )
        fi

    else

        RH_LIST+=(\
         libpng12-devel \
         libudev-devel \
         kernel-devel-$(uname -r) \
         kernel-headers-$(uname -r) \
         opencv \
         openssl-static \
         protobuf-static \
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
     graphviz \
     libboost-dev \
     libboost-filesystem-dev \
     libboost-program-options-dev \
     libcurl4-openssl-dev \
     libdrm-dev \
     libdw-dev \
     libelf-dev \
     libffi-dev \
     libgtest-dev \
     libjpeg-dev \
     libjson-glib-dev \
     libncurses5-dev \
     libopencv-core-dev \
     libpng-dev \
     libprotoc-dev \
     libssl-dev \
     libsystemd-dev \
     libtiff5-dev \
     libudev-dev \
     libyaml-dev \
     linux-libc-dev \
     lm-sensors \
     lsb-release \
     make \
     ocl-icd-dev \
     ocl-icd-libopencl1 \
     ocl-icd-opencl-dev \
     opencl-headers \
     pciutils \
     perl \
     rapidjson-dev \
     pkg-config \
     protobuf-compiler \
     python3 \
     libpython3-dev \
     python3-breathe \
     python3-pip \
     python3-sphinx \
     python3-sphinx-rtd-theme \
     sphinx-common \
     strace \
     unzip \
     uuid-dev \
    )

    if [ $docker == 0 ] && [ $sysroot == 0 ]; then
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

fd_package_list()
{
    FD_LIST=(\
     boost-devel \
     boost-filesystem \
     boost-program-options \
     boost-static \
     cmake \
     cppcheck \
     curl \
     dkms \
     dmidecode \
     gcc \
     gcc-c++ \
     gdb \
     git \
     glibc-static \
     gnuplot \
     gnutls-devel \
     gtest-devel \
     json-glib-devel \
     kernel-devel-$(uname -r) \
     kernel-headers-$(uname -r) \
     libcurl-devel \
     libdrm-devel \
     libjpeg-turbo-devel \
     libpng12-devel \
     libstdc++-static \
     libtiff-devel \
     libudev-devel \
     libuuid-devel \
     libyaml-devel \
     lm_sensors \
     make \
     ncurses-devel \
     ocl-icd \
     ocl-icd-devel \
     opencl-headers \
     opencv \
     openssl-devel \
     openssl-static \
     pciutils \
     perl \
     pkgconfig \
     protobuf-compiler \
     protobuf-devel \
     protobuf-static \
     python \
     python-pip \
     python2-sphinx \
     python3 \
     python3-pip \
     redhat-lsb \
     rapidjson-devel \
     rpm-build \
     strace \
     systemd-devel \
     systemd-devel \
     unzip \
     zlib-static \
    )
}


suse_package_list()
{
   SUSE_LIST=(\
     cmake \
     curl \
     dkms \
     dmidecode \
     gcc \
     gcc-c++ \
     gdb \
     git-core \
     glibc-devel-static \
     gnuplot \
     json-glib-devel \
     kernel-devel \
     kernel-devel \
     libboost_filesystem1_66_0-devel \
     libboost_program_options1_66_0-devel \
     libcurl-devel \
     libdrm-devel \
     libgnutls-devel \
     libjpeg8-devel \
     libopenssl-devel \
     libpng12-devel \
     libtiff-devel \
     libudev-devel \
     libuuid-devel \
     libxml2-devel \
     libyaml-devel \
     lsb-release \
     make \
     ncurses-devel \
     opencl-cpp-headers \
     pciutils \
     perl \
     pkg-config \
     protobuf-devel \
     python3-devel \
     python3-pip \
     rpm-build \
     strace \
     unzip \
     zlib-devel-static \
   )
}

update_package_list()
{
    if [ $FLAVOR == "ubuntu" ] || [ $FLAVOR == "debian" ]; then
        ub_package_list
    elif [ $FLAVOR == "centos" ] || [ $FLAVOR == "rhel" ] || [ $FLAVOR == "amzn" ]; then
        rh_package_list
    elif [ $FLAVOR == "fedora" ]; then
        fd_package_list
    elif [ $FLAVOR == "sles" ]; then
        suse_package_list
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

    if [ $FLAVOR == "sles" ]; then
        rpm -q "${SUSE_LIST[@]}"
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
    echo "Enabling EPEL repository..."
    rpm -q --quiet epel-release
    if [ $? != 0 ]; then
    	 yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
	     yum check-update
    fi
    echo "Installing cmake3 from EPEL repository..."
    yum install -y cmake3
    if [ $docker == 0 ]; then
        echo "Enabling CentOS SCL repository..."
        yum --enablerepo=extras install -y centos-release-scl
    fi
}

prep_rhel7()
{
    echo "Enabling EPEL repository..."
    rpm -q --quiet epel-release
    if [ $? != 0 ]; then
    	 yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
	 yum check-update
    fi

    echo "Enabling RHEL SCL repository..."
    yum-config-manager --enable rhel-server-rhscl-7-rpms

    echo "Enabling repository 'rhel-7-server-e4s-optional-rpms"
    subscription-manager repos --enable "rhel-7-server-e4s-optional-rpms"

    echo "Enabling repository 'rhel-7-server-optional-rpms'"
    subscription-manager repos --enable "rhel-7-server-optional-rpms"
}

prep_rhel8()
{
    echo "Enabling EPEL repository..."
    rpm -q --quiet epel-release
    if [ $? != 0 ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm
        yum check-update
    fi

    echo "Enabling CodeReady-Builder repository..."
    subscription-manager repos --enable "codeready-builder-for-rhel-8-x86_64-rpms"
}

prep_centos8()
{
    echo "Enabling EPEL repository..."
    rpm -q --quiet epel-release
    if [ $? != 0 ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm
        yum check-update
    fi

    yum install -y dnf-plugins-core

    echo "Enabling PowerTools and AppStream repo for CentOS8 ..."
    #minor version of CentOs
    MINOR=`cat /etc/centos-release | awk -F. '{ print $2 }'`
    if [ $MINOR -gt "2" ]; then
        yum config-manager --set-enabled powertools
        yum config-manager --set-enabled appstream
    else
        yum config-manager --set-enabled PowerTools
        yum config-manager --set-enabled AppStream
    fi
      
}

prep_centos()
{
    if [ $MAJOR == 8 ]; then
        prep_centos8
    else
        prep_centos7
    fi
}

prep_rhel()
{
   if [ $MAJOR == 8 ]; then
        prep_rhel8
    else
        prep_rhel7
    fi

    echo "Installing cmake3 from EPEL repository..."
    yum install -y cmake3
}

prep_amzn()
{
    echo "Installing amazon EPEL..."
    amazon-linux-extras install epel
    echo "Installing cmake3 from EPEL repository..."
    yum install -y cmake3
    echo "Installing opencl header from EPEL repository..."
    yum install -y ocl-icd ocl-icd-devel opencl-headers
}

prep_sles()
{
    echo "Preparing SLES for package dependencies..."

    if [ "$VERSION" == "15.2" ]; then
	SUSEConnect -p sle-module-desktop-applications/$VERSION/x86_64
	SUSEConnect -p sle-module-development-tools/$VERSION/x86_64
	SUSEConnect -p PackageHub/$VERSION/x86_64
	zypper addrepo https://download.opensuse.org/repositories/science/SLE_15_SP2/science.repo
	zypper addrepo https://download.opensuse.org/repositories/devel:libraries:c_c++/SLE_15_SP2/devel:libraries:c_c++.repo
	zypper --no-gpg-checks refresh
	zypper install -y opencl-headers ocl-icd-devel rapidjson-devel
	zypper mr -d -f science devel_libraries_c_c++ devel_languages_python
    fi
}

install()
{
    if [ $FLAVOR == "ubuntu" ] || [ $FLAVOR == "debian" ]; then
        prep_ubuntu

        echo "Installing packages..."
        apt install -y "${UB_LIST[@]}"
    fi

    if [ $FLAVOR == "ubuntu" ] && [ $MAJOR == 20 ]; then
	apt install clang-tidy
    fi

    # Enable EPEL on CentOS/RHEL
    if [ $FLAVOR == "centos" ]; then
        prep_centos
    elif [ $FLAVOR == "rhel" ]; then
        prep_rhel
    elif [ $FLAVOR == "amzn" ]; then
        prep_amzn
    elif [ $FLAVOR == "sles" ]; then
        prep_sles
    fi

    if [ $FLAVOR == "rhel" ] || [ $FLAVOR == "centos" ] || [ $FLAVOR == "amzn" ]; then
        echo "Installing RHEL/CentOS packages..."
        yum install -y "${RH_LIST[@]}"
        if [ $ds9 == 1 ]; then
            yum install -y devtoolset-9
	elif [ $ARCH == "ppc64le" ]; then
            yum install -y devtoolset-7
	elif [ $MAJOR -lt "8" ]  && [ $FLAVOR != "amzn" ]; then
            if [ $FLAVOR == "centos" ]; then
                yum --enablerepo=base install -y devtoolset-9
            else
                yum install -y devtoolset-9
            fi
	fi
    fi

    if [ $FLAVOR == "fedora" ]; then
        echo "Installing Fedora packages..."
        yum install -y "${FD_LIST[@]}"
    fi

    if [ $FLAVOR == "sles" ] ; then
        echo "Installing SUSE packages..."
        ${SUDO} zypper install -y "${SUSE_LIST[@]}"
    fi

    # Install/upgrade pybind11 for building the XRT python bindings
    # We need 2.6.0 minimum version
    pip3 install -U pybind11
}

update_package_list

if [ $validate == 1 ]; then
    validate
else
    install
fi
