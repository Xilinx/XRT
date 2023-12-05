CC    = g++

xrt_install_dir := "/opt/xilinx/xrt"
ifdef XRT_INSTALL_DIR
	xrt_install_dir := ${XRT_INSTALL_DIR}
endif

CFLAGS = -DDSA64 -g -std=c++11 mssg.cpp -I../../build/Debug${xrt_install_dir}/include
LDFLAGS = -L../../build/Debug${xrt_install_dir}/lib -lxrt_core -lpthread -lrt
run:
	@$(CC) $(CFLAGS) -o host.exe $(LDFLAGS)

clean:
	@find . -name '*.log' -delete
	@find . -name '*.exe' -delete
	@find ../ -name '*.log' -delete


%:
	@true
