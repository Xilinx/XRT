/* SPDX-License-Identifier: Apache License 2.0 */
/* Copyright (C) 2023 Advanced Micro Devices, Inc. */

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

class HIPError : public std::system_error
{
private:
    static std::string message(hipError_t ec, const std::string& what) {
        std::string str = what;
        str += ": ";
        str += hipGetErrorString(ec);
        str += " (";
        str += hipGetErrorName(ec);
        str += ")";
        return str;
//        hipDrvGetErrorString(result, str);
    }

public:
  explicit
  HIPError(hipError_t ec, const std::string& what = "")
      : system_error(ec, std::system_category(), message(ec, what))
  {}
};

inline void hipCheck(hipError_t status, const char *note = "") {
    if (status != hipSuccess) {       \
        throw HIPError(status, note);   \
    }
}


class Timer {
    std::chrono::high_resolution_clock::time_point mTimeStart;
public:
    Timer() {
        reset();
    }
    long long stop() {
        std::chrono::high_resolution_clock::time_point timeEnd = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - mTimeStart).count();
    }
    void reset() {
        mTimeStart = std::chrono::high_resolution_clock::now();
    }
};

// Abstraction of device buffer so we can do automatic buffer dealocation (RAII)
template<typename T> class DeviceBO {
    T *_buffer;
public:
    DeviceBO(size_t size) : _buffer(nullptr) {
        hipCheck(hipMalloc((void**)&_buffer, size * sizeof(T)));
    }
    ~DeviceBO() noexcept {
        hipCheck(hipFree(_buffer));
    }
    T *get() const {
        return _buffer;
    }

    T *&get() {
        return _buffer;
    }

};

class HipDevice {
private:
    hipDevice_t mDevice;
    int mIndex;
    std::map<std::string, hipModule_t> mModuleTable;

public:
    HipDevice(int index = 0) : mIndex(index) {
        hipCheck(hipDeviceGet(&mDevice, index));
    }

    virtual ~HipDevice() {
        for (auto it : mModuleTable)
            (void)hipModuleUnload(it.second);
    }

    void showInfo(std::ostream &stream) const {
        char name[64];
        hipCheck(hipDeviceGetName(name, sizeof(name), mDevice));
        stream << name << std::endl;

        hipUUID_t hid;
        hipCheck(hipDeviceGetUuid(&hid, mDevice));
        boost::uuids::uuid bid;
        std::memcpy(&bid, hid.bytes, sizeof(hid));
        stream << bid << std::endl;

        hipDeviceProp_t devProp;
        hipCheck(hipGetDeviceProperties(&devProp, mIndex));
        stream << devProp.name << std::endl;
        stream << devProp.totalGlobalMem/0x100000 << " MB" << std::endl;
        stream << devProp.maxThreadsPerBlock << " Threads" << std::endl;
    }

    hipFunction_t getFunction(const char *fileName, const char *funcName) {
        std::map<std::string, hipModule_t>::iterator it = mModuleTable.find(fileName);
        hipModule_t hmodule;
        hmodule = it->second;
        if (it == mModuleTable.end()) {
            hipCheck(hipModuleLoad(&hmodule, fileName), fileName);
            mModuleTable.insert(it, std::pair<std::string, hipModule_t>(fileName, hmodule));
        }
        hipFunction_t hfunction;
        hipCheck(hipModuleGetFunction(&hfunction, hmodule, funcName), funcName);
        return hfunction;
    }
};
