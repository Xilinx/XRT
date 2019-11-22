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

// Please keep eternal include file dependencies to a minimum
#include <boost/property_tree/ptree.hpp>
#include <cstdint>
#include <vector>
#include <string>
#include <list>
#include <map>
#include <boost/any.hpp>

namespace xrt_core {

class device_core {

  public:
    /**
     * Returns the class handle use to query the abstract 
     * libraries 
     * 
     * @return The handle instance
     */
    static const device_core & get_handle();

  public:
    virtual void get_devices(boost::property_tree::ptree &_pt) const = 0;
    virtual void get_device_info(uint64_t _deviceID, boost::property_tree::ptree &_pt) const = 0;
    virtual void read_device_dma_stats(uint64_t _deviceID, boost::property_tree::ptree &_pt) const = 0;
    virtual uint64_t get_total_devices() const = 0;

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
    // Future TODO: Move a way from static enumeration to dynamic enumeration
    typedef enum {
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
      QR_POWER_MICROWATTS
    } QueryRequest;

    virtual void query_device(uint64_t _deviceID, QueryRequest _eQueryRequest, const std::type_info & _typeInfo, boost::any &_returnValue) const = 0;

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

device_core*
initialize_child_ctor();

} // xrt_core

#endif 
