/*
 * Copyright (C) 2019-2021 Xilinx, Inc
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

#include "error.h"
#include "ishim.h"
#include "query.h"
#include "query_reset.h"
#include "scope_guard.h"
#include "uuid.h"

#include "core/include/xrt.h"
#include "core/include/experimental/xrt_xclbin.h"

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <boost/any.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/optional/optional.hpp>

// TODO: don't seem to belong here and definitely not as #defines
#define XILINX_ID  0x10ee
#define ARISTA_ID  0x3475
#define INVALID_ID 0xffff

namespace xrt_core {

using device_collection = std::vector<std::shared_ptr<xrt_core::device>>;

/**
 * class device - interface to support OS agnositic operations on a device
 */
class device : public ishim
{
public:
  // device index type
  using id_type = unsigned int;
  using handle_type = xclDeviceHandle;
  using memory_type = xrt::xclbin::mem::memory_type;

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
   */
  virtual handle_type
  get_device_handle() const = 0;

  /**
   * get_mgmt_handle() - Get underlying mgmt device handle if any
   *
   * Return: Handle for mgmt device, or XRT_NULL_HANDLE if undefined
   *
   * Currently windows is only OS that differentiates mgmt handle from
   * device handle.  As such this function really doesn't belong here
   * in base class, but it avoids dynamic_cast from base device to
   * concrete device for query calls.
   */
  virtual handle_type
  get_mgmt_handle() const
  {
    return XRT_NULL_HANDLE;
  }

  /**
   * get_user_handle() - Get underlying user device handle if any
   *
   * Return: Handle for user device.
   *
   * User the device is default the same as device handle.
   */
  virtual handle_type
  get_user_handle() const
  {
    return get_device_handle();
  }

  /**
   * is_userpf_device() - Is this device a userpf
   *
   * Return: true if this device is associate with userpf, false otherwise
   *
   * This currently makes sense only on Linux.  It used by
   * device_linux to direct sysfs calls to the proper pf.  As such
   * this function really doesn't belong here in base class, but it
   * avoids dynamic_cast from base device to concrete device for
   * query calls.
   */
  virtual bool
  is_userpf() const
  {
    return false;
  }

  /**
   * is_nodma() - Is this device a NODMA device
   *
   * Return: true if device is nodma
   *
   * This function is added to avoid sysfs access in
   * critical path.
   */
  XRT_CORE_COMMON_EXPORT
  bool
  is_nodma() const;

 private:
  // Private look up function for concrete query::request
  virtual const query::request&
  lookup_query(query::key_type query_key) const = 0;

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

  /**
   * update() - Update a given property for this device
   *
   * @QueryRequestType: Template parameter identifying a specific query request
   * @args:  Variadic arguments forwarded to the QueryRequestType
   */
  template <typename QueryRequestType, typename ...Args>
  void
  update(Args&&... args) const
  {
    auto& qr = lookup_query(QueryRequestType::key);
    return qr.put(this, std::forward<Args>(args)...);
  }

  /**
   * load_xclbin() - Load an xclbin object on this device
   *
   * This function loads argument xclbin.  It is the entry point for
   * xrt::device::load_xclbin() APIs variants.  The function keeps a
   * reference to the argument xclbin.
   */
  XRT_CORE_COMMON_EXPORT
  void
  load_xclbin(const xrt::xclbin& xclbin);

  XRT_CORE_COMMON_EXPORT
  void
  load_xclbin(const uuid& xclbin_id);

  /**
   * register_axlf() - Callback from shim after AXLF succesfully loaded
   *
   * This function is called after an axlf has been succesfully loaded
   * by the shim layer API xclLoadXclBin().  Since xclLoadXclBin() can
   * be called explicitly by end-user code, the callback is necessary
   * in order to register current axlf with the device object
   */
  XRT_CORE_COMMON_EXPORT
  void
  register_axlf(const axlf*);

  /**
   * get_xclbin_uuid() - Get uuid of currently loaded xclbin
   */
  XRT_CORE_COMMON_EXPORT
  uuid
  get_xclbin_uuid() const;

  /**
   * get_axlf_section() - Get section from currently loaded axlf
   *
   * xclbin_id:  Check that xclbin_id matches currently cached
   * Return:     Pair of section data and size in bytes
   *
   * This function provides access to meta data sections that are
   * from currently loaded xclbin.  The returned section is from when the
   * xclbin was loaded by this process.  The function cannot be used
   * unless this process loaded the xclbin.
   *
   * The function returns {nullptr, 0} if section is not cached.
   *
   * Same behavior as other get_axlf_section()
   */
  XRT_CORE_COMMON_EXPORT
  std::pair<const char*, size_t>
  get_axlf_section(axlf_section_kind section, const uuid& xclbin_id = uuid()) const;

