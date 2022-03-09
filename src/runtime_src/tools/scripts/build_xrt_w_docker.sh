#!/bin/bash

# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#

# Must be Ubuntu 18.04 or Ubuntu 20.04
BASE_IMAGE=$1:-ubuntu:18.04

SCRIPT_DIR=$(dirname $(readlink -f $0))

REPO_DIR=$(readlink -f $SCRIPT_DIR/../../../../)

XRTDEPS_DIR=$SCRIPT_DIR

BUILD_DIR=$REPO_DIR/build

echo "Creating xrt-build-ubuntu Docker Image with xrtdeps.sh"

CONTAINER_ID=$(docker run -d -e DEBIAN_FRONTEND=noninteractive -v $REPO_DIR:$REPO_DIR -w $XRTDEPS_DIR ubuntu:18.04 /bin/bash -c " apt-get update && ./xrtdeps.sh")

docker wait $CONTAINER_ID

docker commit $CONTAINER_ID xrt-build-ubuntu

echo "Building XRT inside of Docker container"
echo "Outputs will be in: $BUILD_DIR"
docker run --rm --user $UID -v $REPO_DIR:$REPO_DIR -w $BUILD_DIR xrt-build-ubuntu /bin/bash -c "./build.sh"

docker rm $CONTAINER_ID
