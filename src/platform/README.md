About this directory
====================

# mnt-sd/
This directory is a recipe of the initial script.
It would auto mount partition 1 of the SD card to /mnt and source init.sh script in /mnt if it exists.
To use this recipe, copy it to a yocto or petalinux project. If you still don't know how to use it, please google yocto/petalinux and learn.

# recipes-xrt/
This directory including all of the necessary recipes to enable XRT on an embedded system.
Copy the whole directory to your yocto or petalinux project. You could reference src/runtime\_src/tools/scripts/peta\_build.sh to learn how to use it.

# Platforms
The zcu102ng/ and zcu104\_revmin/ etc. are XRT platforms.
A standard platform directory should looks like,
	__<platform_name>__
	|-- config.sh (optional)
	|-- dynamic_postlink.tcl (optional)
	|-- src
	|   |-- a53 (Determined by CPU arch)
	|   |   |-- xrt
	|   |       |-- image
	|   |       |   |-- init.sh (optional)
	|   |       |   |-- platform\_desc.txt (optional)
	|   |       |
	|   |       |-- linux.bif
	|   |-- generic.readme
	|   |-- prebuilt
	|   |   |-- apsys\_0.xml
	|   |   |-- partitions.xml
	|   |   |-- portinfo.c
	|   |   |-- portinfo.h
	|   |-- qemu
	|   |   |-- lnx
	|   |       |-- pmu\_args.txt
	|   |       |-- qemu\_args.txt
	|   |-- __<platform\_name>__.hpfm
	|-- __<platform_name>__\_dsa.tcl
	|-- __<platform_name>__\_pfm.tcl
	|-- __<platform_name>__\_fragment.dts (optional)

