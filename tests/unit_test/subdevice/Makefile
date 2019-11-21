# Copyright (C) 2016-2017 Xilinx, Inc
#
# Licensed under the Apache License, Version 2.0 (the "License"). You may
# not use this file except in compliance with the License. A copy of the
# License is located at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.

XOCC := $(XILINX_SDX)/bin/xocc
EMCONFIGUTIL := $(XILINX_SDX)/bin/emconfigutil
MODE ?= hw_emu
DSA ?= xilinx_vcu1525_dynamic_5_1

# % env XILINX_SDX=/proj/xbuilds/2018.3_daily_latest/installs/lin64/SDx/2018.3 make xclbin
# % env XILINX_SDX=/proj/xbuilds/2018.3_daily_latest/installs/lin64/SDx/2018.3 make emconfig
# % run.sh make host.exe
# % run.sh ./host.exe cuselect.xclbin

# sources
KERNEL_SRC := addone.cl
HOST_SRC := main.cpp

# targets
HOST_EXE := host.exe
XOS := addone.$(MODE).xo
XCLBIN := addone.$(MODE).xclbin
EMCONFIG_FILE := emconfig.json

# flags
XOCC_LINK_OPTS :=  \
 --nk addone:4 \
 --sp addone_1.a:bank0 \
 --sp addone_1.b:bank1 \
 --sp addone_2.a:bank1 \
 --sp addone_2.b:bank2 \
 --sp addone_3.a:bank2 \
 --sp addone_3.b:bank3 \
 --sp addone_4.a:bank3  \
 --sp addone_4.b:bank0

XOCC_COMMON_OPTS := -s -t $(MODE) --platform $(DSA)
CFLAGS := -g -std=c++14 -I$(XILINX_XRT)/include
LFLAGS := -L$(XILINX_XRT)/lib -lxilinxopencl -lpthread -lrt
NUMDEVICES := 1

# run time args
EXE_OPT := addone.$(MODE).xclbin

# primary build targets
.PHONY: xclbin compile all clean

xclbin: $(XCLBIN)
compile: $(HOST_EXE)
all: clean xclbin compile run

clean:
	-$(RM) $(EMCONFIG_FILE) $(HOST_EXE) $(XCLBIN) $(XOS)

# kernel rules
$(XOS): $(KERNEL_SRC)
	$(RM) $@
	$(XOCC) $(XOCC_COMMON_OPTS) -c -o $@ $+


$(XCLBIN): $(XOS)
	$(XOCC) $(XOCC_COMMON_OPTS) -l -o $@ $+ $(XOCC_LINK_OPTS)

# host rules
$(HOST_EXE): $(HOST_SRC)
	g++ $(CFLAGS) -o $@ $+ $(LFLAGS)
	@echo 'Compiled Host Executable: $(HOST_EXE)'

ifneq ($(MODE),hw)
$(EMCONFIG_FILE):
	$(EMCONFIGUTIL) --nd $(NUMDEVICES) --od . --platform $(DSA)
else
$(EMCONFIG_FILE):
endif

run: $(XCLBIN) $(HOST_EXE) $(EMCONFIG_FILE)
	./host.exe $(EXE_OPT)
