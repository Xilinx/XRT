CC    = g++

xrt_install_path := "/opt/xilinx/xrt"
ifdef XRT_INSTALL_PATH
	xrt_install_path := ${XRT_INSTALL_PATH}
endif

CFLAGS = -DDSA64 -g -std=c++11 mssg.cpp -I../../build/Debug${xrt_install_path}/include
LDFLAGS = -L../../build/Debug${xrt_install_path}/lib -lxrt_core -lpthread -lrt
run:
	@$(CC) $(CFLAGS) -o host.exe $(LDFLAGS)

clean:
	@find . -name '*.log' -delete
	@find . -name '*.exe' -delete
	@find ../ -name '*.log' -delete


%:
	@true
