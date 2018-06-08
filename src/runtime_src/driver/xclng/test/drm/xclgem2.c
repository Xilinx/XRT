#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <cassert>
#include "drm/drm.h"
#include "xocl_ioctl.h"

#define BUFF_SIZE 5*1024*1024
#define BUFF_NUM 1024
//arm-linux-gnueabihf-g++ -g -std=c++11 -I /home/sonals/git/drm/include -I ../drm/zgem zgem1.c
//g++ -g -std=c++11 -I /home/sonals/git/drm/include -I ../drm/xocl xclgem1.c

class Timer {
	std::chrono::high_resolution_clock::time_point mTimeStart;
	public:
	Timer() {
		reset();
	}
	double stop() {
		std::chrono::high_resolution_clock::time_point timeEnd = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - mTimeStart).count();
	}
	void reset() {
		mTimeStart = std::chrono::high_resolution_clock::now();
	}
};

static int openDevice(const char *name)
{
	std::string dev("/dev/dri/renderD");
	drm_version version;
	std::memset(&version, 0, sizeof(version));
	version.name = new char[128];
	version.name_len = 128;
	version.desc = new char[512];
	version.desc_len = 512;
	version.date = new char[128];
	version.date_len = 128;

	for (int i = 128; ;i++) {
		std::string devName = dev;
		devName += std::to_string(i);
		int fd = open(devName.c_str(),  O_RDWR);
		if (fd < 0) {
			return fd;
		}

		int result = ioctl(fd, DRM_IOCTL_VERSION, &version);
		if (result < 0)
			continue;
		if (!std::strstr(version.name, name))
			continue;
		std::cout << version.name << "\n";
		std::cout << version.version_major << '.' << version.version_minor << '.' << version.version_patchlevel << "\n";
		std::cout << version.desc << "\n";
		return fd;
	}
}


