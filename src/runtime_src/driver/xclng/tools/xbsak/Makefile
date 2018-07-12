EXENAME := xbsak

CXX_EXT := cpp
CL_EXT := cl

AR := ar
CXX := g++

CXXFLAGS := -Wall -Werror -std=c++11 -pthread

ROOT1 = $(dir $(dir $(CURDIR)))
ROOT = $(ROOT1)..

HAL_INC := -I$(ROOT) -I../../../.. -I../../../include

CXXFLAGS += $(HAL_INC) -I../../xrt/user_gem/

HAL_LIBNAME := $(ROOT)/xrt/user_gem/libxclgemdrv.a

ifeq ($(debug),1)
	CXXFLAGS += -g -D_DEBUG
else
	CXXFLAGS += -O2 -DNDEBUG
endif

SRCS := $(wildcard *.$(CXX_EXT))
OBJS := $(patsubst %.$(CXX_EXT), %.o, $(SRCS))

-include $(OBJS:.o=.d)

LDFLAGS := -static-libstdc++ -pthread

all : $(EXENAME)

%.o: %.$(CXX_EXT)
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) $(MYCXXFLAGS) -c $< -o $@
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) $(MYCXXFLAGS) -c -MM $< -o $(patsubst %.o, %.d, $@)

$(EXENAME): $(OBJS) $(HAL_LIBNAME)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS) $(HAL_LIBNAME) -lrt -lncurses

clean:
	rm -rf *.o *.d $(EXENAME)

.PHONY: all

.DEFAULT_GOAL := all
