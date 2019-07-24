#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <time.h>
#include <unistd.h>
#include "../../include/zynq_ioctl.h"


#define XADDONE_CONTROL_ADDR_AP_CTRL       0x00
#define XADDONE_CONTROL_ADDR_GIE           0x04
#define XADDONE_CONTROL_ADDR_IER           0x08
#define XADDONE_CONTROL_ADDR_ISR           0x0c
#define XADDONE_CONTROL_ADDR_A_DATA        0x10
#define XADDONE_CONTROL_BITS_A_DATA        32
#define XADDONE_CONTROL_ADDR_B_DATA        0x18
#define XADDONE_CONTROL_BITS_B_DATA        32
#define XADDONE_CONTROL_ADDR_ELEMENTS_DATA 0x20
#define XADDONE_CONTROL_BITS_ELEMENTS_DATA 32

//aarch64-linux-gnu-g++ -g -std=c++11 -I ../../include/ -I ../../../../ add1_zgem.cpp -o add1_zocl_test


bool is_ready(uint32_t *addptr) {
  return !((*(addptr + XADDONE_CONTROL_ADDR_AP_CTRL) >> 0) & 0x1);
}

bool is_done(uint32_t *addptr) {
  return ((*(addptr + XADDONE_CONTROL_ADDR_AP_CTRL) >> 1) & 0x1);
}

bool is_idle(uint32_t *addptr) {
  return ((*(addptr + XADDONE_CONTROL_ADDR_AP_CTRL) >> 2) & 0x1);
}

void start_kernel(uint32_t *addptr) {
  *(addptr + XADDONE_CONTROL_ADDR_AP_CTRL) |= 0x1;
}

