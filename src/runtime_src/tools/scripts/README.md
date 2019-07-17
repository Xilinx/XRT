Scripts and example usage
=========================

## xrtdeps.sh
This script is used to setup the environments on a supported system (Ubuntu/CentOS) before the first time compiling XRT.
It will install all necessary packages to compile and run XRT.

## pkgdsa.sh
The pkgdsa.sh script is used to generate deb/rpm package for a XSA.
It needs a development platform directory which has .xpfm, `hw/` and `sw/`.

For example:
``` bash
$ pkgdsa.sh -dsa <name> -dsadir <path/to/platform> -pkgdir ./pkgdsa
```

## setup.csh/.sh
At the end, these scripts would be packaged into xrt package and install to `/opt/xilinx/xrt directory`.

## Scripts used to build platform

### xsa\_build.sh
This script is used to generate a XSA from a Vivado Tcl script. It will use the vivado you installed on the system.
You could use this script to build XSA for a platform in `src/platform/`.
This script will create a workspace `xsa_build/` in current directory and copy the Tcl script directory into the workspace.

For example:
```bash
$ xsa_build.sh ../../../platform/zcu104_revmin/zcu104_revmin_xsa.tcl
```

### peta\_build.sh
This script is used to generate PetaLinux image from a XSA file. It Only support PetaLinux 2019.2 and later.
Source PetaLinux setup script before running this script.

It only needs a \<XSA\_name\>.xsa as input and a `<XSA_name>/` directory will be created for PetaLinux project.
Specially, if the XSA directory, which has .xsa file, has path `src/<CPU_ARCH>/xrt/image/`, the script would copy image.ub, fsbl.elf to proper place in `src/` of the XSA directory and prepare sysroot in `src/aarch64-xilinx-linux`.

You are able to configure the PetaLinux project, Linux kernel, devie tree, rootfs by provided a config.sh file.
You have two ways to do that,
1. Use --config option to specify a config.sh file.
2. Create config.sh in the same directory of the .xsa file.

If both methods were used, the script will respect '--config'.
If no spcific configure is needed, this script would use default configurations.

An example config.sh file is `src/platform/zcu102ng/config.sh`.

The PetaLinux project is by default created from '--template zynqMP'. The config.sh allow platform to select template(zynqMP/zynq).
If a BSP is needed, use --bsp/-b option to specify BSP file. Usually image is bigger if you build from a BSP. The benifit is BSP is more stable, less chance to meet u-boot, SD slot, ethernet issues etc.

Before petalinux-build, there is a `pre_build_hook` allow you to do other changes in PetaLinux project, such as apply patchs.
After petalinux-build, there is a `post_build_hook` allow you to do other things you need, such as create BOOT.BIN.

> NOTE: Don't move this script to another place. It uses relative path in XRT repository. To make it easy of used, you could create a symbol link for this script in another place.

For example:
```bash
# Build from BSP, the image would become larger.
# This is required for all revmin platform (Otherwise the SD card and Ethernet would not work).
$ peta_build.sh --bsp <BSP> ./xsa_build/zcu104_revmin/zcu104_revmin.xsa

# Build from template. This is a good choice if you know petalinux very well.
$ peta_build.sh ./xsa_build/zcu102ng/zcu102ng.xsa

# Build with special configurations
$ peta_build.sh --config config.sh ./xsa_build/zcu102ng/zcu102ng.xsa
```

### pfm\_build.sh
This script is used to generate platform from a xsct Tcl script. It will use the xsct installed on the system.
The created platform is in platform/ directory.

For example:
```bash
$ pfm_build.sh ../../../platform/zcu104_revmin/zcu104_revmin_pfm.tcl
```

### ertbuild.sh
This script is used to generate platform from src/platform.
It would call xsa\_build.sh, peta\_build.sh and pfm\_build.sh to build each component.
You need to specify tools in the arguments list. Those tools are vivado, xsct, petalinux.

For example:
```bash
# For zcu104_revmin. Please use <BSP> to build revmin platforms. Do not build revmin platform from template, unless you know what you are doing.
# The -build-xsa options is Yes means build XSA. No means skip it.
# The -full-peta-linux-build options is Yes means build PetaLinux from scratch. No means build xrt and zocl only.
$ ertbuild.sh -platform zcu104_revmin -xrt <XRT_REPO_PATH> -vivado $XILINX_VIVADO/bin/vivado -xsct $XILINX_SDX/bin/xsct -petalinux $PETALINUX -bsp <BSP> -full-peta-linux-build Yes -build-xsa Yes

# For zcu102ng. Do not build zcu102ng from bsp. Otherwise you will see device tree error in petalinux-build.
$ ertbuild.sh -platform zcu102ng -xrt <XRT_REPO_PATH> -vivado $XILINX_VIVADO/bin/vivado -xsct $XILINX_SDX/bin/xsct -petalinux $PETALINUX -full-peta-linux-build Yes -build-xsa Yes
```
