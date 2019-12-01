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

#include "error.h"
#include "xrt.h"

// Please keep eternal include file dependencies to a minimum
#include <boost/property_tree/ptree.hpp>
#include <cstdint>
#include <vector>
#include <string>
#include <list>
#include <map>
#include <boost/any.hpp>

namespace xrt_core {

/**
 * class device - interface to support OS agnositic operations on a device
 */
class device
{
public:
  // device index type
  using id_type = unsigned int;

  /**
   * enum QueryRequest - Query request types
   *
   * Used with @query_device to retrive device information from driver
   * per OS inmplementation.  For example, on Linux most query
   * requests will be implemented using sysfs, whereas on Windows, the
   * query requests are implemented as ioctl calls.
   */
  enum QueryRequest {
      QR_PCIE_VENDOR,
      QR_PCIE_DEVICE,
      QR_PCIE_SUBSYSTEM_VENDOR,
      QR_PCIE_SUBSYSTEM_ID,
      QR_PCIE_LINK_SPEED,
      QR_PCIE_EXPRESS_LANE_WIDTH,

      QR_DMA_THREADS_RAW,

      QR_ROM_VBNV,
      OR_ROM_DDR_BANK_SIZE,
      QR_ROM_DDR_BANK_COUNT_MAX,
      QR_ROM_FPGA_NAME,

      QR_XMC_VERSION,
      QR_XMC_SERIAL_NUM,
      QR_XMC_MAX_POWER,
      QR_XMC_BMC_VERSION,

      QR_DNA_SERIAL_NUM,
      QR_CLOCK_FREQS,
      QR_IDCODE,

      QR_STATUS_MIG_CALIBRATED,
      QR_STATUS_P2P_ENABLED,

      QR_TEMP_CARD_TOP_FRONT,
      QR_TEMP_CARD_TOP_REAR,
      QR_TEMP_CARD_BOTTOM_FRONT,

      QR_TEMP_FPGA,

      QR_FAN_TRIGGER_CRITICAL_TEMP,
      QR_FAN_FAN_PRESENCE,
      QR_FAN_SPEED_RPM,

      QR_CAGE_TEMP_0,
      QR_CAGE_TEMP_1,
      QR_CAGE_TEMP_2,
      QR_CAGE_TEMP_3,

      QR_12V_PEX_MILLIVOLTS,
      QR_12V_PEX_MILLIAMPS,

      QR_12V_AUX_MILLIVOLTS,
      QR_12V_AUX_MILLIAMPS,

      QR_3V3_PEX_MILLIVOLTS,
      QR_3V3_AUX_MILLIVOLTS,

      QR_DDR_VPP_BOTTOM_MILLIVOLTS,
      QR_DDR_VPP_TOP_MILLIVOLTS,
      QR_5V5_SYSTEM_MILLIVOLTS,
      QR_1V2_VCC_TOP_MILLIVOLTS,
      QR_1V2_VCC_BOTTOM_MILLIVOLTS,
      QR_1V8_MILLIVOLTS,
      QR_0V85_MILLIVOLTS,
      QR_0V9_VCC_MILLIVOLTS,
      QR_12V_SW_MILLIVOLTS,
      QR_MGT_VTT_MILLIVOLTS,
      QR_INT_VCC_MILLIVOLTS,
      QR_INT_VCC_MILLIAMPS,
      QR_3V3_PEX_MILLIAMPS,
      QR_0V85_MILLIAMPS,
      QR_3V3_VCC_MILLIVOLTS,
      QR_HBM_1V2_MILLIVOLTS,
      QR_2V5_VPP_MILLIVOLTS,
      QR_INT_BRAM_VCC_MILLIVOLTS,
      QR_FIREWALL_DETECT_LEVEL,
      QR_FIREWALL_STATUS,
      QR_FIREWALL_TIME_SEC,
      QR_POWER_MICROWATTS,

      QR_FLASH_BAR_OFFSET
  };
public:

  device(id_type device_id);
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

public:
  /*
   * query_device() - Retrive query request data
   *
   * @deviceID : device to retrieve data for
   * @qr: query reqest type to retrieve
   * @ti: C++ typeid identifying the data type stored in the returned value
   * @ret: type erased boost::any object used to return the retrieved data
   *
   * This function isvirtual and must be defined by OS implmentation
   * classes.  The public interface into calling this funciton is a
   * templated function that pouplates the type_info argument.
   *
   * As strange as this function looks it is merely a poor mans
   * templated virtual function
   */
  virtual void query(QueryRequest qr, const std::type_info & ti, boost::any &ret) const = 0;
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

  //flash functions
  virtual void auto_flash(const std::string& shell, const std::string& id, bool force) const = 0;
  virtual void reset_shell() const = 0;
  virtual void update_shell(const std::string& flashType, const std::string& primary, const std::string& secondary) const = 0;
  virtual void update_SC(const std::string& file) const = 0;

  // Helper methods
  typedef std::string (*FORMAT_STRING_PTR)(const boost::any &);
  static std::string format_primative(const boost::any & _data);
  static std::string format_hex(const boost::any & _data);
  static std::string format_hex_base2_shiftup30(const boost::any & _data);
  static std::string format_base10_shiftdown3(const boost::any &_data);
  static std::string format_base10_shiftdown6(const boost::any &_data);

  void
  query_and_put(QueryRequest qr,
                const std::type_info & _typeInfo,
                boost::property_tree::ptree & pt,
                const std::string &_sPropertyName,
                FORMAT_STRING_PTR stringFormat = format_primative) const;

  void
  query_and_put(QueryRequest qr, boost::property_tree::ptree & pt) const;

  struct QueryRequestEntry {
    std::string sPrettyName;
    std::string sPtreeNodeName;
    const std::type_info *pTypeInfo;
    FORMAT_STRING_PTR string_formatter;
  };

  const QueryRequestEntry * get_query_entry(QueryRequest qr) const;

private:
  id_type m_device_id;
};

/**
 * class no_such_query - exception for missing query request
 *
 * Implementions should throw no_such_query if a query request is not
 * implemented.  This exception is to differentiate between not implemented
 * requests which is not necessarily fatal and other types of errors.
 */
class no_such_query : public std::runtime_error
{
  device::QueryRequest m_qr;
public:
  no_such_query(device::QueryRequest qr, const std::string& what = "")
    : std::runtime_error(what), m_qr(qr)
  {}

  device::QueryRequest
  get_qr() const
  {
    return m_qr;
  }
};

/**
 * invalid_query_value() - terminal to check for invalid query value
 *
 * query_device() returns query values, but on missing implementation
 * or non fatal error the query value should be invalid_query_value.
 */
template <typename QueryType>
static QueryType
invalid_query_value()
{
  return std::numeric_limits<QueryType>::max();
}

/**
 * query_device() - Retrive query request data
 *
 * @deviceID : device to retrieve data for
 * @qr: query reqest type to retrieve
 * Return:
 * @ret: type erased boost::any object used to return the retrieved data
 *
 * Public interface to access device data per query request.   The QueryType
 * template parameter defines the type of data returned in the return param.
 */
template <typename QueryType>
static QueryType
query_device(const std::shared_ptr<device>& device, device::QueryRequest qr)
{
  boost::any ret = invalid_query_value<QueryType>();
  try {
    device->query(qr, typeid(QueryType), ret);
  }
  catch (const no_such_query&) {
  }
  return boost::any_cast<QueryType>(ret);
}

} // xrt_core

#endif
