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

<pre><code>    <b>&lt;platform_name&gt;</b>
    |-- config.sh (optional)
    |-- dynamic_postlink.tcl (optional)
    |-- src
    |   |-- a53 (Determined by CPU arch)
    |   |   |-- xrt
    |   |       |-- image
    |   |       |   |-- init.sh (optional)
    |   |       |   |-- platform_desc.txt (optional)
    |   |       |
    |   |       |-- linux.bif
    |   |-- generic.readme
    |   |-- prebuilt
    |   |   |-- apsys_0.xml
    |   |   |-- partitions.xml
    |   |   |-- portinfo.c
    |   |   |-- portinfo.h
    |   |-- qemu
    |   |   |-- lnx
    |   |       |-- pmu_args.txt
    |   |       |-- qemu_args.txt
    |   |-- <b>&lt;platform_name&gt;</b>.hpfm
    |-- <b>&lt;platform_name&gt;</b>_dsa.tcl
    |-- <b>&lt;platform_name&gt;</b>_pfm.tcl
    |-- <b>&lt;platform_name&gt;</b>_fragment.dts (optional)
</code></pre>

