/**
 * Copyright (C) 2019 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef XRT_CORE_DEVICE_H
#define XRT_CORE_DEVICE_H

#include "config.h"
#include "query.h"
#include "error.h"
#include "ishim.h"
#include "scope_guard.h"
#include "xrt.h"

// Please keep eternal include file dependencies to a minimum
#include <cstdint>
#include <vector>
#include <string>
#include <boost/any.hpp>
#include <boost/property_tree/ptree.hpp>

namespace xrt_core {

/**
 * class device - interface to support OS agnositic operations on a device
 */
class device : ishim
{

public:
  // device index type
  using id_type = unsigned int;
public:

  XRT_CORE_COMMON_EXPORT
  device(id_type device_id);

  XRT_CORE_COMMON_EXPORT
  virtual ~device();

  device(const device&) = delete;
  device& operator=(const device&) = delete;

  /**
   * get_device_id() - Get device index
   */
  id_type
  get_device_id() const
  {
    return m_device_id;
  }

  /**
   * get_device_handle() - Get underlying shim device handle
   *
   * Throws if called on non userof devices
   */
  virtual xclDeviceHandle
  get_device_handle() const = 0;

  virtual xclDeviceHandle
  get_mgmt_handle() const
  {
    return XRT_NULL_HANDLE;
  }

  virtual xclDeviceHandle
  get_user_handle() const
  {
    return XRT_NULL_HANDLE;
  }

  /**
   * is_userpf_device() - Is this device a userpf
   */
  virtual bool
  is_userpf() const
  {
    return false;
  }

 private:
  // Private look up function for concrete query::request
  virtual const query::request&
  lookup_query(query::key_type query_key) const = 0;
  /**
   * open() - opens a device with an fd which can be used for non pcie read/write
   * xospiversal and xspi use this
   */
  virtual int  open(const std::string& subdev, int flag) const = 0;
  /**
   * close() - close the fd
   */
  virtual void close(int dev_handle) const = 0;

public:
  /**
   * query() - Query the device for specific property
   *
   * @QueryRequestType: Template parameter identifying a specific query request
   * Return: QueryRequestType::result_type value wrapped as boost::any.
   */
  template <typename QueryRequestType>
  boost::any
  query() const
  {
    auto& qr = lookup_query(QueryRequestType::key);
    return qr.get(this);
  }

  /**
   * query() - Query the device for specific property
   *
   * @QueryRequestType: Template parameter identifying a specific query request
   * @args:  Variadic arguments forwarded the QueryRequestType
   * Return: QueryRequestType::result_type value wrapped as boost::any.
   */
  template <typename QueryRequestType, typename ...Args>
  boost::any
  query(Args&&... args) const
  {
    auto& qr = lookup_query(QueryRequestType::key);
    return qr.get(this, std::forward<Args>(args)...);
  }

  virtual void get_info(boost::property_tree::ptree &pt) const = 0;
  virtual void read_dma_stats(boost::property_tree::ptree &pt) const = 0;

  void get_rom_info(boost::property_tree::ptree & pt) const;
  void get_xmc_info(boost::property_tree::ptree & pt) const;
  void get_platform_info(boost::property_tree::ptree & pt) const;
  void read_thermal_pcb(boost::property_tree::ptree &pt) const;
  void read_thermal_fpga(boost::property_tree::ptree &pt) const;
  void read_fan_info(boost::property_tree::ptree &pt) const;
  void read_thermal_cage(boost::property_tree::ptree &pt) const;
  void read_electrical(boost::property_tree::ptree &pt) const;
  void read_power(boost::property_tree::ptree &pt) const;
  void read_firewall(boost::property_tree::ptree &pt) const;

  /**
   * read() - maps pcie bar and copy bytes word (32bit) by word
   */
  virtual void read(uint64_t offset, void* buf, uint64_t len) const = 0;
  /**
   * write() - maps pcie bar and copy bytes word (32bit) by word
   */
  virtual void write(uint64_t offset, const void* buf, uint64_t len) const = 0;
  
  /* 
   * file_open() - Opens a scoped fd
   */
  scope_guard<int, std::function<void(int)>>
  file_open(const std::string& subdev, int flag)
  {
    return scope_guard<int, std::function<void(int)>>(open(subdev, flag), std::bind(&device::close, this, std::placeholders::_1));
  }

  // Helper methods
  typedef std::string (*FORMAT_STRING_PTR)(const boost::any &);
  static std::string format_primative(const boost::any & _data);
  static std::string format_hex(const boost::any & _data);
  static std::string format_hex_base2_shiftup30(const boost::any & _data);
  static std::string format_base10_shiftdown3(const boost::any &_data);
  static std::string format_base10_shiftdown6(const boost::any &_data);

 private:
  id_type m_device_id;
};

/**
 * device_query() - Retrive query request data
 *
 * @device : device to retrieve data for
 * Return: value per QueryRequestType
 */

template <typename QueryRequestType>
static typename QueryRequestType::result_type
device_query(const device* device)
{
  auto ret = device->query<QueryRequestType>();
  return boost::any_cast<typename QueryRequestType::result_type>(ret);
}

template <typename QueryRequestType>
static typename QueryRequestType::result_type
device_query(const std::shared_ptr<device>& device)
{
  return device_query<QueryRequestType>(device.get());
}

template <typename QueryRequestType, typename ...Args>
static typename QueryRequestType::result_type
device_query(const std::shared_ptr<device>& device, Args&&... args)
{
  return device_query<QueryRequestType>(device.get(), std::forward<Args>(args)...);
}

template <typename QueryRequestType>
struct ptree_updater
{
  template <typename ValueType>
  static void
  put(const ValueType& value, boost::property_tree::ptree& pt)
  {
    pt.put(QueryRequestType::name(), QueryRequestType::to_string(value));
  }

  static void
  put(const std::vector<std::string>& value, boost::property_tree::ptree& pt)
  {
    boost::property_tree::ptree pt_array;
    for (auto& str : value) {
      boost::property_tree::ptree pt_item;
      pt_item.put("", QueryRequestType::to_string(str));
      pt_array.push_back(std::make_pair("", pt_item));
    }
    pt.add_child(QueryRequestType::name(), pt_array);
  }
  
  static void
  query_and_put(const device* device, boost::property_tree::ptree& pt)
  {
    try {
      auto value = xrt_core::device_query<QueryRequestType>(device);
      put(value, pt);
    }
    catch (const std::exception& ex) {
      pt.put(QueryRequestType::name(), ex.what());
    }
  }
};

} // xrt_core

#endif
