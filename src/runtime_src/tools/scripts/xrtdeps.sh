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
     cppcheck \
     curl \
     dkms \
     elfutils-devel \
     elfutils-libs \
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
     systemtap-sdt-devel \
     unzip \
     zlib-static \
    )

    if [ $FLAVOR == "amzn" ]; then
        RH_LIST+=(\
        system-lsb-core \
        compat-libtiff3 \
        )
    elif [ $MAJOR -le 8 ]; then
        RH_LIST+=(\
        redhat-lsb \
        compat-libtiff3 \
        )
    fi

    if [ $MAJOR -ge 8 ]; then

        RH_LIST+=(\
          systemd-devel \
          libarchive \
          )

        if [ $FLAVOR == "rhel" ]; then
            RH_LIST+=(\
              opencv \
            )
        fi

    else

        RH_LIST+=(\
         libpng12-devel \
         libudev-devel \
         opencv \
         openssl-static \
         protobuf-static \
        )

    fi

    if [ $docker == 0 ]; then
        if [ $FLAVOR == "centos" ]; then
        # In CentOs kernel-devel and headers with $(uname -r) version
        # are not available always which causes xrtdeps to fail if we
        # include these packages with specific version.
            RH_LIST+=(\
              kernel-devel \
              kernel-headers \
            )
        else
            RH_LIST+=(\
              kernel-devel-$(uname -r) \
              kernel-headers-$(uname -r) \
            )
        fi
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
     systemtap-sdt-dev \
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
     elfutils-devel \
     elfutils-libs \
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
     libffi-devel \
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
     systemtap-sdt-devel \
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
     systemtap-sdt-devel \
     unzip \
     zlib-devel-static \
   )
}

mariner_package_list()
{
    MN_LIST=(\
     binutils \
     boost-devel \
     boost-static \
     cmake \
     cppcheck \
     curl \
     dkms \
     elfutils-devel \
     elfutils-libs \
     gcc \
     gcc-c++ \
     gdb \
     git \
     glibc-static \
     gmock-devel \
     gnuplot \
     gnutls-devel \
     gtest-devel \
     json-glib-devel \
     libcurl-devel \
     libdrm-devel \
     libffi-devel \
     libjpeg-turbo-devel \
     libpng12-devel \
     libstdc++-static \
     libtiff-devel \
     libudev-devel \
     libuuid-devel \
     libyaml-devel \
     lm_sensors \
     lsb-release \
     make \
     ncurses-devel \
     ocl-icd \
     ocl-icd-devel \
     opencl-headers \
     openssl-devel \
     openssl-static \
     pciutils \
     perl \
     pkgconfig \
     protobuf-compiler \
     protobuf-devel \
     protobuf-static \
     python3 \
     python3-pip \
     python3-devel \
     rapidjson-devel \
     rpm-build \
     strace \
     unzip \
     zlib-static \
    )
}

update_package_list()
{
    if [ $FLAVOR == "ubuntu" ] || [ $FLAVOR == "debian" ]; then
        ub_package_list
    elif [ $FLAVOR == "centos" ] || [ $FLAVOR == "rhel" ] || [ $FLAVOR == "amzn" ] || [ $FLAVOR == "almalinux" ]; then
        rh_package_list
    elif [ $FLAVOR == "fedora" ]; then
        fd_package_list
    elif [ $FLAVOR == "sles" ]; then
        suse_package_list
    elif [ $FLAVOR == "mariner" ]; then
        mariner_package_list
    else
        echo "unknown OS flavor $FLAVOR"
        exit 1
    fi
}

validate()
{
    if [ $FLAVOR == "ubuntu" ] || [ $FLAVOR == "debian" ]; then
        #apt-get -qq list "${UB_LIST[@]}"
        dpkg -l "${UB_LIST[@]}" > /dev/null
        if [ $? == 0 ] ; then
            # Validate we have OpenCL 2.X headers installed
            dpkg-query -s opencl-headers | grep '^Version: 2\.'
        fi
    fi

    if [ $FLAVOR == "centos" ] || [ $FLAVOR == "rhel" ] || [ $FLAVOR == "amzn" ] || [ $FLAVOR == "almalinux" ]; then
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

    if [ $FLAVOR == "mariner" ]; then
        rpm -q "${MN_LIST[@]}"
        if [ $? == 0 ] ; then
            # Validate we have OpenCL 2.X headers installed
            rpm -q -i opencl-headers | grep '^Version' | grep ': 2\.'
        fi
    fi

}

