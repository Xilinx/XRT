<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright (C) 2018-2022 Xilinx, Inc. All rights reserved. -->
<!-- Copyright (C) 2022-2026 Advanced Micro Devices, Inc. All rights reserved. -->
Scripts and example usage
=========================

## xrtdeps.sh
This script is used to setup the environments on a supported system (Ubuntu/CentOS) before the first time compiling XRT.
It will install all necessary packages to compile and run XRT.

## pkgdsa.sh (obsoleted, use pkg\_hw\_platform.sh instead)
The pkgdsa.sh script is used to generate deb/rpm package for a XSA.
It needs a development platform directory which has .xpfm, `hw/` and `sw/`.

For example:
``` bash
$ pkgdsa.sh -dsa <name> -dsadir <path/to/platform> -pkgdir ./pkgdsa
```

## setup.csh/.sh
At the end, these scripts would be packaged into xrt package and install to `/opt/xilinx/xrt directory`.
