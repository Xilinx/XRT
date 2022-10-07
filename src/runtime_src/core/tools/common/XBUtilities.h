// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __XBUtilities_h_
#define __XBUtilities_h_

// Include files
// Please keep these to the bare minimum
#include "core/common/device.h"
#include "core/common/query_requests.h"

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>

namespace XBUtilities {

  class Timer {
  private:
    std::chrono::high_resolution_clock::time_point m_time_start;

  public:
    Timer() { reset(); }

    std::chrono::duration<double> get_elapsed_time() 
    {
      std::chrono::high_resolution_clock::time_point time_end = std::chrono::high_resolution_clock::now();
      return std::chrono::duration<double>(time_end - m_time_start);
    }

    void reset() { m_time_start = std::chrono::high_resolution_clock::now(); }

    static std::string
    format_time(std::chrono::duration<double> duration);
  };
 
  void can_proceed_or_throw(const std::string& info, const std::string& error);

  void print_exception(const std::system_error& e);

  void xrt_version_cmp(bool isUserDomain);


  void collect_devices( const std::set<std::string>  &_deviceBDFs,
                        bool _inUserDomain,
                        xrt_core::device_collection &_deviceCollection);

  std::shared_ptr<xrt_core::device> get_device ( const std::string &deviceBDF,
                                                 bool in_user_domain);

  boost::property_tree::ptree
  get_available_devices(bool inUserDomain);

   /**
   * get_axlf_section() - Get section from the file passed in
   *
   * filename: file containing the axlf section
   *
   * Return: pair of section data and size in bytes
   */
  std::vector<char>
  get_axlf_section(const std::string& filename, axlf_section_kind section);

  /**
   * get_uuids() - Get UUIDs from the axlf section
   *
   * dtbuf: axlf section to be parsed
   *
   * Return: list of UUIDs
   */
  std::vector<std::string> get_uuids(const void *dtbuf);

  xrt_core::query::reset_type str_to_reset_obj(const std::string& str);

  std::string
  get_xrt_pretty_version();

  /**
   * OEM ID is a unique number called as the
   * Private Enterprise Number (PEN) maintained by IANA
   *
   * Return: Manufacturer's name
   */
  std::string
  parse_oem_id(const std::string& oemid);

  std::string
  parse_clock_id(const std::string& id);


  std::string
  string_to_UUID(std::string str);

  enum class unit
  {
    bytes,
    Hertz
  };
  uint64_t
  string_to_base_units(std::string str, const unit& conversion_unit);

  inline bool
  is_power_of_2(const uint64_t x)
  {
    /*
    * Verify that the given value is greater than zero
    * and that only one bit is set in the given value
    * if only one bit is set this implies the given value is a power of 2
    */
    return (x != 0) && ((x & (x - 1)) == 0);
  }

 /*
  * xclbin locking
  */
  struct xclbin_lock
  {
    xrt_core::device* m_device;
    xuid_t m_uuid;

    xclbin_lock(xrt_core::device* device)
      : m_device(device)
    {
      auto xclbinid = xrt_core::device_query<xrt_core::query::xclbin_uuid>(m_device);

      uuid_parse(xclbinid.c_str(), m_uuid);

      if (uuid_is_null(m_uuid))
        throw std::runtime_error("'uuid' invalid, please re-program xclbin.");

      m_device->open_context(m_uuid, std::numeric_limits<unsigned int>::max(), true);
    }

    ~xclbin_lock()
    {
      m_device->close_context(m_uuid, std::numeric_limits<unsigned int>::max());
    }
  };

};

#endif
