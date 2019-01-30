#!/bin/tcsh -f

setenv CROSS_COMPILE arm-linux-gnueabihf-
setenv LINUXDIR /home/sonals/git/linux-xlnx
make modules
