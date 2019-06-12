Scripts and example usage
=========================

## xrtdeps.sh
This script is used to setup the environments on a supported system (Ubuntu/CentOS) before the first time compiling XRT.
It will install all necessary packages to compile and run XRT.

## pkgdsa.sh
The pkgdsa.sh script is used to generate deb/rpm package for a DSA.
It needs a development platform directory which has .xpfm, `hw/` and `sw/`.

For example:
``` bash
$ pkgdsa.sh -dsa <name> -dsadir <path/to/platform> -pkgdir ./pkgdsa
```

## setup.csh/.sh
At the end, these scripts would be packaged into xrt package and install to `/opt/xilinx/xrt directory`.

## Scripts used to build platform

### dsa\_build.sh
This script is used to generate a DSA from a Vivado Tcl script. It will use the vivado you installed on the system.
You could use this script to build DSA for a platform in `src/platform/`.
This script will create a workspace `dsa_build/` in current directory and copy the Tcl script directory into the workspace.

For example:
```bash
$ dsa_build.sh ../../../platform/zcu104_revmin/zcu104_revmin_dsa.tcl
```

### peta\_build.sh
This script is used to generate PetaLinux image from a DSA file. Source PetaLinux setup script before running this script.

It only needs a \<DSA\_name\>.dsa as input and a `<DSA_name>/` directory will be created for PetaLinux project.
Specially, if the DSA directory, which has .dsa file, has path `src/<CPU_ARCH>/xrt/image/`, the script would copy image.ub, fsbl.elf to proper place in `src/` of the DSA directory and prepare sysroot in `src/aarch64-xilinx-linux`.

You are able to configure the PetaLinux project, Linux kernel, devie tree, rootfs by provided a config.sh file.
You have two ways to do that,
1. Use --config option to specify a config.sh file.
2. Create config.sh in the same directory of the .dsa file.

If both methods were used, the script will respect '--config'.
If no spcific configure is needed, this script would use default configurations.

An example config.sh file is `src/platform/zcu102ng/config.sh`.

The PetaLinux project is by default created from '--template zynqMP'. The config.sh allow platform to select template(zynqMP/zynq).
If a BSP is needed, use --bsp/-b option to specify BSP file.

Before petalinux-build, there is a `pre_build_hook` allow you to do other changes in PetaLinux project, such as apply patchs.
After petalinux-build, there is a `post_build_hook` allow you to do other things you need, such as create BOOT.BIN.

> NOTE: Don't move this script to another place. It uses relative path in XRT repository. To make it easy of used, you could create a symbol link for this script in another place.

For example:
```bash
# Build from BSP. This is required for zcu104_revmin platform.
$ peta_build.sh --bsp <BSP> ./dsa_build/zcu104_revmin.dsa

# Build from template by default
$ peta_build.sh ./dsa_build/zcu102ng.dsa

# Build with special configurations
$ peta_build.sh --config config.sh ./dsa_build/zcu102ng.dsa
```

### pfm\_build.sh
This script is used to generate platform from a xsct Tcl script. It will use the xsct installed on the system.
The created platform is in platform/ directory.

For example:
```bash
$ pfm_build.sh ../../../platform/zcu104_revmin/zcu104_revmin_pfm.tcl
```

### petalinux.sh
This script is used to generate platform from src/platform.
It would call dsa\_build.sh, peta\_build.sh and pfm\_build.sh to build each component.
You need to specify tools in the arguments list. Those tools are vivado, xsct, petalinux.

For example:
```bash
# For zcu104_revmin
$ petalinux.sh $XILINX_VIVADO/bin/vivado $XILINX_SDX/bin/xsct $PETALINUX zcu104_revmin <XRT_REPO_PATH> <BSP>

# For zcu102ng
$ petalinux.sh $XILINX_VIVADO/bin/vivado $XILINX_SDX/bin/xsct $PETALINUX zcu102ng <XRT_REPO_PATH>
```
