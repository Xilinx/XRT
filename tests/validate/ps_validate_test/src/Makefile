#
# Copyright 2022 Xilinx, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# makefile-generator v1.0.3
#

############################## Help Section ##############################
.PHONY: help

help::
	$(ECHO) "Makefile Usage:"
	$(ECHO) "  make all TARGET=<sw_emu/hw_emu/hw> PLATFORM=<FPGA platform> EDGE_COMMON_SW=<rootfs and kernel image path>."
	$(ECHO) "      Command to generate the design for specified Target and Shell."
	$(ECHO) ""
	$(ECHO) "  make clean "
	$(ECHO) "      Command to remove the generated non-hardware files."
	$(ECHO) ""
	$(ECHO) "  make cleanall"
	$(ECHO) "      Command to remove all the generated files."
	$(ECHO) ""
	$(ECHO) "  make test PLATFORM=<FPGA platform>"
	$(ECHO) "     Command to run the application. This is same as 'run' target but does not have any makefile dependency."
	$(ECHO) ""
	$(ECHO) "  make run TARGET=<sw_emu/hw_emu/hw> PLATFORM=<FPGA platform> EDGE_COMMON_SW=<rootfs and kernel image path>"
	$(ECHO) "      Command to run application in emulation."
	$(ECHO) ""
	$(ECHO) "  make build TARGET=<sw_emu/hw_emu/hw> PLATFORM=<FPGA platform> EDGE_COMMON_SW=<rootfs and kernel image path>"
	$(ECHO) "      Command to build xclbin application."
	$(ECHO) ""
	$(ECHO) "  make host"
	$(ECHO) "      Command to build host application."
	$(ECHO) "      EDGE_COMMON_SW is required for SoC shells. Please download and use the pre-built image from - "
	$(ECHO) "      https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/embedded-platforms.html"
	$(ECHO) ""
	$(ECHO) "  make clean "
	$(ECHO) "      Command to remove the generated non-hardware files."
	$(ECHO) ""
	$(ECHO) "  make cleanall"
	$(ECHO) "      Command to remove all the generated files."
	$(ECHO) ""


############################## Setting up Project Variables ##############################
# Points to top directory of Git repository
PWD = $(shell readlink -f .)

TARGET := hw
HOST_ARCH := x86
SYSROOT := $(EDGE_COMMON_SW)/sysroots/cortexa72-cortexa53-xilinx-linux

include ./utils.mk

XSA := 
ifneq ($(PLATFORM), )
XSA := $(call device2xsa, $(PLATFORM))
endif
TEMP_DIR := ./_x.$(TARGET).$(XSA)
BUILD_DIR := ./build_dir.$(TARGET).$(XSA)

CMD_ARGS = -p $(GEN_DIR)

CXXFLAGS += $(opencl_CXXFLAGS) -Wall -O0 -g -std=c++1y
LDFLAGS += $(opencl_LDFLAGS)

########################## Checking if PLATFORM in allowlist #######################
PLATFORM_BLOCKLIST += zc vck190 2019 2018 

############################## Setting up Host Variables ##############################
#Include Required Host Source Files
HOST_SRCS += ./host.cpp 
# Host compiler global settings
CXXFLAGS += -fmessage-length=0 -I$(XILINX_XRT)/include
LDFLAGS += -lrt -lstdc++ -lxrt_core -lxrt_coreutil -lpthread -luuid -L$(XILINX_XRT)/lib
LDFLAGS += -lboost_filesystem -lboost_system

############################## Setting up Kernel Variables ##############################
# Kernel compiler global settings
EXECUTABLE = ./ps_validate_test.exe

############################## Setting up PS Kernel Variables ##############################
CROSSCXX := $(XILINX_VITIS)/gnu/aarch64/lin/aarch64-linux/bin/aarch64-linux-gnu-g++
CROSSCFLAGS = -I$(XILINX_XRT)/include -I$(SYSROOT)/usr/include/xrt -I$(SYSROOT)/ -fPIC -fvisibility=hidden -lrt --sysroot=$(SYSROOT) -Ilibs
CROSSCFLAGS += -Wall -O2 -g
CROSSLDFLAGS = -shared -lxrt_coreutil -L$(SYSROOT)/usr/lib -lpthread -lrt -ldl -lcrypt -lstdc++ --sysroot=$(SYSROOT)

ps_target = ps_validate.so
ps_OBJS = ps_validate.o

############################## Declaring Binary Containers ##############################
BINARY_NAME = ps_validate
BINARY_CONTAINERS += $(BINARY_NAME).xclbin

############################## Setting Targets ##############################
CP = cp -rf

.PHONY: all clean cleanall docs
all: check-platform check-device check-vitis check_edge_sw $(EXECUTABLE) $(ps_target) $(BINARY_CONTAINERS)

.PHONY: host
host: $(EXECUTABLE)

.PHONY: build
build: check-vitis check-device check_edge_sw $(ps_target) $(BINARY_CONTAINERS)

############################## Setting Rules for PS Kernel objects (Building PS Kernels) ##############################
$(ps_OBJS) : ./ps_validate.c
	$(CROSSCXX) -c $(CROSSCFLAGS) -o $@ $<

$(ps_target): $(ps_OBJS)
	$(CROSSCXX) $< $(CROSSLDFLAGS) -o $@

############################## Setting Rules for Binary Containers (Building Kernels) ##############################
$(BINARY_CONTAINERS):
	xclbinutil --add-pskernel $(ps_target) --output $(BINARY_CONTAINERS) --force

############################## Setting Rules for Host (Building Host Executable) ##############################
$(EXECUTABLE): $(HOST_SRCS) | check-xrt
		$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

############################## Setting Essential Checks and Running Rules ##############################
run: all
	$(EXECUTABLE) $(CMD_ARGS)


.PHONY: test
test: $(EXECUTABLE)
	$(EXECUTABLE) $(CMD_ARGS)

############################## Cleaning Rules ##############################
# Cleaning stuff
clean:
	-$(RMDIR) $(EXECUTABLE) $(XCLBIN)/{*sw_emu*,*hw_emu*} 
	-$(RMDIR) profile_* TempConfig system_estimate.xtxt *.rpt *.csv 
	-$(RMDIR) src/*.ll *v++* .Xil emconfig.json dltmp* xmltmp* *.log *.jou *.wcfg *.wdb

cleanall: clean
	-$(RMDIR) build_dir*
	-$(RMDIR) package.*
	-$(RMDIR) _x* *xclbin.run_summary qemu-memory-_* emulation _vimage pl* start_simulation.sh *.xclbin