int main(int argc, char *argv[])
{
	const char *dev = "xocl";
	unsigned kind = 0;
	if (argc > 2) {
		std::cerr << "Usage: " << argv[0] << " [xocl]\n";
		return 1;
	}

	if (argc == 2) {
		dev = argv[1];
		if (std::strcmp(dev, "xocl")) {
			std::cerr << "Usage: " << argv[0] << " [zocl]\n";
			return 1;
		}
	}

	int fd = openDevice(dev);

	if (fd < 0) {
		return -1;
	}

	drm_version version;
	std::memset(&version, 0, sizeof(version));
	version.name = new char[128];
	version.name_len = 128;
	version.desc = new char[512];
	version.desc_len = 512;
	version.date = new char[128];
	version.date_len = 128;

	int result = ioctl(fd, DRM_IOCTL_VERSION, &version);
	std::cout << version.name << "\n";
	std::cout << version.version_major << '.' << version.version_minor << '.' << version.version_patchlevel << "\n";
	std::cout << version.desc << "\n";

	std::cout << "CREATE" << std::endl;
	drm_xocl_create_bo infoarr[BUFF_NUM];
	Timer timer;
	for (int i =0; i < BUFF_NUM ; i++){
		infoarr[i] = {BUFF_SIZE, 0xffffffff, 0};
		result = ioctl(fd, DRM_IOCTL_XOCL_CREATE_BO, &infoarr[i]);
		if (result < 0) std::cout << "Error in result = " << result << std::endl;
		//std::cout << "Handle " << infoarr[i].handle << std::endl;
	}
	double timer_stop;
	timer_stop = timer.stop();
	timer_stop/= BUFF_NUM;
	std::cout << "Buffer creation time: " << timer_stop << " usec" <<" for buffer size: "<<BUFF_SIZE << std::endl;


	char *bufferA = new char[BUFF_SIZE];
	char *bufferCheck = new char[BUFF_SIZE];

	std::cout << "PWRITE" << std::endl;
	std::cout << "BO1" << std::endl;
	std::memset(bufferA, 'a', BUFF_SIZE);
	drm_xocl_pwrite_bo pwriteInfoarr[BUFF_NUM];
	for (int i =0; i < BUFF_NUM ; i++){
		pwriteInfoarr[i] = {infoarr[i].handle, 0, 0, BUFF_SIZE, reinterpret_cast<uint64_t>(bufferA)};
		result = ioctl(fd, DRM_IOCTL_XOCL_PWRITE_BO, &pwriteInfoarr[i]);
		if (result < 0) std::cout << "Error in result = " << result << std::endl;
	}
	std::cout << "PREAD/COMPARE" << std::endl;
	std::cout << "BO1" << std::endl;
	drm_xocl_pread_bo preadInfoarr[BUFF_NUM];
	for (int i =0; i < BUFF_NUM ; i++){

		preadInfoarr[i]= {infoarr[i].handle, 0, 0, BUFF_SIZE, reinterpret_cast<uint64_t>(bufferCheck)};
		result = ioctl(fd, DRM_IOCTL_XOCL_PREAD_BO, &preadInfoarr[i]);
		if (result < 0) std::cout << "Error in result = " << result << std::endl;
		result = std::memcmp(bufferA, bufferCheck, BUFF_SIZE);
		if (result < 0) std::cout << "Error in result = " << result << std::endl;
	}

	std::cout << "MMAP" << std::endl;
	std::cout << "BO1" << std::endl;
	drm_xocl_map_bo mapInfoarr[BUFF_NUM];
	void *ptr[BUFF_NUM];
	double timer_map;
	timer.reset();
	for (int i =0; i < BUFF_NUM ; i++){
		mapInfoarr[i]= {infoarr[i].handle, 0, 0};
		result = ioctl(fd, DRM_IOCTL_XOCL_MAP_BO, &mapInfoarr[i]);
		if (result < 0) std::cout << "Error in result = " << result << std::endl;		
		//std::cout << "Handle " << info1.handle << std::endl;
		ptr[i] = mmap(0, infoarr[i].size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mapInfoarr[i].offset);
		//std::cout << "Offset "  << std::hex << mapInfo1.offset << std::dec << std::endl;
		//std::cout << "Pointer " << ptr1 << std::endl;
	} 
	timer_map = timer.stop();
	timer_map/= BUFF_NUM;
	std::cout << "Mapping time: " << timer_map << " usec" << "\n\n";

	std::cout << "MMAP/COMPARE" << std::endl;
	std::cout << "BO1" << std::endl;
	for (int i =0; i < BUFF_NUM ; i++){
		result = std::memcmp(bufferA, ptr[i], BUFF_SIZE);
		if (result < 0) std::cout << "Error in result = " << result << std::endl;
	}
	std::cout << "MMAP/UPDATE" << std::endl;
	for (int i =0; i < BUFF_NUM ; i++){
		std::memset(ptr[i], 'p', BUFF_SIZE);
	}
	//std::memset(bufferA, 'p', BUFF_SIZE);
	std::cout << "MUNMAP" << std::endl;
	std::cout << "BO1" << std::endl;
	double timer_munmap;
	timer.reset();
	for (int i =0; i < BUFF_NUM ; i++){
		result = munmap(ptr[i], BUFF_SIZE);
		if (result < 0) std::cout << "Error in result = " << result << std::endl;
	}
	timer_munmap = timer.stop();
	timer_munmap/= BUFF_NUM;
	std::cout << "Un-Mapping time: " << timer_munmap << " usec" << "\n\n";

	delete [] bufferA;
	delete [] bufferCheck;

	std::cout << "CLOSE" << std::endl;
	std::cout << "BO1" << std::endl;
	drm_gem_close closeInfoarr[BUFF_NUM];
	double timer_end;
	timer.reset();
	for (int i =0; i < BUFF_NUM ; i++){
		closeInfoarr[i] = {infoarr[i].handle, 0};
		result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfoarr[i]);
		if (result < 0) std::cout << "Error in result = " << result << std::endl;
	}
	timer_end = timer.stop();
	timer_end/= BUFF_NUM;
	std::cout << "Buffer release time: " << timer_end << " usec" <<" for buffer size: "<<BUFF_SIZE << "\n\n";

	result = close(fd);
	if (result < 0) std::cout << "TEST FAILED" << result << std::endl;
	std::cout<<"TEST PASSED"<<std::endl;
	return result;
}