  std::pair<const char*, size_t>
  get_axlf_section_or_error(axlf_section_kind section, const uuid& xclbin_id = uuid()) const;

  template<typename SectionType>
  SectionType
  get_axlf_section(axlf_section_kind section, const uuid& xclbin_id = uuid()) const
  {
    return reinterpret_cast<SectionType>(get_axlf_section(section, xclbin_id).first);
  }

  template<typename SectionType>
  SectionType
  get_axlf_section_or_error(axlf_section_kind section, const uuid& xclbin_id = uuid()) const
  {
    return reinterpret_cast<SectionType>(get_axlf_section_or_error(section, xclbin_id).first);
  }

  /**
   * get_memidx_encoding() - An encoding compressing mem topology indices
   *
   * Returned container is indexed by mem_topology index and maps to
   * encoded index.
   */
  const std::vector<size_t>&
  get_memidx_encoding(const uuid& xclbin_id = uuid()) const;

  memory_type
  get_memory_type(size_t memidx) const;

  /**
   * get_ert_slots() - Get number of ERT CQ slots
   *
   * Returns: Pair of number of slots and size of each slot
   */
  XRT_CORE_COMMON_EXPORT
  std::pair<size_t, size_t>
  get_ert_slots(const char* xml, size_t xml_size) const;

  XRT_CORE_COMMON_EXPORT
  std::pair<size_t, size_t>
  get_ert_slots() const;

  // Move all these 'pt' functions out the class interface
  virtual void get_info(boost::property_tree::ptree&) const {}
  /**
   * read() - maps pcie bar and copy bytes word (32bit) by word
   * THIS FUNCTION DOES NOT BELONG HERE
   */
  virtual void read(uint64_t, void*, uint64_t) const {}
  /**
   * write() - maps pcie bar and copy bytes word (32bit) by word
   * THIS FUNCTION DOES NOT BELONG HERE
   */
  virtual void write(uint64_t, const void*, uint64_t) const {}

  virtual void reset(query::reset_type&) const {}
 
  /**
   * xclmgmt_load_xclbin() - loads the xclbin through the mgmt pf
   */
  virtual void xclmgmt_load_xclbin(const char*) const{}

  /**
   * open() - opens a device with an fd which can be used for non pcie read/write
   * xospiversal and xspi use this
   */
  virtual int
  open(const std::string&, int) const
  { throw std::runtime_error("Not implemented"); }

  /**
   * close() - close the fd
   */
  virtual void
  close(int) const
  { throw std::runtime_error("Not implemented"); }

  /**
   * file_open() - Opens a scoped fd
   * THIS FUNCTION DOES NOT BELONG HERE
   */
  scope_value_guard<int, std::function<void()>>
  file_open(const std::string& subdev, int flag) const
  {
    auto fd = open(subdev, flag);
    return {fd, std::bind(&device::close, this, fd)};
  }

 private:
  id_type m_device_id;
  mutable boost::optional<bool> m_nodma = boost::none;

  std::vector<size_t> m_memidx_encoding; // compressed mem_toplogy indices
  xrt::xclbin m_xclbin;                  // currently loaded xclbin
};

/**
 * device_query() - Retrive query request data
 *
 * @device : device to retrieve data for
 * Return: value per QueryRequestType
 */

template <typename QueryRequestType>
inline typename QueryRequestType::result_type
device_query(const device* device)
{
  auto ret = device->query<QueryRequestType>();
  return boost::any_cast<typename QueryRequestType::result_type>(ret);
}

template <typename QueryRequestType, typename ...Args>
inline typename QueryRequestType::result_type
device_query(const device* device, Args&&... args)
{
  auto ret = device->query<QueryRequestType>(std::forward<Args>(args)...);
  return boost::any_cast<typename QueryRequestType::result_type>(ret);
}

template <typename QueryRequestType>
inline typename QueryRequestType::result_type
device_query(const std::shared_ptr<device>& device)
{
  return device_query<QueryRequestType>(device.get());
}

template <typename QueryRequestType, typename ...Args>
inline typename QueryRequestType::result_type
device_query(const std::shared_ptr<device>& device, Args&&... args)
{
  auto ret = device->query<QueryRequestType>(std::forward<Args>(args)...);
  return boost::any_cast<typename QueryRequestType::result_type>(ret);
}

template <typename QueryRequestType, typename ...Args>
inline void
device_update(const device* device, Args&&... args)
{
  device->update<QueryRequestType>(std::forward<Args>(args)...);
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
