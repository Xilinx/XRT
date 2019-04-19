all:
	g++ -g -std=c++14 -I${XILINX_XRT}/include mpd.cpp -L${XILINX_XRT}/lib -lxrt_core -ldl -luuid -pthread -o mpd
	g++ -g -std=c++14 -I${XILINX_XRT}/include msd.cpp -L${XILINX_XRT}/lib -lxrt_core -ldl -luuid -pthread -o msd
