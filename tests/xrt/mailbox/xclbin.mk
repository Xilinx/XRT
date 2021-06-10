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
KERNEL_SRC := $(wildcard $(KSRCDIR)/krnl_stream_*.cpp)
KERNEL_XO := $(addprefix $(KBLDDIR)/, $(notdir $(addsuffix .xo,$(basename $(KERNEL_SRC)))))
KERNEL_XCLBIN := $(KBLDDIR)/kernel.$(MODE).xclbin
EMCONFIG_FILE := emconfig.json

VPPCFLAGS := -s -t $(MODE) --platform $(XSA) --temp_dir $(KBLDDIR)/_x
VPPLFLAGS := $(VPPCFLAGS) --config $(KSRCDIR)/krnl_stream_vadd_vmult.ini

%/.vpp :
	mkdir -p $(dir $@)
	touch $@

$(KBLDDIR)/krnl_stream_vadd.xo: $(KSRCDIR)/krnl_stream_vadd.cpp | $(KBLDDIR)/.vpp
	$(VPP) -c -k krnl_stream_vadd $(VPPCFLAGS) -o $@ $<

$(KBLDDIR)/krnl_stream_vdatamover.xo: $(KSRCDIR)/krnl_stream_vdatamover.cpp | $(KSRCDIR)/runPre.tcl $(KBLDDIR)/.vpp
	$(VPP) -c -k krnl_stream_vdatamover --hls.pre_tcl $(firstword $|) $(VPPCFLAGS) -o $@ $<

$(KBLDDIR)/krnl_stream_vmult.xo: $(KSRCDIR)/krnl_stream_vmult.cpp | $(KBLDDIR)/.vpp
	$(VPP) -c -k krnl_stream_vmult $(VPPCFLAGS) -o $@ $<

$(KERNEL_XCLBIN) : $(KERNEL_XO)
	$(VPP) -l $(VPPLFLAGS) -o $@ $^

$(EMCONFIG_FILE):
	emconfigutil --platform $(XSA) --nd 1

xclbin : $(KERNEL_XCLBIN)

emconfig: $(EMCONFIG_FILE)
