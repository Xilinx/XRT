/*
 * xclHALProxy2.h
 *
 *  Created on: Jul 26, 2016
 *      Author: umangp
 */

#ifndef _XCLHALPROXY2_H_
#define _XCLHALPROXY2_H_

#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include "core/include/xclhal2.h"
#include "core/edge/include/zynq_ioctl.h"

class xclHALProxy2 {
public:
  xclHALProxy2();
  virtual ~xclHALProxy2(){
      mClose(mDeviceHandle);
      dlclose(mDriverHandle);
  }
  unsigned int allocate_bo(size_t size, int unused, unsigned flags) {

    std::cout << "Inside Allocate BO: " << mDeviceHandle <<  std::endl;
    std::cout << "Size: " << size <<  std::endl;
    std::cout << "Fpags: " << flags <<  std::endl;
    return mAllocBO(mDeviceHandle, size, unused, flags);
  }

  void* map_bo(unsigned int boHandle, bool write) {
    return mMapBO(mDeviceHandle, boHandle, write );
  }

  uint64_t get_physical_addr(unsigned int boHandle) {
    return mGetDeviceAddr(mDeviceHandle, boHandle);
  }

  void free_bo(unsigned int boHandle) {
    return mFreeBO(mDeviceHandle, boHandle);
  }

  void write_control_reg(uint64_t offset, const void *hostBuf, size_t size) {
    mControlWrite(mDeviceHandle, XCL_ADDR_KERNEL_CTRL, offset, hostBuf, size );
  }

  void read_control_reg(uint64_t offset, void *hostBuf, size_t size) {
    mControlRead(mDeviceHandle, XCL_ADDR_KERNEL_CTRL, offset, hostBuf, size );
  }

  bool is_ready(unsigned offset = 0) {

    uint32_t ctrl_reg;
    mControlRead(mDeviceHandle, XCL_ADDR_KERNEL_CTRL, offset, &ctrl_reg, 1 );
    return !((ctrl_reg >> 0) & 0x1);
  }

  bool is_done(unsigned offset = 0 ) {
    uint32_t ctrl_reg;
    mControlRead(mDeviceHandle, XCL_ADDR_KERNEL_CTRL, offset, &ctrl_reg, 1 );
    return ((ctrl_reg >> 1) & 0x1);
  }

  bool is_idle(unsigned offset = 0) {
    uint32_t ctrl_reg;
    mControlRead(mDeviceHandle, XCL_ADDR_KERNEL_CTRL, offset, &ctrl_reg, 1 );
    return ((ctrl_reg >> 2) & 0x1);
  }

  void start_kernel(unsigned offset = 0) {

    uint32_t ctrl_reg;
    mControlRead(mDeviceHandle, XCL_ADDR_KERNEL_CTRL, offset, &ctrl_reg, 1 );
    ctrl_reg |= 0x01;

    mControlWrite(mDeviceHandle, XCL_ADDR_KERNEL_CTRL, offset, &ctrl_reg, 1 );
  }

  void print_kernel_status(){

    uint32_t isDone, isIdle, isReady;
    isDone = is_done();
    isIdle = is_idle();
    isReady = is_ready();
    printf("---current kernel status done:%d, idle:%d, Ready:%d ---\n\r", isDone, isIdle, isReady);

  }


private:

  xclDeviceHandle mDeviceHandle;
  void *mDriverHandle;
  typedef unsigned int (*xclAllocBO)(xclDeviceHandle handle, size_t size, int unused, unsigned flags);
  typedef void (*xclFreeBO)(xclDeviceHandle handle, unsigned int boHandle);
  typedef void * (*xclMapBO)(xclDeviceHandle handle, unsigned int boHandle, bool write);
  typedef uint64_t (*xclGetDeviceAddr)(xclDeviceHandle handle, unsigned int boHandle);

  typedef size_t (*xclWrite)(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset,
      const void *hostBuf, size_t size);

  typedef size_t (*xclRead)(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset,
      void *hostbuf, size_t size);
  typedef xclDeviceHandle (*openFuncType)(unsigned deviceIndex, const char *logFileName,
      xclVerbosityLevel level);
  typedef void (*closeFuncType)(xclDeviceHandle handle);
//  typedef unsigned (*probeFuncType)();



  openFuncType mOpen;
  closeFuncType mClose;
  xclAllocBO mAllocBO;
  xclFreeBO mFreeBO;
  xclMapBO mMapBO;
  xclWrite mControlWrite;
  xclRead mControlRead;
  xclGetDeviceAddr mGetDeviceAddr;


};


#endif /* _XCLHALPROXY2_H_ */
