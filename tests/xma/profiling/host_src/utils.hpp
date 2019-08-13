// Copyright (C) 2018 Xilinx Inc.
// All rights reserved.

#ifndef _utils_hpp
#define _utils_hpp

#include <iostream>
#include <stdexcept>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <limits>
#include <chrono>
#include <mutex>
#include <cstring>
#include <cstdarg>
#include <cstdio>

#include <sys/mman.h>

// driver includes
#include "ert.h"

// host_src includes
#include "xclhal2.h"
#include "xclbin.h"

#include <fstream>

#define MAYBE_UNUSED __attribute__((unused))

namespace utils {

static std::mutex s_debug_mutex;

MAYBE_UNUSED
static void 
debugf(const char* format,...)
{
  std::lock_guard<std::mutex> lk(s_debug_mutex);
  va_list args;
  va_start(args,format);
  vprintf(format,args);
  va_end(args);
}

#ifdef VERBOSE
# define DEBUGF(format,...) utils::debugf(format, ##__VA_ARGS__)
# define PRINTF(format,...) utils::debugf(format, ##__VA_ARGS__)
#else
# define DEBUGF(format,...)
# define PRINTF(format,...) utils::debugf(format, ##__VA_ARGS__)
#endif

/**
 * @return
 *   nanoseconds since first call
 */
MAYBE_UNUSED
unsigned long
time_ns()
{
  static auto zero = std::chrono::high_resolution_clock::now();
  auto now = std::chrono::high_resolution_clock::now();
  auto integral_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now-zero).count();
  return integral_duration;
}

/**
 * Simple time guard to accumulate scoped time
 */
class time_guard
{
  unsigned long zero = 0;
  unsigned long& tally;
public:
  time_guard(unsigned long& t)
    : zero(time_ns()), tally(t)
  {}

  ~time_guard()
  {
    tally += time_ns() - zero;
  }
};

/**
 * struct device_object - wrapper for device handle and xclbin data
 *
 * @handle: device handle
 * @cu_base_addr: base address for CUs in device
 */
struct device_object
{
  xclDeviceHandle handle;
  uint64_t cu_base_addr;
};

using device = std::shared_ptr<device_object>;

/**
 * struct buffer_object - wrapper for a buffer object
 *
 * @bo: buffer object handle
 * @data: mapped kernel space data accessible in userspace
 * @size: size of buffer object
 * @dev: device handle associated with this buffer object
 */
struct buffer_object
{
  unsigned int bo;
  void* data;
  size_t size;
  xclDeviceHandle dev;
};

using buffer = std::shared_ptr<buffer_object>;

/**
 * create_exec_bo() - create a buffer object command execution
 *
 * @device: Device to associated with the buffer object should be allocated
 * @sz: Size of the buffer object
 * Return: Shared pointer to the allocated and mapped buffer object
 */
MAYBE_UNUSED
buffer
create_exec_bo(const device& device, size_t sz)
{
  auto delBO = [](buffer_object* bo) {
    munmap(bo->data,bo->size);
    xclFreeBO(bo->dev,bo->bo);
  };

  auto ubo = std::make_unique<buffer_object>();
  ubo->dev = device->handle;
  ubo->bo = xclAllocBO(ubo->dev,sz,xclBOKind(0),(1<<31));
  ubo->data = xclMapBO(ubo->dev,ubo->bo,true /*write*/);
  ubo->size = sz;
  std::memset(reinterpret_cast<ert_packet*>(ubo->data),0,sz);
  return buffer(ubo.release(),delBO);
}

/**
 * Manage a free list of exec buffer objects to avoid repeated allocation
 * This is useful only if all buffers are allocated with same size
 */
static std::map<void*, std::vector<buffer>> s_freelist;

/**
 * get_exec_buffer() - Get an exec buffer object form the freelist or allocate
 *
 * @device: Device to associated with the buffer object should be allocated
 * @sz: Size of the buffer object
 * Return: Shared pointer to the allocated and mapped buffer object
 *
 * The function fails if requested size doesn't match the size of the
 * first free object on the freelist
 */
MAYBE_UNUSED
static buffer
get_exec_buffer(const device& device, size_t sz)
{
  auto itr = s_freelist.find(device->handle);
  if (itr != s_freelist.end()) {
    auto& freelist = (*itr).second;
    if (!freelist.empty()) {
      auto buffer = freelist.back();
      freelist.pop_back();
      if (buffer->size != sz)
        throw std::runtime_error("requested size mismatch");
      std::memset(reinterpret_cast<ert_packet*>(buffer->data),0,sz);
      return buffer;
    }
  }
  return create_exec_bo(device,sz);
}

