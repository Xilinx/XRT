VPP := $(XILINX_VITIS)/bin/v++
MODE := hw
DSA := $(XPFM_FILE_PATH)

# sources
KERNEL_SRC := kernel.cl

# targets
XO1 := kernel.$(MODE).xo
XCLBIN := kernel.$(MODE).xclbin
XOS := $(XO1)

# flags
VPP_COMMON_FLAGS := --platform $(DSA) -t $(MODE)
VPP_CFLAGS := $(VPP_COMMON_FLAGS) -c 
VPP_LFLAGS := $(VPP_COMMON_FLAGS) -l 

# primary build targets
.PHONY: xclbin

xclbin:  $(XCLBIN)

clean:
	/bin/rm -rf $(XCLBIN) $(XOS) _x

# kernel rules
$(XO1): $(KERNEL_SRC)
	$(RM) $@
	$(VPP) $(VPP_CFLAGS) -o $@ $+

$(XCLBIN): $(XOS)
	$(VPP) $(VPP_LFLAGS) -o $@ $+

