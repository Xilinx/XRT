CC    = g++
CFLAGS = -DDSA64 -g -std=c++11 mssg.cpp -I../../build/Debug/opt/xilinx/xrt/include
LDFLAGS = -L../../build/Debug/opt/xilinx/xrt/lib -lxrt_core -lxilinxopencl -lpthread -lrt
run:
	@$(CC) $(CFLAGS) -o host.exe $(LDFLAGS)

clean:
	@find . -name '*.log' -delete
	@find . -name '*.exe' -delete
	@find ../ -name '*.log' -delete


%:
	@true