void print_kernel_status(uint32_t *add1ptr){

  uint32_t isDone, isIdle, isReady;
  isDone = is_done(add1ptr);
  isIdle = is_idle(add1ptr);
  isReady = is_ready(add1ptr);
  printf("---current kernel status done:%d, idle:%d, Ready:%d ---\n\r", isDone, isIdle, isReady);

}

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

    std::cout << "============================================================" << std::endl;
    std::cout << "CREATE" << std::endl;
    drm_zocl_create_bo info1 = {1024*1024*4, 0xffffffff, DRM_ZOCL_BO_FLAGS_COHERENT | DRM_ZOCL_BO_FLAGS_CMA};
    int result = ioctl(fd, DRM_IOCTL_ZOCL_CREATE_BO, &info1);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info1.handle << std::endl;

    drm_zocl_create_bo info2 = {1024*1024*4, 0xffffffff, DRM_ZOCL_BO_FLAGS_COHERENT | DRM_ZOCL_BO_FLAGS_CMA};
    result = ioctl(fd, DRM_IOCTL_ZOCL_CREATE_BO, &info2);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info2.handle << std::endl;


    std::cout << "============================================================" << std::endl;
    std::cout << "INFO" << std::endl;
    drm_zocl_info_bo infoInfo1 = {info1.handle, 0, 0};
    result = ioctl(fd, DRM_IOCTL_ZOCL_INFO_BO, &infoInfo1);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info1.handle << std::endl;
    std::cout << "Size " << infoInfo1.size << std::endl;
    std::cout << "Physical " << std::hex << infoInfo1.paddr << std::dec << std::endl;

    std::cout << "============================================================" << std::endl;
    drm_zocl_info_bo infoInfo2 = {info2.handle, 0, 0};
    result = ioctl(fd, DRM_IOCTL_ZOCL_INFO_BO, &infoInfo2);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info2.handle << std::endl;
    std::cout << "Size " << infoInfo2.size << std::endl;
    std::cout << "Physical " << std::hex << infoInfo2.paddr << std::dec << std::endl;

    std::cout << "============================================================" << std::endl;
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

    std::cout << "============================================================" << std::endl;
    std::cout << "PWRITE ptr1: "<< std::hex  << std::memset(ptr1, 'd', 1024*1024*4) << std::dec << std::endl;
    std::cout << "PWRITE ptr2: "<< std::hex  << std::memset(ptr2,  0 , 1024*1024*4) << std::dec << std::endl;


    printf("====Printing 40 elements of a---\n\r");
    uint32_t *p1 = (uint32_t*) ptr1;
    for (int i = 0; i < 40; ++i) {
      printf("Mem addr: 0x%x, Data: 0x%x\n\r", p1 + i, p1[i]);
    }
    printf("====Printing 40 elements of b---\n\r");
    p1 = (uint32_t*) ptr2;
    for (int i = 0; i < 40; ++i) {
      printf("Mem addr: 0x%x, Data: 0x%x\n\r", p1 + i, p1[i]);
    }

    std::cout << "============================================================" << std::endl;
    std::cout << "Compute Unit Status: " << std::endl;
    uint32_t *add1ptr = (uint32_t*)mmap(0, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("Compute Unit addr: %p\n", add1ptr);

    //Print the registers again before starting the kernel
    for (int i = 0; i < 9; ++i) {
      printf("Addr: %p, Data 0x%x\n", add1ptr + i, (uint32_t)(*(add1ptr + i)));
    }
    std::cout << "=================Writing values===========================================" << std::endl;
    try {
      //Set the address of a
      *(add1ptr + (XADDONE_CONTROL_ADDR_A_DATA/4) ) = infoInfo1.paddr;

      //Set the address of b
      *(add1ptr + (XADDONE_CONTROL_ADDR_B_DATA/4) ) = infoInfo2.paddr;

      //Set #of elements
      *(add1ptr + (XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4) ) = 1024*1024;

      std::cout << "=================Reading back values===========================================" << std::endl;

      unsigned i = XADDONE_CONTROL_ADDR_A_DATA/4;
      printf("Addr: %p, Data 0x%x\n", add1ptr + i, (uint32_t)(*(add1ptr + i)));
      i = XADDONE_CONTROL_ADDR_B_DATA/4;
      printf("Addr: %p, Data 0x%x\n", add1ptr + i, (uint32_t)(*(add1ptr + i)));
      i = XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4;
      printf("Addr: %p, Data 0x%x\n", add1ptr + i, (uint32_t)(*(add1ptr + i)));

      uint32_t isDone, isIdle, isReady;
      isDone = is_done(add1ptr);
      isIdle = is_idle(add1ptr);
      isReady = is_ready(add1ptr);
      printf("---current kernel status done:%d, idle:%d, Ready:%d ---\n\r", isDone, isIdle, isReady);

      printf (">>>>Now starting kernel...\n\r");

      start_kernel(add1ptr );


      while (1){
        isDone = is_done(add1ptr);
        isIdle = is_idle(add1ptr);
        isReady = is_ready(add1ptr);
        printf("---current kernel status done:%d, idle:%d, Ready:%d ---\n\r", isDone, isIdle, isReady);
        if (isDone && isIdle) {

          printf("Exiting while 1 loop ---\n\r");
          break;
        }
        usleep(100);

      }
      printf("====Quit test built-in kernel---\n\r");
      printf("====Printing 40 elements of a---\n\r");
      p1 = (uint32_t*) ptr1;
      for (int i = 0; i < 40; ++i) {
        printf("Mem addr: 0x%x, Data: 0x%x\n\r", p1 + i, p1[i]);
      }
      printf("====Printing 40 elements of b---\n\r");
      p1 = (uint32_t*) ptr2;
      for (int i = 0; i < 40; ++i) {
        printf("Mem addr: 0x%x, Data: 0x%x\n\r", p1 + i, p1[i]);
      }

      print_kernel_status(add1ptr);

      std::cout << "============================================================" << std::endl;
    }
    catch (...) {

      std::cout << "CLOSE" << std::endl;
      drm_gem_close closeInfo = {info1.handle, 0};
      result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
      std::cout << "result = " << result << std::endl;

      closeInfo.handle = info2.handle;
      result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
      std::cout << "result = " << result << std::endl;

      result = close(fd);
      std::cout << "result = " << result << std::endl;
      return result;
    }


    std::cout << "CLOSE" << std::endl;
    drm_gem_close closeInfo = {info1.handle, 0};
    result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
    std::cout << "result = " << result << std::endl;

    closeInfo.handle = info2.handle;
    result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
    std::cout << "result = " << result << std::endl;

    result = close(fd);
    std::cout << "result = " << result << std::endl;

    return result;
}
