#!/bin/bash

#
# Run this script if xrt installation fails due to a pyopencl issue
# This can help narrow down the reason for pyopencl installation/import failure 
#

#python/pip checks
WARNING=0
PYTHON=$(sudo python -V 2>&1 | grep -Po '(?<=Python )(.+)' | grep -Po '[0-9].[0-9]')
if [ -z $PYTHON ] ; then
    echo "ERROR: Python is not installed on the system"
    exit 1
fi
echo "Python version: $PYTHON"

PIP=$(sudo pip -V 2>&1 | grep -Po '(?<=pip )(.+)' | grep -Po '[0-9][0-9].[0-9]')
if [ -z $PIP ] ; then
    echo "ERROR: Pip is not installed on the system"
    exit 1
elif [ 1 -eq "$(echo "${PIP} < 18.0" | bc)" ]; then
    WARNING=1
    echo "WARNING: An older version of pip is installed. Please try"
    echo "upgrading pip using 'sudo pip install --upgrade pip'"
fi
echo "Pip version: $PIP"

PIP_IN_PYTHON=$(sudo pip -V 2>&1 | grep -Po '(?<=python )(.+)' | grep -Po '[0-9].[0-9]')
if [ 1 -eq "$(echo "${PYTHON} != ${PIP_IN_PYTHON}" | bc)" ]; then
    echo "ERROR: Pip is installed in Python(${PIP_IN_PYTHON}) library"
    echo "whereas user environment python is pointing to Python(${PYTHON})."
    echo "Please make sure that the user env python is pointing to the"
    echo "python with pip."
    exit 1
fi

#pip package checks
SETUPTOOLS=$(sudo pip list 2>&1 | grep setuptools)
if [ -z "$SETUPTOOLS" ] ; then
    echo "ERROR: Please install setuptools using "
    echo "'sudo pip install setuptools'"
fi

NUMPY=$(sudo pip list 2>&1 | grep numpy)
if [ -z "$NUMPY" ] ; then
    echo "ERROR: Please install numpy using "
    echo "'sudo pip install numpy'"
fi

#pyopencl installation/version checks
PYOPENCL=$(python -m pip list 2>&1 | grep -Po '(?<=pyopencl )(.+)' | grep -Po '[0-9][0-9][0-9][0-9]')
if [ -z "$PYOPENCL" ] ; then
    echo "ERROR: Please install pyopencl using "
    echo "'sudo pip install pyopencl'"
    exit 1
elif [ $PYOPENCL -lt 2019 ] ; then
    WARNING=1
    echo "WARNING: An older version of pyopencl is installed. Please upgrade"
    echo "it by running 'sudo pip install --upgrade pyopencl'"
fi

#pyopencl import checks
if python -c "import pyopencl" &> /dev/null ; then
    echo "XRT installation didn't fail because of a pyopencl issue"
    echo "Please recheck the installation log"
elif [ $WARNING -eq 1 ] ; then
    echo "Please address the warnings and retry pyopencl installation"
else
    echo "The script wasn't able to determine the issue with pyopencl "
    echo "installation or import"
fi
exit 0
