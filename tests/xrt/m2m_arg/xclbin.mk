# % source <Vitis>/settings64.sh

# HW:
# % make -f xclbin.mk XSA=... xclbin
#
# HW_EMU
# % make -f xclbin.mk MODE=hw_emu XSA=... xclbin
# % make -f xclbin.mk MODE=hw_emu XSA=... emconfig
VPP := $(XILINX_VITIS)/bin/v++
EMCONFIGUTIL := $(XILINX_VITIS)/bin/emconfigutil
MODE := hw
XSA := $(XPFM_FILE_PATH)
VPPFLAGS := 

KBLDDIR := $(MODE)/$(basename $(notdir $(XSA)))
KSRCDIR := ksrc
KERNEL_SRC := $(wildcard $(KSRCDIR)/*.cpp)
KERNEL_XO := $(addprefix $(KBLDDIR)/, $(notdir $(addsuffix .xo,$(basename $(KERNEL_SRC)))))
KERNEL_XCLBIN := $(KBLDDIR)/kernel.$(MODE).xclbin
EMCONFIG_FILE := emconfig.json

VPPCFLAGS := -s -t $(MODE) --platform $(XSA) --temp_dir $(KBLDDIR)/_x
VPPLFLAGS := $(VPPCFLAGS) --config $(KSRCDIR)/vitis_link.cfg

%/.vpp :
	mkdir -p $(dir $@)
	touch $@

$(KBLDDIR)/krnl_vmult_factor.xo: $(KSRCDIR)/krnl_vmult_factor.cpp | $(KBLDDIR)/.vpp
	$(VPP) -c -k krnl_vmult_factor $(VPPCFLAGS) -o $@ $<

$(KBLDDIR)/krnl_vadd_factor.xo: $(KSRCDIR)/krnl_vadd_factor.cpp | $(KBLDDIR)/.vpp
	$(VPP) -c -k krnl_vadd_factor $(VPPCFLAGS) -o $@ $<

$(KBLDDIR)/krnl_vadd.xo: $(KSRCDIR)/krnl_vadd.cpp | $(KBLDDIR)/.vpp
	$(VPP) -c -k krnl_vadd $(VPPCFLAGS) -o $@ $<

$(KERNEL_XCLBIN) : $(KERNEL_XO)
	$(VPP) -l $(VPPLFLAGS) -o $@ $^

$(EMCONFIG_FILE):
	emconfigutil --platform $(XSA) --nd 1

xclbin : $(KERNEL_XCLBIN)

emconfig: $(EMCONFIG_FILE)
