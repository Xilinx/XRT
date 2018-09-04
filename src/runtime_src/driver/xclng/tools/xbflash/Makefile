EXENAME := xbflash

CXX_EXT := cpp

CXX := g++

CXXFLAGS := -g -Wall -std=c++11 -pthread
RT_SRC_ROOT := ../../../..

# External Dependencies
EXT_DEP_DIR := $(RT_SRC_ROOT)/driver/xclng/xrt/user_gem

HAL_INC := -I$(RT_SRC_ROOT) -I../../../..

CXXFLAGS += $(HAL_INC) -I$(RT_SRC_ROOT)/driver/include/ -I$(RT_SRC_ROOT)/driver/xclng/include -I$(EXT_DEP_DIR)

HAL_LIBNAME := $(RT_SRC_ROOT)/driver/xclng/xrt/user_gem/libxclgemdrv.a

ifeq ($(debug),1)
	CXXFLAGS += -g -D_DEBUG
else
	CXXFLAGS += -O2 -DNDEBUG
endif

SRCS := $(wildcard *.$(CXX_EXT))
OBJS := $(patsubst %.$(CXX_EXT), %.o, $(SRCS))
# OBJS += $(EXT_DEP_DIR)/$(EXT_DEP_OBJ).o

-include $(OBJS:.o=.d)

LDFLAGS := -static-libstdc++ -pthread

all : $(EXENAME)

%.o: %.$(CXX_EXT)
	$(CXX) $(CXXFLAGS) -c $< -o $@
	$(CXX) $(CXXFLAGS) -c -MM $< -o $(patsubst %.o, %.d, $@)

$(EXENAME): $(OBJS) 
	$(CXX) -o $@ $(OBJS) $(LDFLAGS) $(HAL_LIBNAME) -lrt

clean:
	rm -rf *.o *.d $(EXENAME) 

.PHONY: all

.DEFAULT_GOAL := all
