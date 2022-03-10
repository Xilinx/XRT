#!/bin/bash

# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc. All rights reserved.
#

base="ubuntu:18.04"
arg=""

usage()
{
    echo "Usage: build_xrt_with_docker.sh [options]"
    echo
    echo "[-help]          List this help"
    echo "[-dbg]           Build debug library (default)"
    echo "[-opt]           Build optimized library (default)"
    echo "[-base <image>   Build binaries using specified base docker image; must be ubuntu:18.04 or ubuntu:20.04"
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        -dbg)
            arg="-dbg"
            shift
            ;;
        -opt)
            arg="-opt"
            shift
            ;;
        -base)
            shift
            base=$1
            shift
            ;;
        *)
            echo "unknown option"
            usage
            ;;
    esac
done

# Directory where this script exists
SCRIPT_DIR=$(dirname $(readlink -f $0))

# Root directory of the XRT Repository
REPO_DIR=$(readlink -f $SCRIPT_DIR/../../../../)

# Directory where the xrtdeps.sh script exists
XRTDEPS_DIR=$SCRIPT_DIR

# Directory where the build.sh script exists
BUILD_DIR=$REPO_DIR/build

echo "Creating xrt-build-ubuntu Docker Image with xrtdeps.sh using $base"

# Create an ubuntu based docker image that has all of XRT's required dependencies
# This container will run in detached mode
CONTAINER_ID=$(docker run -d -e DEBIAN_FRONTEND=noninteractive -v $REPO_DIR:$REPO_DIR -w $XRTDEPS_DIR $base /bin/bash -c "apt-get update && ./xrtdeps.sh -docker")

echo "Waiting for xrt-build-ubuntu Docker Image to be ready"
# Wait for the above detached container to stop running
docker wait $CONTAINER_ID

# Save the above container as a new image, and label it xrt-build-ubuntu
docker commit $CONTAINER_ID xrt-build-ubuntu

echo "Building XRT inside of Docker container"
echo "Outputs will be in: $BUILD_DIR"
# Run a new temporary container that will build XRT
docker run --rm --user $UID -v $REPO_DIR:$REPO_DIR -w $BUILD_DIR xrt-build-ubuntu /bin/bash -c "./build.sh $arg"

# Delete the detached container
docker rm $CONTAINER_ID

# Remove the base image that was created with xrtdeps.sh
docker rmi xrt-build-ubuntu

exit 0
