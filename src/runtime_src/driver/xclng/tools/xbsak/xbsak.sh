#!/bin/bash

# Wrapper script for xbutil

echo "!!Xbsak is obsolete and will be removed soon. Please use xbutil, instead!!"
echo ""

PATH_DIR=`dirname $0`
${PATH_DIR}/xbutil "$@"
