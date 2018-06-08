#include <stdio.h>
#include <dlfcn.h>
//#include "ctest.h"
#include <stdlib.h>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <chrono>
#include <unistd.h>
#include <time.h>
#include "driver/include/xclhal2.h"

#include "xclHALProxy2.h"



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

// aarch64-linux-gnu-g++ -g -std=c++11 zynq_user_test_add1.cpp -I /proj/rdi-xsj/staff/umangp/work1/HEAD/src/products/sdx/ocl/src/runtime_src -I ../../kernel2 -ldl -o usertest


//
//bool is_ready(xclDeviceHandle dHandle) {
//
//  uint32_t ctr_reg;
//  size_t temp = xclRead_o( XCL_ADDR_KERNEL_CTRL, XADDONE_CONTROL_ADDR_AP_CTRL, (void * ) ctrl_reg, 1 );
//  return !((*(addptr + XADDONE_CONTROL_ADDR_AP_CTRL) >> 0) & 0x1);
//}
//
//bool is_done(xclDeviceHandle dHandle) {
//  return ((*(addptr + XADDONE_CONTROL_ADDR_AP_CTRL) >> 1) & 0x1);
//}
//
//bool is_idle(xclDeviceHandle dHandle) {
//  return ((*(addptr + XADDONE_CONTROL_ADDR_AP_CTRL) >> 2) & 0x1);
//}
//
//void start_kernel(uint32_t *addptr) {
//  *(addptr + XADDONE_CONTROL_ADDR_AP_CTRL) |= 0x1;
//}
//
//void print_kernel_status(uint32_t *add1ptr){
//
//  uint32_t isDone, isIdle, isReady;
//  isDone = is_done(add1ptr);
//  isIdle = is_idle(add1ptr);
//  isReady = is_ready(add1ptr);
//  printf("---current kernel status done:%d, idle:%d, Ready:%d ---\n\r", isDone, isIdle, isReady);
//
//}


int main(int argc, char **argv) {
  xclHALProxy2 xclHALProxy2_o;

  std::cout << "Before Allocate BO" << std::endl;

  unsigned int boHandle1 = xclHALProxy2_o.allocate_bo( 1024 * 1024 * 4, XCL_BO_SHARED_PHYSICAL, 3);
  printf("BO Open Handle=%d\n", boHandle1);

  if (boHandle1 < 0) {
    std::cout << "Allocate BO 1 failed" << std::endl;
    exit(7);
  }

  unsigned int boHandle2 = xclHALProxy2_o.allocate_bo( 1024 * 1024 * 4, XCL_BO_SHARED_PHYSICAL, 3);
  printf("BO Open Handle=%d\n", boHandle2);


  if (boHandle2 < 0) {
    std::cout << "Allocate BO 2 failed" << std::endl;
    exit(7);
  }

  uint32_t *ptr1 = (uint32_t*) xclHALProxy2_o.map_bo( boHandle1, true);
  if (!ptr1) {
    std::cout << "Map BO  failed" << std::endl;
    exit(8);
  }
  uint32_t *ptr2 = (uint32_t*) xclHALProxy2_o.map_bo( boHandle2, true);
  if (!ptr2) {
    std::cout << "Map BO  failed" << std::endl;
    exit(8);
  }
  std::cout << "PWRITE ptr1: "<< std::hex  << memset(ptr1, 'd', 1024*1024*4) << std::dec << std::endl;
  std::cout << "PWRITE ptr2: "<< std::hex  << memset(ptr2,  0 , 1024*1024*4) << std::dec << std::endl;



  std::cout << "============================================================" << std::endl;
  printf("====Printing 100 elements of a---\n\r");
  uint32_t *p1 = (uint32_t*) ptr1;
  for (int i = 0; i < 100; ++i) {
    printf("Mem addr: 0x%x, Data: 0x%x\n\r", p1 + i, p1[i]);
  }
  printf("====Printing 100 elements of b---\n\r");
  p1 = (uint32_t*) ptr2;
  for (int i = 0; i < 100; ++i) {
    printf("Mem addr: 0x%x, Data: 0x%x\n\r", p1 + i, p1[i]);
  }

  std::cout << "============================================================" << std::endl;

  uint32_t ctrl_regs[10];
  xclHALProxy2_o.read_control_reg( 0, (void * ) ctrl_regs, 9);
  for (int i = 0; i < 9; ++i) {
    std::cout << "Reg: " << i << " : Value : " << ctrl_regs[i] << std::endl;
  }
  std::cout << "============================================================" << std::endl;

  xclHALProxy2_o.print_kernel_status();

  uint32_t data = 1024*1024;
  xclHALProxy2_o.write_control_reg(XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4, &data, 1 );

  uint32_t read_data = 0;
  xclHALProxy2_o.read_control_reg(XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4, &read_data, 1 );
  std::cout << "Verifying num of elements written : " << std::hex << read_data  << std::dec << std::endl;


  data = (uint32_t) xclHALProxy2_o.get_physical_addr(boHandle1);
  xclHALProxy2_o.write_control_reg(XADDONE_CONTROL_ADDR_A_DATA/4, &data, 1 );

  read_data = 0;
  xclHALProxy2_o.read_control_reg(XADDONE_CONTROL_ADDR_A_DATA/4, &read_data, 1 );
  std::cout << "Verifying num of elements written : " << std::hex << read_data  << std::dec << std::endl;

  data = (uint32_t) xclHALProxy2_o.get_physical_addr(boHandle2);
  xclHALProxy2_o.write_control_reg(XADDONE_CONTROL_ADDR_B_DATA/4, &data, 1 );

  read_data = 0;
  xclHALProxy2_o.read_control_reg(XADDONE_CONTROL_ADDR_B_DATA/4, &read_data, 1 );
  std::cout << "Verifying num of elements written : " << std::hex << read_data  << std::dec << std::endl;

  printf (">>>>Now starting kernel...\n\r");

  xclHALProxy2_o.start_kernel();
  uint32_t isDone, isIdle, isReady;

  while (1){
    isDone = xclHALProxy2_o.is_done();
    isIdle = xclHALProxy2_o.is_idle();
    isReady = xclHALProxy2_o.is_ready();
    printf("---current kernel status done:%d, idle:%d, Ready:%d ---\n\r", isDone, isIdle, isReady);
    if (isDone && isIdle) {

      printf("Exiting while 1 loop ---\n\r");
      break;
    }
    usleep(10000);

  }
  printf("====Quit test built-in kernel---\n\r");
  printf("====Printing 100 elements of a---\n\r");
  p1 = (uint32_t*) ptr1;
  for (int i = 0; i < 100; ++i) {
    printf("Mem addr: 0x%x, Data: 0x%x\n\r", p1 + i, p1[i]);
  }
  printf("====Printing 100 elements of b---\n\r");
  p1 = (uint32_t*) ptr2;
  for (int i = 0; i < 100; ++i) {
    printf("Mem addr: 0x%x, Data: 0x%x\n\r", p1 + i, p1[i]);
  }

  xclHALProxy2_o.print_kernel_status();

  std::cout << "============================================================" << std::endl;




  xclHALProxy2_o.free_bo( boHandle1);
  xclHALProxy2_o.free_bo( boHandle2);
  std::cout << "Free done" << std::endl;


  return 0;
}
