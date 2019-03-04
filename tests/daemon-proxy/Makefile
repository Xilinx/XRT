#all:
#	gcc -g -I"../../src/runtime_src/driver/xclng/include/" -I"../../src/runtime_src/driver/include" mproxyd.c -lpthread -o daemon.exe
all:
	g++ -g -std=c++14 -I${XILINX_XRT}/include mproxyd.cpp -L${XILINX_XRT}/lib -lxrt_core -ldl -luuid -pthread -o daemon.exe
