#!/bin/bash

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
     rpm-build \
     strace \
     unzip \
     zlib-static \
     libcurl-devel \
     python3 \
     python3-pip \
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

    # Centos8
    if [ $MAJOR == 8 ]; then

        RH_LIST+=(\
         systemd-devel \
         kernel-devel-$(uname -r) \
         kernel-headers-$(uname -r) \
        )

    else

        RH_LIST+=(\
         libpng12-devel \
         libudev-devel \
         kernel-devel-$(uname -r) \
         kernel-headers-$(uname -r) \
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
     pciutils \
     pkg-config \
     protobuf-compiler \
     sphinx-common \
     strace \
     unzip \
     uuid-dev \
     libcurl4-openssl-dev \
     libudev-dev \
     libsystemd-dev \
     python3 \
     python3-pip \
     python3-sphinx \
     python3-sphinx-rtd-theme \
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
     systemd-devel \
     python3 \
     python3-pip \
     systemd-devel \
     libpng12-devel \
     libudev-devel \
     kernel-devel-$(uname -r) \
     kernel-headers-$(uname -r) \
     openssl-static \
     protobuf-static \
     python \
     python-pip \
     #docs need
     python2-sphinx \
     dmidecode \
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

    MINOR=`echo ${VERSION} | awk -F. '{print $2}'`
    if [ "$MINOR" != "" ] && [ $MINOR -gt 6 ]; then
      echo "Enabling repository 'rhel-7-server-optional-rpms'"
      subscription-manager repos --enable "rhel-7-server-optional-rpms"
      echo "Enabling repository 'rhel-7-server-e4s-optional-rpms"
      subscription-manager repos --enable "rhel-7-server-e4s-optional-rpms"
    fi
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
    echo "Installing cmake3 from EPEL repository..."
    yum install -y cmake3
    echo "Enabling PowerTools repo for CentOS8 ..."
    yum install -y dnf-plugins-core
    yum config-manager --set-enabled PowerTools
    yum config-manager --set-enabled AppStream
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
    yum install cmake3
    echo "Installing opencl header from EPEL repository..."
    yum install ocl-icd ocl-icd-devel opencl-headers
}

install()
{
    if [ $FLAVOR == "ubuntu" ] || [ $FLAVOR == "debian" ]; then
        prep_ubuntu

        echo "Installing packages..."
        apt install -y "${UB_LIST[@]}"
    fi

    # Enable EPEL on CentOS/RHEL
    if [ $FLAVOR == "centos" ]; then
        prep_centos
    elif [ $FLAVOR == "rhel" ]; then
        prep_rhel
    elif [ $FLAVOR == "amzn" ]; then
        prep_amzn
    fi

    if [ $FLAVOR == "rhel" ] || [ $FLAVOR == "centos" ] || [ $FLAVOR == "amzn" ]; then
        echo "Installing RHEL/CentOS packages..."
        yum install -y "${RH_LIST[@]}"
	if [ $ARCH == "ppc64le" ]; then
            yum install -y devtoolset-7
	elif [ $MAJOR -lt "8" ]  && [ $FLAVOR != "amzn" ]; then
            yum install -y devtoolset-6
	fi
    fi

    if [ $FLAVOR == "fedora" ]; then
        echo "Installing Fedora packages..."
        yum install -y "${FD_LIST[@]}"
    fi

    # Install pybind11 for building the XRT python bindings
    pip3 install pybind11
}

update_package_list

if [ $validate == 1 ]; then
    validate
else
    install
fi
