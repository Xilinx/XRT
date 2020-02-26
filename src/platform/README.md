About this directory
====================

# Platforms 
All platforms have been moved to https://github.com/Xilinx/Vitis_Embedded_Platform_Source.

# mnt-sd/
The mnt-sd/ folder has been removed. You could find it in above platform repo.
Go to a platform, like Xilinx_Official_Platforms/zcu102_base.
The mnt-sd/ is in project-spec/meta-user/recipes-apps folder.

# recipes-xrt/
The XRT recipe has been moved to https://github.com/Xilinx/meta-xilinx/tree/master/meta-xilinx-bsp/recipes-xrt.
Since PetaLinux 2019.2 release, XRT is a build-in recipe.
You cound select xrt or zocl in the menu of "petalinux-config -c rootfs".
