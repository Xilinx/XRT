# % .../XRT/build/run.sh make -f xclbin.mk DSA=... xclbin
# % .../XRT/build/run.sh make -f xclbin.mk DSA=... emconfig

VPP := $(XILINX_VITIS)/bin/v++
EMCONFIGUTIL := $(XILINX_VITIS)/bin/emconfigutil
MODE := hw_emu
DSA := $(XPFM_FILE_PATH)

BLDDIR := $(MODE)/$(basename $(notdir $(DSA))_$(MODE))

# sources
KERNEL_SRC := kernel.cl

# targets
XOS := $(BLDDIR)/kernel.xo
XCLBIN := $(BLDDIR)/kernel.$(MODE).xclbin
EMCONFIG_FILE := $(BLDDIR)/emconfig.json

# flags
VPP_LINK_OPTS := \
--nk addone:8

VPP_COMMON_OPTS := -s -t $(MODE) --platform $(DSA) --temp_dir $(BLDDIR)

# primary build targets
.PHONY: xclbin

xclbin:  $(XCLBIN)
emconfig: $(EMCONFIG_FILE)

clean:
	-$(RM) $(XCLBIN) $(XOS)

$(BLDDIR):
	mkdir -p $(BLDDIR)

# kernel rules
$(XOS): $(KERNEL_SRC) $(BLDDIR)
	$(RM) $@
	$(VPP) $(VPP_COMMON_OPTS) $(KERNEL_1_OPTS) -c -o $@ $<

$(XCLBIN): $(XOS)
	$(VPP) $(VPP_COMMON_OPTS) -l -o $@ $< $(VPP_LINK_OPTS)

$(EMCONFIG_FILE): $(BLDDIR)
	$(EMCONFIGUTIL) --nd 1 --od $(BLDDIR) --platform $(DSA)

