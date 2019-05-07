CROSS_COMPILE := aarch64-linux-gnu-
TARGET := arm64
DRIVER := zocl

KBUILD_OPTS := M=$(CURDIR)/$(DRIVER)
KBUILD_OPTS += ARCH=$(TARGET)
KBUILD_OPTS += CROSS_COMPILE=$(CROSS_COMPILE)

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