/**  
 * recycle_exec_buffer() - recycle a used exec buffer object
 * 
 * @ebo: Exec buffer object to recycle
 */
MAYBE_UNUSED
static void
recycle_exec_buffer(buffer ebo)
{
  s_freelist[ebo->dev].emplace_back(std::move(ebo));
}

/**
 * create_bo() - Create a 'ram' buffer object
 *
 * @device: Device to associate with the buffer object should be allocated
 * @sz: Size of the buffer object
 * @bank: Bank number to allocated buffer in; if not specified allocate in default bank
 * Return: Shared pointer to the allocated and mapped buffer object
 */
MAYBE_UNUSED
buffer
create_bo(const device& device, size_t sz, int bank=-1)
{
  auto delBO = [](buffer_object* bo) {
    munmap(bo->data,bo->size);
    xclFreeBO(bo->dev,bo->bo);
  };

  auto ubo = std::make_unique<buffer_object>();
  ubo->dev = device->handle;
  ubo->bo = bank>=0
    ? xclAllocBO(ubo->dev,sz,XCL_BO_DEVICE_RAM,(1<<bank))
    : xclAllocBO(ubo->dev,sz,XCL_BO_DEVICE_RAM,0);
  ubo->data = xclMapBO(ubo->dev,ubo->bo,true /*write*/);
  ubo->size = sz;
  return buffer(ubo.release(),delBO);
}

/**
 * init() - Initialize xrt with an xclbin file
 *
 * @bit: Path to xclbin file
 * @device_index: Index of device to open
 * @log: Log file
 * Return: Shared pointer to device object
 */  
MAYBE_UNUSED
static device
init(const std::string& bit, unsigned int deviceIndex, const std::string& log)
{
  auto delDO = [](device_object* dobj) {
    xclClose(dobj->handle);
  };

  if(deviceIndex >= xclProbe())
    throw std::runtime_error("Cannot find device index specified");

  auto udo = std::make_unique<device_object>();
  udo->handle = xclOpen(deviceIndex, log.c_str(), XCL_INFO);
  
  xclDeviceInfo2 deviceInfo;
  if (xclGetDeviceInfo2(udo->handle, &deviceInfo))
    throw std::runtime_error("Unable to obtain device information");

  std::cout << "Shell = " << deviceInfo.mName << "\n";
  std::cout << "Index = " << deviceIndex << "\n";
  std::cout << "PCIe = GEN" << deviceInfo.mPCIeLinkSpeed << " x " << deviceInfo.mPCIeLinkWidth << "\n";
  std::cout << "OCL Frequency = " << deviceInfo.mOCLFrequency[0] << " MHz" << "\n";
  std::cout << "DDR Bank = " << deviceInfo.mDDRBankCount << "\n";
  std::cout << "Device Temp = " << deviceInfo.mOnChipTemp << " C\n";
  std::cout << "MIG Calibration = " << std::boolalpha << deviceInfo.mMigCalib << std::noboolalpha << "\n";

  if (xclLockDevice(udo->handle))
    throw std::runtime_error("Cannot lock device");

  std::ifstream stream(bit);
  stream.seekg(0,stream.end);
  size_t size = stream.tellg();
  stream.seekg(0,stream.beg);

  std::vector<char> header(size);
  stream.read(header.data(),size);

  if (std::strncmp(header.data(), "xclbin2", 8))
    throw std::runtime_error("Invalid bitstream");
  
  auto xclbin = reinterpret_cast<const xclBin*>(header.data());
  if (xclLoadXclBin(udo->handle, xclbin))
    throw std::runtime_error("Bitstream download failed");

  std::cout << "Finished downloading bitstream " << bit << std::endl;

  auto top = reinterpret_cast<const axlf*>(header.data());
  auto ip = xclbin::get_axlf_section(top, IP_LAYOUT);
  auto layout = reinterpret_cast<ip_layout*>(header.data() + ip->m_sectionOffset);

  // compute cu base addr
  udo->cu_base_addr = std::numeric_limits<uint64_t>::max();
  std::for_each(layout->m_ip_data,layout->m_ip_data+layout->m_count,
                [&udo](auto ip_data) {
                  if (ip_data.m_type != IP_KERNEL)
                    return;
                  udo->cu_base_addr = std::min(udo->cu_base_addr,ip_data.m_base_address);
                });

  return device(udo.release(),delDO);
}

} // namespace utils

#endif
