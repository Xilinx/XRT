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
MODE := hw_emu
DSA := xilinx_vcu1525_dynamic_5_1

# % env XILINX_SDX=/proj/xbuilds/2018.2_daily_latest/installs/lin64/SDx/2018.2 make xclbin
# % env XILINX_SDX=/proj/xbuilds/2018.2_daily_latest/installs/lin64/SDx/2018.2 make emconfig
# % run.sh make host.exe
# % run.sh ./host.exe addone.xclbin

# sources
KERNEL_SRC := addone.cl
HOST_SRC := main.cpp

# targets
HOST_EXE := host.exe
XOS := addone.$(MODE).xo
XCLBIN := addone.$(MODE).xclbin
EMCONFIG_FILE := emconfig.json

# flags
XOCC_LINK_OPTS := \
--nk add0:1:add00 \
--nk add1:1:add10 \
--nk add2:1:add20 \
--nk add3:1:add30 \
--sp add00.m_axi_gmem:bank0 \
--sp add10.m_axi_gmem:bank1 \
--sp add20.m_axi_gmem:bank2 \
--sp add30.m_axi_gmem:bank3

XOCC_COMMON_OPTS := -s -t $(MODE) --platform $(DSA)
CFLAGS := -g -std=c++11 -I$(XILINX_XRT)/include
LFLAGS := -L$(XILINX_XRT)/lib -lxilinxopencl -lpthread -lrt
NUMDEVICES := 1

# run time args
EXE_OPT := addone.xclbin

# primary build targets
.PHONY: xclbin compile all clean run

xclbin:  $(XCLBIN)
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

$(EMCONFIG_FILE):
	$(EMCONFIGUTIL) --nd $(NUMDEVICES) --od . --platform $(DSA)

run: $(XCLBIN) $(HOST_EXE) $(EMCONFIG_FILE)
	./host.exe $(EXE_OPT)
