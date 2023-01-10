KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

# Base on Linux Documentation/kbuild/modules.txt.
# $(src) provides the absolute path by pointing to the directory
# where the currently executing kbuild file is located.
ifneq ($(src),)
# building modules
ifneq ($(wildcard $(src)/../common),)
# dkms flow
common_dir := ../common
else
# build from source code
common_dir := ../../../common/drv
endif

else
# clean
make_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
common_dir := $(make_dir)/../../../common/drv
ifeq ($(wildcard $(make_dir)/../../../common/drv),)
common_dir := $(make_dir)/../common
endif

endif

ccflags-y := -I$(src)/include -I$(src)/../../include -I$(src)/../../../include -I$(src)/../../../common/drv/include

#flags passed from xrt_git.bb file are added below
ifneq ($(cflags_zocl),)
	ccflags-y += $(cflags_zocl)
endif

# Version is specific to release
XRT_DRIVER_VERSION ?= 2.15.0

# Add patch number in version if 'XRT_VERSION_PATCH' env variable is defined
ifneq ($(XRT_VERSION_PATCH),)
	XRT_DRIVER_VERSION:=$(XRT_DRIVER_VERSION:.0=.$(XRT_VERSION_PATCH))
endif

drv_common-y   := $(common_dir)/kds_core.o \
                  $(common_dir)/cu_hls.o \
                  $(common_dir)/fast_adapter.o \
                  $(common_dir)/xrt_cu.o \
                  $(common_dir)/xrt_xclbin.o

zocl-y := \
	$(drv_common-y) \
	zocl_sysfs.o \
	zocl_ioctl.o \
	zocl_ert.o \
	zocl_watchdog.o \
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
	cu_scu.o \
	scu.o \
	zocl_error.o \
	zocl_aie.o \
	zocl_ctrl_ert.o \
	zocl_xgq.o \
	zocl_lib.o \
	zocl_cu_xgq.o \
	zocl_rpu_channel.o \
	zocl_csr_intc.o \
	zocl_xgq_intc.o

obj-m	+= zocl.o

SRC := $(shell pwd)

all: modules

git_variables:
	$(eval GIT_HASH ?= $(shell git rev-parse --verify HEAD))
	$(eval GIT_HASH_DATE ?= $(shell git log -1 --pretty=format:%cD))
	$(eval GIT_BRANCH ?= $(shell git rev-parse --abbrev-ref HEAD))
	$(eval TMP_MODIFIED_FILES ?= $(shell git status --porcelain -uno))
	$(eval GIT_MODIFIED_FILES ?= $(subst \\n,\,,$(TMP_MODIFIED_FILES)))
	$(eval DATE_NOW ?= $(shell date "+%Y-%m-%d"))

	$(eval cflags_module := -DXRT_HASH="\"$(GIT_HASH)\"" -DXRT_HASH_DATE="\"$(GIT_HASH_DATE)\"" -DXRT_BRANCH="\"$(GIT_BRANCH)\"" -DXRT_MODIFIED_FILES="\"$(GIT_MODIFIED_FILES)\"" -DXRT_DATE="\"$(DATE_NOW)\"" -DXRT_DRIVER_VERSION="\"$(XRT_DRIVER_VERSION)\"")

modules: git_variables
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) CFLAGS_MODULE='$(cflags_module)' modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
	$(MAKE) -C $(common_dir) clean
