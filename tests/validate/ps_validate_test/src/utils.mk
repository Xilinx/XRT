#+-------------------------------------------------------------------------------
# The following parameters are assigned with default values. These parameters can
# be overridden through the make command line
#+-------------------------------------------------------------------------------

DEBUG := no

#Generates debug summary report
ifeq ($(DEBUG), yes)
VPP_LDFLAGS += --dk list_ports
endif

ifneq ($(TARGET), hw)	
VPP_FLAGS += -g	
endif

#Setting PLATFORM
ifeq ($(PLATFORM),)
ifneq ($(DEVICE),)
$(warning WARNING: DEVICE is deprecated in make command. Please use PLATFORM instead)
PLATFORM := $(DEVICE)
endif
endif

#Platform Architecture
ifneq ($(PLATFORM),)
DEV_ARCH := $(shell platforminfo -p $(PLATFORM) | grep 'FPGA Family' | sed 's/.*://' | sed '/ai_engine/d' | sed 's/^[[:space:]]*//')
endif

check_edge_sw:
ifndef EDGE_COMMON_SW
	$(error EDGE_COMMON_SW variable is not set, please download and use the pre-built image from https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/embedded-platforms.html)
endif

#Checks for XILINX_VITIS
check-vitis:
ifndef XILINX_VITIS
	$(error XILINX_VITIS variable is not set, please set correctly using "source <Vitis_install_path>/Vitis/<Version>/settings64.sh" and rerun)
endif

#Checks for XILINX_XRT
check-xrt:
ifndef XILINX_XRT
	$(error XILINX_XRT variable is not set, please set correctly using "source /opt/xilinx/xrt/setup.sh" and rerun)
endif

check-device:
	@set -eu; \
	inallowlist=False; \
	inblocklist=False; \
	if [ "$(PLATFORM_ALLOWLIST)" = "" ]; \
	    then inallowlist=True; \
	fi; \
	for dev in $(PLATFORM_ALLOWLIST); \
	    do if [[ $$(echo $(PLATFORM) | grep $$dev) != "" ]]; \
	    then inallowlist=True; fi; \
	done ;\
	for dev in $(PLATFORM_BLOCKLIST); \
	    do if [[ $$(echo $(PLATFORM) | grep $$dev) != "" ]]; \
	    then inblocklist=True; fi; \
	done ;\
	if [[ $$inblocklist == True ]]; \
	    then echo "[ERROR]: This example is not supported for $(PLATFORM)."; exit 1;\
	fi; \
	if [[ $$inallowlist == False ]]; \
	    then echo "[Warning]: The platform $(PLATFORM) not in allowlist."; \
	fi;

#Checks for Correct architecture
ifneq ($(HOST_ARCH), $(filter $(HOST_ARCH),x86))
$(error HOST_ARCH variable not set correctly, supported HOST_ARCH is x86 only)
endif

#Setting CXX
CXX := g++

check-platform:
ifndef PLATFORM
	$(error PLATFORM not set. Please set the PLATFORM properly and rerun. Run "make help" for more details.)
endif

#   device2xsa - create a filesystem friendly name from device name
#   $(1) - full name of device
device2xsa = $(strip $(patsubst %.xpfm, % , $(shell basename $(PLATFORM))))

############################## Deprecated Checks and Running Rules ##############################
check:
	$(ECHO) "WARNING: \"make check\" is a deprecated command. Please use \"make run\" instead"
	make run

exe:
	$(ECHO) "WARNING: \"make exe\" is a deprecated command. Please use \"make host\" instead"
	make host

# Cleaning stuff
RM = rm -f
RMDIR = rm -rf

ECHO:= @echo

docs: README.rst

README.rst: description.json
	$(XF_PROJ_ROOT)/common/utility/readme_gen/readme_gen.py description.json