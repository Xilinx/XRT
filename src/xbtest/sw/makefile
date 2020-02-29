
code_coverage =

TARGET = xbtest
SRC_DIRS = ./src
SRC_DIRS += ./lib/xcl2
INCLUDE_DIR = ./include
INCLUDE_LIB_DIR = ./lib/xcl2
OUTPUT_DIR = ./output

SRCS := $(shell find $(SRC_DIRS) -name *.cpp)
OBJS := $(addsuffix .o, $(basename $(SRCS)))

CPPFLAGS = -O0 -g -Wall -c -I$(INCLUDE_DIR) -I$(INCLUDE_LIB_DIR) -I/opt/xilinx/xrt/include/
LDFLAGS = -lxilinxopencl -lxrt_core -lpthread -lrt -lstdc++ -L/opt/xilinx/xrt/lib/

## if you want to compile on your VM, un comment the 2 following lines
#LDFLAGS += -lxrt_coreutil
#CXX = /bin/g++

# check that gcc version is older than 7.3 if such, insert extra flag
GCC_VERSION := $(shell expr `gcc -dumpversion | cut -f 1-2 -d.` \< 7)

#OS := $(shell lsb_release --id)
#ifeq "$(OS)" "Distributor ID:	CentOS"
OS := $(shell lsb_release --id)
ifeq "$(OS)" "Distributor ID:	RedHatEnterpriseServer"
	REDHAT := 1
else
	REDHAT := 0
endif

ifeq "$(OS)" "Distributor ID:	CentOS"
	CENTOS := 1
else
	CENTOS := 0
endif

ifeq "$(REDHAT)" "0"
ifeq "$(GCC_VERSION)" "1"
	CPPFLAGS += -std=c++11
endif
endif

# -Wl,--verbose
#glib1.0 for json parsing
CPPFLAGS += $(shell pkg-config --cflags json-glib-1.0)
LDFLAGS += $(shell pkg-config --libs json-glib-1.0)
ifeq ($(code_coverage), true)
	COVFLAGS = --coverage
else
	COVFLAGS =
endif

ifndef shell_date
	DFLAG :=-DBUILD_DATE="$(shell date)"
else
	DFLAG :=-DBUILD_DATE="$(shell_date)"
endif

ifndef build
	DFLAG += -DPERFORCE_VERSION=0
else
	DFLAG += -DPERFORCE_VERSION=$(build)
endif

ifdef u50_subbuffer
	DFLAG += -DU50_USE_SUB_BUFFER
endif

.PHONY: output all clean

all: output $(TARGET)

output:
	@echo "MAKEFILE:  INFO: Creating output directory ${OUTPUT_DIR}"
	@mkdir -p ${OUTPUT_DIR}
	@echo "...Done"

clean:
	@echo "MAKEFILE:  INFO: Deleting output directory ${OUTPUT_DIR}"
	@$(RM) -rf $(OUTPUT_DIR)
	@echo "...Done"

%.o:%.cpp
	@echo ""
ifeq "$(REDHAT)" "1"
	scl enable devtoolset-7 '$(CXX) $(CPPFLAGS) $(DFLAG) $(COVFLAGS) -o ${OUTPUT_DIR}/$(notdir $(@)) -c $<'
else
	$(CXX) $(CPPFLAGS) $(DFLAG) $(COVFLAGS) -o ${OUTPUT_DIR}/$(notdir $(@)) -c $<
endif

$(TARGET): $(OBJS)
	@echo ""
ifeq "$(REDHAT)" "1"
	scl enable devtoolset-7 '$(CXX) $(COVFLAGS) -o ${OUTPUT_DIR}/$@ $(addprefix ${OUTPUT_DIR}/,$(notdir $(OBJS))) $(LDFLAGS)'
else
	$(CXX) $(COVFLAGS) -o ${OUTPUT_DIR}/$@ $(addprefix ${OUTPUT_DIR}/,$(notdir $(OBJS))) $(LDFLAGS)
endif
	@echo ""
	@echo "MAKEFILE:  INFO: Target generated ${OUTPUT_DIR}/$@"
	@echo "...Done"
