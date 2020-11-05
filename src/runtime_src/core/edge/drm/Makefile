CROSS_COMPILE := aarch64-linux-gnu-
TARGET := arm64
DRIVER := zocl

KBUILD_OPTS := M=$(CURDIR)/$(DRIVER)
KBUILD_OPTS += ARCH=$(TARGET)
KBUILD_OPTS += CROSS_COMPILE=$(CROSS_COMPILE)

GIT_HASH := `git rev-parse --verify HEAD`
GIT_HASH_DATE := `git log -1 --pretty=format:%cD`
GIT_BRANCH := `git rev-parse --abbrev-ref HEAD`
TMP_MODIFIED_FILES := `git status --porcelain -u no`
GIT_MODIFIED_FILES := $(subst \\n,\,,$(TMP_MODIFIED_FILES))
DATE_NOW := `date \"+%Y-%m-%d\"`

XRT_DRIVER_VERSION := 2.8.0

all: modules check-env

modules: check-env
	$(MAKE) -C $(LINUXDIR) $(KBUILD_OPTS) modules

install:
	$(MAKE) -C $(LINUXDIR) $(KBUILD_OPTS) modules_install

clean:
	$(MAKE) -C $(LINUXDIR) $(KBUILD_OPTS) clean

check-env:
ifndef LINUXDIR
	$(error LINUXDIR is undefined)
endif
