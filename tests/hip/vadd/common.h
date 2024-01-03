// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc.

#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "hip/hip_runtime_api.h"

class
test_hip_error : public std::system_error
{
private:
  static std::string
  message(hipError_t ec, const std::string& what) {
    std::string str = what;
    str += ": ";
    str += hipGetErrorString(ec);
    str += " (";
    str += hipGetErrorName(ec);
    str += ")";
    return str;
  }

public:
  explicit
  test_hip_error(hipError_t ec, const std::string& what = "")
    : system_error(ec, std::system_category(), message(ec, what))
  {}
};

inline void
test_hip_check(hipError_t status, const char *note = "") {
  if (status != hipSuccess) {           \
    throw test_hip_error(status, note);       \
  }
}


class
hip_test_timer {
  std::chrono::high_resolution_clock::time_point mTimeStart;
public:
  hip_test_timer() {
    reset();
  }
  long long
  stop() {
    std::chrono::high_resolution_clock::time_point timeEnd = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - mTimeStart).count();
  }
  void
  reset() {
    mTimeStart = std::chrono::high_resolution_clock::now();
  }
};

// Abstraction of device buffer so we can do automatic buffer dealocation (RAII)
template<typename T> class
hip_test_device_bo {
  T *_buffer;
public:
  hip_test_device_bo(size_t size) : _buffer(nullptr) {
    test_hip_check(hipMalloc((void**)&_buffer, size * sizeof(T)));
  }
  ~hip_test_device_bo() noexcept {
    test_hip_check(hipFree(_buffer));
  }
  T *get() const {
    return _buffer;
  }

  T *&get() {
    return _buffer;
  }

};

class
hip_test_device {
private:
  hipDevice_t m_device;
  int m_index;
  std::map<std::string, hipModule_t> mModuleTable;

public:
  hip_test_device(int index = 0) : m_index(index) {
    test_hip_check(hipDeviceGet(&m_device, index));
  }

  ~hip_test_device() {
    for (auto it : mModuleTable)
      (void)hipModuleUnload(it.second);
  }

  void
  show_info(std::ostream &stream) const {
    char name[64];
    test_hip_check(hipDeviceGetName(name, sizeof(name), m_device));
    stream << name << std::endl;

    hipUUID_t hid;
    test_hip_check(hipDeviceGetUuid(&hid, m_device));
    boost::uuids::uuid bid;
    std::memcpy(&bid, hid.bytes, sizeof(hid));
    stream << bid << std::endl;

    hipDeviceProp_t devProp;
    test_hip_check(hipGetDeviceProperties(&devProp, m_index));
    stream << devProp.name << std::endl;
    stream << devProp.totalGlobalMem/0x100000 << " MB" << std::endl;
    stream << devProp.maxThreadsPerBlock << " Threads" << std::endl;
  }

  hipFunction_t
  get_function(const char *fileName, const char *funcName) {
    std::map<std::string, hipModule_t>::iterator it = mModuleTable.find(fileName);
    hipModule_t hmodule;
    hmodule = it->second;
    if (it == mModuleTable.end()) {
      test_hip_check(hipModuleLoad(&hmodule, fileName), fileName);
      mModuleTable.insert(it, std::pair<std::string, hipModule_t>(fileName, hmodule));
    }
    hipFunction_t hfunction;
    test_hip_check(hipModuleGetFunction(&hfunction, hmodule, funcName), funcName);
    return hfunction;
  }
};
