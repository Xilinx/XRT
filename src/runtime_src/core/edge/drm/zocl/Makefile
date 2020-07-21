KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

# Base on Linux Documentation/kbuild/modules.txt.
# $(src) provides the absolute path by pointing to the directory
# where the currently executing kbuild file is located.
ifneq ($(src),)
# building modules
ifneq ($(wildcard $(src)/../common),)
# dkms dir
common_dir = ../common
else
# build from source code
common_dir = ../../../common/drv
endif

else
# clean
common_dir = ../../../common/drv
ifeq ($(wildcard ../../../common/drv),)
common_dir = ../common
endif

endif

ccflags-y := -I$(src)/include -I$(src)/../../include -I$(src)/../../../include -I$(src)/../../../common/drv/include

drv_common-y   := $(common_dir)/kds_core.o \
		  $(common_dir)/cu_hls.o \
		  $(common_dir)/xrt_cu.o \
		  $(common_dir)/xrt_xclbin.o

zocl-y := \
	$(drv_common-y) \
	sched_exec.o \
	zocl_sysfs.o \
	zocl_ioctl.o \
	zocl_ert.o \
	zocl_drv.o \
	zocl_bo.o \
	zocl_dma.o \
	zocl_cu.o \
	zocl_mailbox.o \
	zocl_ov_sysfs.o \
	zocl_ospi_versal.o \
	zocl_xclbin.o \
	zocl_sk.o \
	zocl_kds.o \
	cu.o \
	zocl_generic_cu.o

obj-m	+= zocl.o

SRC := $(shell pwd)

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
	$(MAKE) -C $(common_dir) clean
