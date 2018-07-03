#!/bin/bash

# Script to install XRT dependencies
# Note all packages listed here are required for XRT. Some of them like jpeg, png, tiff, etc are used by applications
RH_LIST=(\
     boost-devel \
     boost-filesystem \
     boost-static \
     cmake \
     compat-libtiff3 \
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
     ocl-icd \
     ocl-icd-devel \
     opencl-headers \
     opencv \
     pciutils \
     perl \
     pkgconfig \
     python \
     redhat-lsb \
     rpm-build \
     strace \
     unzip \
     curl \
     protobuf-devel \
     protobuf-compiler \
     protobuf-static \
     )

UB_LIST=(\
     cmake \
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
     libopencv-core-dev \
     libpng-dev \
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
     python3-sphinx \
     python3-sphinx-rtd-theme \
     sphinx-common \
     strace \
     unzip \
     uuid-dev \
     curl \
     libprotoc-dev \
     protobuf-compiler \
)

FLAVOR=`grep '^ID=' /etc/os-release | awk -F= '{print $2}'`
FLAVOR=`echo $FLAVOR | tr -d '"'`

if [ $FLAVOR == "ubuntu" ]; then
    sudo apt install -y "${UB_LIST[@]}"
fi

# Enable GCC 6 compiler set on RHEL/CentOS 7.X
if [ $FLAVOR == "rhel" ]; then
    sudo yum-config-manager --enable rhel-server-rhscl-7-rpms
elif [ $FLAVOR == "centos" ]; then
    sudo yum install centos-release-scl
fi

if [ $FLAVOR == "rhel" ] || [ $FLAVOR == "centos" ]; then
    sudo yum install -y "${RH_LIST[@]}"
    sudo yum install devtoolset-6
fi
