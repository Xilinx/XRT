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
#include "../../include/zynq_ioctl.h"

//aarch64-linux-gnu-g++ -g -std=c++11 -I ../../include/ -I ../../../../ zgem1.c -o zocl_kernel_test


int main(int argc, char *argv[])
{
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [freq]\n";
        return 1;
    }

    int fd = open("/dev/dri/renderD128",  O_RDWR);
    if (fd < 0) {
        return -1;
    }

    std::cout << "CREATE" << std::endl;
    drm_zocl_create_bo info1 = {1024, 0xffffffff, DRM_ZOCL_BO_FLAGS_COHERENT | DRM_ZOCL_BO_FLAGS_CMA};
    int result = ioctl(fd, DRM_IOCTL_ZOCL_CREATE_BO, &info1);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info1.handle << std::endl;

    drm_zocl_create_bo info2 = {4200, 0xffffffff, DRM_ZOCL_BO_FLAGS_COHERENT | DRM_ZOCL_BO_FLAGS_CMA};
    result = ioctl(fd, DRM_IOCTL_ZOCL_CREATE_BO, &info2);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info2.handle << std::endl;

    drm_zocl_create_bo info3 = {4200, 0xffffffff, DRM_ZOCL_BO_FLAGS_COHERENT | DRM_ZOCL_BO_FLAGS_CMA};
    result = ioctl(fd, DRM_IOCTL_ZOCL_CREATE_BO, &info3);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info3.handle << std::endl;

    std::cout << "INFO" << std::endl;
    drm_zocl_info_bo infoInfo1 = {info1.handle, 0, 0};
    result = ioctl(fd, DRM_IOCTL_ZOCL_INFO_BO, &infoInfo1);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info1.handle << std::endl;
    std::cout << "Size " << infoInfo1.size << std::endl;
    std::cout << "Physical " << std::hex << infoInfo1.paddr << std::dec << std::endl;

    drm_zocl_info_bo infoInfo2 = {info2.handle, 0, 0};
    result = ioctl(fd, DRM_IOCTL_ZOCL_INFO_BO, &infoInfo2);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info2.handle << std::endl;
    std::cout << "Size " << infoInfo2.size << std::endl;
    std::cout << "Physical " << std::hex << infoInfo2.paddr << std::dec << std::endl;

    drm_zocl_info_bo infoInfo3 = {info3.handle, 0, 0};
    result = ioctl(fd, DRM_IOCTL_ZOCL_INFO_BO, &infoInfo3);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info3.handle << std::endl;
    std::cout << "Size " << infoInfo3.size << std::endl;
    std::cout << "Physical " << std::hex << infoInfo3.paddr << std::dec << std::endl;

    char *bufferA = new char[1024];
    char *bufferB = new char[4200];
    char *bufferC = new char[4200];
    char *bufferD = new char[4200];

    std::cout << "PWRITE" << std::endl;
    std::memset(bufferA, 'a', 1024);
    drm_zocl_pwrite_bo pwriteInfo1 = {info1.handle, 0, 0, 1024, reinterpret_cast<uint64_t>(bufferA)};
    result = ioctl(fd, DRM_IOCTL_ZOCL_PWRITE_BO, &pwriteInfo1);
    std::cout << "result = " << result << std::endl;

    std::memset(bufferB, 'b', 2048);
    drm_zocl_pwrite_bo pwriteInfo2 = {info2.handle, 0, 0, 4200, reinterpret_cast<uint64_t>(bufferB)};
    result = ioctl(fd, DRM_IOCTL_ZOCL_PWRITE_BO, &pwriteInfo2);
    std::cout << "result = " << result << std::endl;

    std::memset(bufferC, 'c', 2048);
    drm_zocl_pwrite_bo pwriteInfo3 = {info3.handle, 0, 0, 4200, reinterpret_cast<uint64_t>(bufferC)};
    result = ioctl(fd, DRM_IOCTL_ZOCL_PWRITE_BO, &pwriteInfo3);
    std::cout << "result = " << result << std::endl;

    std::cout << "PREAD/COMPARE" << std::endl;
    drm_zocl_pread_bo preadInfo1 = {info1.handle, 0, 0, 1024, reinterpret_cast<uint64_t>(bufferD)};
    result = ioctl(fd, DRM_IOCTL_ZOCL_PREAD_BO, &preadInfo1);
    std::cout << "result = " << result << std::endl;
    result = std::memcmp(bufferA, bufferD, 1024);
    std::cout << "result = " << result << std::endl;

    drm_zocl_pread_bo preadInfo2 = {info2.handle, 0, 0, 4200, reinterpret_cast<uint64_t>(bufferD)};
    result = ioctl(fd, DRM_IOCTL_ZOCL_PREAD_BO, &preadInfo2);
    std::cout << "result = " << result << std::endl;
    result = std::memcmp(bufferB, bufferD, 4200);
    std::cout << "result = " << result << std::endl;

    drm_zocl_pread_bo preadInfo3 = {info3.handle, 0, 0, 4200, reinterpret_cast<uint64_t>(bufferD)};
    result = ioctl(fd, DRM_IOCTL_ZOCL_PREAD_BO, &preadInfo3);
    std::cout << "result = " << result << std::endl;
    result = std::memcmp(bufferC, bufferD, 4200);
    std::cout << "result = " << result << std::endl;

    std::cout << "MMAP" << std::endl;
    drm_zocl_map_bo mapInfo1 = {info1.handle, 0, 0};
    result = ioctl(fd, DRM_IOCTL_ZOCL_MAP_BO, &mapInfo1);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info1.handle << std::endl;
    void *ptr1 = mmap(0, info1.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mapInfo1.offset);
    std::cout << "Offset "  << std::hex << mapInfo1.offset << std::dec << std::endl;
    std::cout << "Pointer " << ptr1 << std::endl;

    drm_zocl_map_bo mapInfo2 = {info2.handle, 0, 0};
    result = ioctl(fd, DRM_IOCTL_ZOCL_MAP_BO, &mapInfo2);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info2.handle << std::endl;
    void *ptr2 = mmap(0, info2.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mapInfo2.offset);
    std::cout << "Offset "  << std::hex << mapInfo2.offset << std::dec << std::endl;
    std::cout << "Pointer " << ptr2 << std::endl;

    drm_zocl_map_bo mapInfo3 = {info3.handle, 0, 0};
    result = ioctl(fd, DRM_IOCTL_ZOCL_MAP_BO, &mapInfo3);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info3.handle << std::endl;
    void *ptr3 = mmap(0, info3.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mapInfo3.offset);
    std::cout << "Offset "  << std::hex << mapInfo3.offset << std::dec << std::endl;
    std::cout << "Pointer " << ptr3 << std::endl;

    std::cout << "MMAP/COMPARE" << std::endl;
    result = std::memcmp(bufferA, ptr1, 1024);
    std::cout << "result = " << result << std::endl;

    result = std::memcmp(bufferB, ptr2, 4200);
    std::cout << "result = " << result << std::endl;

    result = std::memcmp(bufferC, ptr3, 4200);
    std::cout << "result = " << result << std::endl;

    std::cout << "MMAP/UPDATE" << std::endl;

    std::memset(ptr1, 'p', 1024);
    std::memset(ptr2, 'q', 4200);
    std::memset(ptr3, 'r', 4200);

    std::memset(bufferA, 'p', 1024);
    std::memset(bufferB, 'q', 4200);
    std::memset(bufferC, 'r', 4200);

    std::cout << "MUNMAP" << std::endl;
    result = munmap(ptr1, 1024);
    std::cout << "result = " << result << std::endl;

    result = munmap(ptr2, 4200);
    std::cout << "result = " << result << std::endl;

    result = munmap(ptr3, 4200);
    std::cout << "result = " << result << std::endl;

    std::cout << "PREAD/COMPARE" << std::endl;
    drm_zocl_pread_bo preadInfo11 = {info1.handle, 0, 0, 1024, reinterpret_cast<uint64_t>(bufferD)};
    result = ioctl(fd, DRM_IOCTL_ZOCL_PREAD_BO, &preadInfo11);
    std::cout << "result = " << result << std::endl;
    result = std::memcmp(bufferA, bufferD, 1024);
    std::cout << "result = " << result << std::endl;

    drm_zocl_pread_bo preadInfo22 = {info2.handle, 0, 0, 4200, reinterpret_cast<uint64_t>(bufferD)};
    result = ioctl(fd, DRM_IOCTL_ZOCL_PREAD_BO, &preadInfo22);
    std::cout << "result = " << result << std::endl;
    result = std::memcmp(bufferB, bufferD, 4200);
    std::cout << "result = " << result << std::endl;

    drm_zocl_pread_bo preadInfo33 = {info3.handle, 0, 0, 4200, reinterpret_cast<uint64_t>(bufferD)};
    result = ioctl(fd, DRM_IOCTL_ZOCL_PREAD_BO, &preadInfo33);
    std::cout << "result = " << result << std::endl;
    result = std::memcmp(bufferC, bufferD, 4200);
    std::cout << "result = " << result << std::endl;

    delete [] bufferA;
    delete [] bufferB;
    delete [] bufferC;
    delete [] bufferD;

    std::cout << "CLOSE" << std::endl;
    drm_gem_close closeInfo = {info1.handle, 0};
    result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
    std::cout << "result = " << result << std::endl;

    closeInfo.handle = info2.handle;
    result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
    std::cout << "result = " << result << std::endl;

    closeInfo.handle = info3.handle;
    result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
    std::cout << "result = " << result << std::endl;

    result = close(fd);
    std::cout << "result = " << result << std::endl;

    return result;
}
