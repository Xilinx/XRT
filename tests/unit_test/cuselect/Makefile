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

# % env XILINX_SDX=/proj/xbuilds/2018.3_daily_latest/installs/lin64/SDx/2018.3 make xclbin
# % env XILINX_SDX=/proj/xbuilds/2018.3_daily_latest/installs/lin64/SDx/2018.3 make emconfig
# % run.sh make host.exe
# % run.sh ./host.exe cuselect.xclbin

# sources
KERNEL_SRC := vadd.cl
HOST_SRC := main.cpp

# targets
HOST_EXE := host.exe
XOS := cuselect.$(MODE).xo
XCLBIN := cuselect.$(MODE).xclbin
EMCONFIG_FILE := emconfig.json

# flags
XOCC_LINK_OPTS := \
--nk vadd:4 \
--sp vadd_1.A:bank0 --sp vadd_1.B:bank1 --sp vadd_1.C:bank2 --sp vadd_1.D:bank3 \
--sp vadd_2.A:bank1 --sp vadd_2.B:bank2 --sp vadd_2.C:bank3 --sp vadd_2.D:bank0 \
--sp vadd_3.A:bank2 --sp vadd_3.B:bank3 --sp vadd_3.C:bank0 --sp vadd_3.D:bank1 \
--sp vadd_4.A:bank3 --sp vadd_4.B:bank0 --sp vadd_4.C:bank1 --sp vadd_4.D:bank2 \

XOCC_COMMON_OPTS := -s -t $(MODE) --platform $(DSA)
CFLAGS := -g -std=c++14 -I$(XILINX_XRT)/include
LFLAGS := -L$(XILINX_XRT)/lib -lxilinxopencl -lpthread -lrt
NUMDEVICES := 1

# run time args
EXE_OPT := cuselect.$(MODE).xclbin

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
