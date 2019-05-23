/*
 * xclHALProxy2.cpp
 *
 *  Created on: Jul 26, 2016
 *      Author: umangp
 */

#include "xclHALProxy2.h"
#include <iostream>

xclHALProxy2::xclHALProxy2() {

  mDriverHandle = dlopen("./libxclzynqdrv.so", RTLD_LAZY);
  if (!mDriverHandle) {
    fprintf(stderr, "%s\n", dlerror());
    exit(1);
  }

  mOpen = (openFuncType) dlsym(mDriverHandle, "xclOpen");
  if (!mOpen) {
    fprintf(stderr, "%s\n", dlerror());
    exit(2);
  }

  mClose = (closeFuncType) dlsym(mDriverHandle, "xclClose");
  if (!mClose) {
    fprintf(stderr, "%s\n", dlerror());
    exit(3);
  }

  mAllocBO = (xclAllocBO) dlsym(mDriverHandle, "xclAllocBO");
  if (!mAllocBO) {
    fprintf(stderr, "%s\n", dlerror());
    exit(4);
  }

  mFreeBO = (xclFreeBO) dlsym(mDriverHandle, "xclFreeBO");
  if (!mFreeBO) {
    fprintf(stderr, "%s\n", dlerror());
    exit(5);
  }

  mMapBO = (xclMapBO) dlsym(mDriverHandle, "xclMapBO");
  if (!mMapBO) {
    fprintf(stderr, "Map error: %s\n", dlerror());
    exit(5);
  }

  mControlWrite = (xclWrite) dlsym(mDriverHandle, "xclWrite");
  if (!mControlWrite) {
    fprintf(stderr, "xclWrite: %s\n", dlerror());
    exit(5);
  }

  mControlRead = (xclRead) dlsym(mDriverHandle, "xclRead");
  if (!mControlRead) {
    fprintf(stderr, "xclRead: %s\n", dlerror());
    exit(5);
  }

  mGetDeviceAddr = (xclGetDeviceAddr) dlsym(mDriverHandle, "xclGetDeviceAddr");
  if (!mGetDeviceAddr) {
    fprintf(stderr, "mGetDeviceAddr: %s\n", dlerror());
    exit(5);
  }



  mDeviceHandle = mOpen(0, "mylog.log", XCL_INFO);
  if (mDeviceHandle == nullptr) {

    fprintf(stderr, "%s\n", dlerror());
    exit(6);
  }

  std::cout << "XCL Open done : Handle: " << mDeviceHandle << std::endl;
}