prep_ubuntu()
{
    echo "Preparing ubuntu ..."
    # Update the list of available packages
    apt-get update
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

prep_rhel9()
{
    echo "Enabling EPEL repository..."
    rpm -q --quiet epel-release
    if [ $? != 0 ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm
        yum check-update
    fi

    echo "Enabling CodeReady-Builder repository..."
    subscription-manager repos --enable "codeready-builder-for-rhel-9-x86_64-rpms"
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
    if [ -z "$MINOR" ]; then
        MINOR=3
    fi
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
    if [ $MAJOR -ge 9 ]; then
        prep_rhel9
    else
        if [ $MAJOR == 8 ]; then
             prep_rhel8
        else
             prep_rhel7
        fi
        echo "Installing cmake3 from EPEL repository..."
        yum install -y cmake3
    fi
}

prep_amzn()
{
    echo "Installing amazon EPEL..."
    amazon-linux-extras install -y epel
    echo "Installing cmake3 from EPEL repository..."
    yum install -y cmake3
    echo "Installing opencl header from EPEL repository..."
    yum install -y ocl-icd ocl-icd-devel opencl-headers
}

prep_mariner()
{
    echo "Installing Mariner extended repository ..."
    dnf install -y mariner-repos-extended
    # echo "Installing cmake3 from EPEL repository..."
    # yum install -y cmake3
    echo "Installing opencl header from Mariner extended repository..."
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
        zypper addrepo https://download.opensuse.org/repositories/home:cvoegl:pmem/SLE_15_SP2/home:cvoegl:pmem.repo
        zypper --no-gpg-checks refresh
        zypper install -y opencl-headers ocl-icd-devel rapidjson-devel
        zypper mr -d -f science home_cvoegl_pmem
    fi
}

prep_alma9()
{
    echo "Enabling EPEL repository..."
    yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm
    yum check-update

    echo "Enabling CodeReady-Builder repository..."
    dnf config-manager --set-enabled crb
}

prep_alma()
{
    if [ $MAJOR -ge 9 ]; then
        prep_alma9
    fi
}

install_pybind11()
{
    echo "Installing pybind11..."
    if [ $FLAVOR == "mariner" ]; then
        sudo dnf install -y pybind11-devel python3-pybind11
    elif [ $FLAVOR == "ubuntu" ] && [ $MAJOR -ge 23 ]; then
        apt-get install -y pybind11-dev
    else
        # Install/upgrade pybind11 for building the XRT python bindings
        # We need 2.6.0 minimum version
        pip3 install -U pybind11
    fi
}

install_hip()
{
    # For building HIP bindings for XRT an install of ROCm HIP is required; install HIP if
    # supported by the Linux distribution
    if [ $FLAVOR == "ubuntu" ]; then
        # Check if ROCm has already been manually installed by the user
        dpkg-query -s hip-dev
        if [ $? == 0 ]; then
            echo "hip-dev already installed..."
            dpkg-query -L hip-dev | grep hip/hip_runtime_api.h
        elif [ $MAJOR -ge 23 ]; then
            # From Ubuntu 23.04 (lunar) onwards, a version of HIP devel tools is bundled--
            # https://packages.ubuntu.com/
            apt-get install -y libamdhip64-dev
        else
            echo "Manual installation of HIP is required, please follow instructions on ROCm website--"
            echo "https://rocm.docs.amd.com/projects/install-on-linux/en/latest/tutorial/install-overview.html"
        fi
    elif [ $FLAVOR == "fedora" ]; then
        rpm -q hip-devel
        if [ $? == 0 ]; then
            echo "hip-devel already installed..."
            rpm -q -l hip-devel | grep hip/hip_runtime_api.h
        elif [ $MAJOR -ge 38 ]; then
            # From version 38 onwards, a version of HIP devel tools is bundled--
            # https://packages.fedoraproject.org/pkgs/rocclr/hip-devel/
            yum install -y hip-devel
        else
            echo "Manual installation of HIP is required, please follow instructions on ROCm website--"
            echo "https://rocm.docs.amd.com/projects/install-on-linux/en/latest/tutorial/install-overview.html"
        fi
    else
            echo "Manual installation of HIP is required, please follow instructions on ROCm website--"
            echo "https://rocm.docs.amd.com/projects/install-on-linux/en/latest/tutorial/install-overview.html"
    fi
}

install()
{
    if [ $FLAVOR == "ubuntu" ] || [ $FLAVOR == "debian" ]; then
        prep_ubuntu

        echo "Installing packages..."
        apt-get install -y "${UB_LIST[@]}"
    fi

    if [ $FLAVOR == "ubuntu" ] && [ $MAJOR == 20 ]; then
        apt-get install -y clang-tidy
    fi

    # Enable EPEL on CentOS/RHEL
    if [ $FLAVOR == "centos" ]; then
        prep_centos
    elif [ $FLAVOR == "rhel" ]; then
        prep_rhel
    elif [ $FLAVOR == "amzn" ]; then
        prep_amzn
    elif [ $FLAVOR == "mariner" ]; then
        prep_mariner
    elif [ $FLAVOR == "sles" ]; then
        prep_sles
    elif [ $FLAVOR == "almalinux" ]; then
        prep_alma
    fi

    if [ $FLAVOR == "rhel" ] || [ $FLAVOR == "centos" ] || [ $FLAVOR == "amzn" ] || [ $FLAVOR == "almalinux" ]; then
        echo "Installing RHEL/CentOS packages..."
        yum install -y "${RH_LIST[@]}"
        if [ $ds9 == 1 ]; then
            yum install -y devtoolset-9
        elif [ $ARCH == "ppc64le" ]; then
            yum install -y devtoolset-7
        elif [ $MAJOR -lt "8" ]  && [ $FLAVOR != "amzn" ]; then
            if [ $FLAVOR == "centos" ]; then
                yum install -y centos-release-scl-rh
            fi
            yum install -y devtoolset-9
        fi
    fi

    if [[ $FLAVOR == "centos" || $FLAVOR == "rhel" ]] && [ $MAJOR -eq "8" ]; then
        yum install -y gcc-toolset-9-toolchain
    fi

    if [ $FLAVOR == "fedora" ]; then
        echo "Installing Fedora packages..."
        yum install -y "${FD_LIST[@]}"
    fi

    if [ $FLAVOR == "sles" ] ; then
        echo "Installing SUSE packages..."
        ${SUDO} zypper install -y "${SUSE_LIST[@]}"
    fi

    if [ $FLAVOR == "mariner" ]; then
        echo "Installing Mariner packages..."
        dnf install -y "${MN_LIST[@]}"
    fi

    install_pybind11

    install_hip
}

update_package_list

if [ $validate == 1 ]; then
    validate
else
    install
fi
