CC    = g++

xrt_install_prefix := "/opt/xilinx"
ifdef XRT_INSTALL_PREFIX
	xrt_install_prefix := ${XRT_INSTALL_PREFIX}
endif

CFLAGS = -DDSA64 -g -std=c++11 mssg.cpp -I../../build/Debug${xrt_install_prefix}/xrt/include
LDFLAGS = -L../../build/Debug${xrt_install_prefix}/xrt/lib -lxrt_core -lpthread -lrt
run:
	@$(CC) $(CFLAGS) -o host.exe $(LDFLAGS)

clean:
	@find . -name '*.log' -delete
	@find . -name '*.exe' -delete
	@find ../ -name '*.log' -delete


%:
	@true
