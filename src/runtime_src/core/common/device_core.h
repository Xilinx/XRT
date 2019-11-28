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

#ifndef DEVICE_CORE_H
#define DEVICE_CORE_H

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
 * class device_core - interface to support OS agnositic querying of device
 *
 * TODO: better class name reflecting its purpose.  It is really about querying
 * device info, device_core is confusing.
 */
class device_core
{
public:
  /**
   * Returns the class handle use to query the abstract
   * libraries
   *
   * @return The handle instance
   *
   * TODO: Hide this singleton instance behind public functions that
   * under the hood call instance().  No need for client code to
   * know about these singleton details.
   */
  static const device_core & instance();

public:
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
  virtual void query_device(uint64_t device_id, QueryRequest qr, const std::type_info & ti, boost::any &ret) const = 0;

public:
  virtual void get_devices(boost::property_tree::ptree &_pt) const = 0;
  virtual void get_device_info(uint64_t _deviceID, boost::property_tree::ptree &_pt) const = 0;
  virtual void read_device_dma_stats(uint64_t _deviceID, boost::property_tree::ptree &_pt) const = 0;

  //flash functions
  virtual void scan_devices(bool verbose, bool json) const = 0;
  virtual void auto_flash(uint64_t _deviceID, std::string& shell, std::string& id, bool force) const = 0;
  virtual void reset_shell(uint64_t _deviceID) const = 0;
  virtual void update_shell(uint64_t _deviceID, std::string flashType, std::string& primary, std::string& secondary) const = 0;
  virtual void update_SC(uint64_t _deviceID, std::string& file) const = 0;
  //end flash functions

  /**
   * get_total_devices() - Get total devices and total usable devices
   *
   * Return: Pair of total devices and usable devices
   */
  virtual std::pair<uint64_t, uint64_t> get_total_devices() const = 0;

 public:

  void get_device_rom_info(uint64_t _deviceID, boost::property_tree::ptree & _pt) const;
  void get_device_xmc_info(uint64_t _deviceID, boost::property_tree::ptree & _pt) const;
  void get_device_platform_info(uint64_t _deviceID, boost::property_tree::ptree & _pt) const;
  void read_device_thermal_pcb(uint64_t _deviceID, boost::property_tree::ptree &_pt) const;
  void read_device_thermal_fpga(uint64_t _deviceID, boost::property_tree::ptree &_pt) const;
  void read_device_fan_info(uint64_t _deviceID, boost::property_tree::ptree &_pt) const;
  void read_device_thermal_cage(uint64_t _deviceID, boost::property_tree::ptree &_pt) const;
  void read_device_electrical(uint64_t _deviceID, boost::property_tree::ptree &_pt) const;
  void read_device_power(uint64_t _deviceID, boost::property_tree::ptree &_pt) const;
  void read_device_firewall(uint64_t _deviceID, boost::property_tree::ptree &_pt) const;

 public:

  /**
   * class device - Encapsulate the device created from a card index
   *
   * @m_midx: card index
   * @m_name: name of device
   * @m_hdl: device handle per shim layer
   *
   * The device is opened immediately when constructed and closed upon
   * destruction.  The class supports execution of shim level functions
   * through its execute method (sample usage SubCmdProgram.cpp)
   */
  class device
  {
    uint64_t m_idx;
    std::string m_name;
    xclDeviceHandle m_hdl;

  public:
    explicit device(uint64_t _deviceID)
      : m_idx(_deviceID)
      , m_name("device[" + std::to_string(m_idx) + "]")
      , m_hdl(xclOpen(static_cast<unsigned int>(_deviceID), nullptr, XCL_QUIET))
    {
      if (!m_hdl)
        throw error("could not open " + m_name);
    }

    ~device()
    {
      xclClose(m_hdl);
    }

    template <typename ShimDeviceFunction, typename ...Args>
    decltype(auto)
    execute(ShimDeviceFunction&& f, Args&&... args)
    {
      return f(m_hdl, std::forward<Args>(args)...);
    }
  };

  /**
   * get_device() - Construct a managed device object frok a device ID
   */
  device
  get_device(uint64_t _deviceID) const;

  // Helper methods
 protected:
  typedef std::string (*FORMAT_STRING_PTR)(const boost::any &);
  static std::string format_primative(const boost::any & _data);
  static std::string format_hex(const boost::any & _data);
  static std::string format_hex_base2_shiftup30(const boost::any & _data);
  static std::string format_base10_shiftdown3(const boost::any &_data);
  static std::string format_base10_shiftdown6(const boost::any &_data);

  void query_device_and_put(uint64_t _deviceID,
                            QueryRequest _eQueryRequest,
                            const std::type_info & _typeInfo,
                            boost::property_tree::ptree & _pt,
                            const std::string &_sPropertyName,
                            FORMAT_STRING_PTR stringFormat = format_primative) const;

  void query_device_and_put(uint64_t _deviceID, QueryRequest _eQueryRequest, boost::property_tree::ptree & _pt) const;

 protected:

  struct QueryRequestEntry {
    std::string sPrettyName;
    std::string sPtreeNodeName;
    const std::type_info *pTypeInfo;
    FORMAT_STRING_PTR string_formatter;
  };

  const QueryRequestEntry * get_query_entry(QueryRequest _eQueryRequest) const;

 protected:
  device_core();
  virtual ~device_core();

 private:
  device_core(const device_core&) = delete;
  device_core& operator=(const device_core&) = delete;

 private:
  static std::map<QueryRequest, QueryRequestEntry> m_QueryTable;
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
  device_core::QueryRequest m_qr;
public:
  no_such_query(device_core::QueryRequest qr, const std::string& what = "")
    : std::runtime_error(what), m_qr(qr)
  {}

  device_core::QueryRequest
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
query_device(uint64_t device_id, device_core::QueryRequest qr)
{
  boost::any ret = invalid_query_value<QueryType>();
  try {
    device_core::instance().query_device(device_id, qr, typeid(QueryType), ret);
  }
  catch (const no_such_query&) {
  }
  return boost::any_cast<QueryType>(ret);
}

} // xrt_core

#endif
