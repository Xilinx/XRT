// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "replay_xrt.hpp"

namespace xrt_core::tools::xbreplay {

/**
 * Replay maintains a map where each member function of the XRT classes is associated
 * with a corresponding lambda function. This API adds a lambda function entry for each
 * corresponding member of the xrt::device class.
 */
void replay_xrt::register_device_class_func()
{
  m_api_map["xrt::device::device(unsigned int)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;
    uint32_t device_index = std::stoi(args[0].second);
    m_device_hndle_map[msg->m_handle] = std::make_shared<xrt::device>(device_index);
  };

  m_api_map["xrt::device::device(const std::string&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;
    std::string bdf = args[0].second;
    m_device_hndle_map[msg->m_handle] = std::make_shared<xrt::device>(bdf);
  };

  m_api_map["xrt::device::device(xclDeviceHandle)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    auto xcl_ref = std::stoull(args[0].second);
    auto xcldev_hdl = m_xcldev_hndle_map[xcl_ref];

    if (xcldev_hdl)
      m_device_hndle_map[msg->m_handle] = std::make_shared<xrt::device>(xcldev_hdl.get());
    else
      throw std::runtime_error("xcldev Handle not found:" + hex_str(msg->m_handle));
  };

  m_api_map["xrt::device::load_xclbin(const std::string&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    std::string fp = get_file_path(msg, ".xclbin");

    /*get device handle */
    auto dev = m_device_hndle_map[msg->m_handle];
    if (dev)
    {
      XBREPLAY_INFO("LOAD XCLBIN PATH", fp);
      m_uuid_device_map[dev] = dev->load_xclbin(fp);
    }
    else
      throw std::runtime_error("dev handle not found:" + hex_str(msg->m_handle));
  };

  m_api_map["xrt::device::load_xclbin(const axlf*)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;
    auto axlf_ref = std::stoull(args[0].second);

    /*get axlf handle */
    auto axlf_hndle = m_axlf_hndle_map[axlf_ref];

    /*get device handle */
    auto dev = m_device_hndle_map[msg->m_handle];

    if (dev && axlf_hndle)
      m_uuid_device_map[dev] = dev->load_xclbin(axlf_hndle.get());
    else
      throw std::runtime_error("dev/axlf_hndle handle not found: " + hex_str(msg->m_handle));
  };

  m_api_map["xrt::device::load_xclbin(const xrt::xclbin&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;
    auto xclbin_ref = std::stoull(args[0].second);

    /*get axlf handle */
    auto xclbin_hndle = m_xclbin_hndle_map[xclbin_ref];

    /*get device handle */
    auto dev = m_device_hndle_map[msg->m_handle];

    if (dev && xclbin_hndle)
      m_uuid_device_map[dev] = dev->load_xclbin(*xclbin_hndle);
    else
      throw std::runtime_error("dev/xclbin_hndle handle devmap:" + hex_str(msg->m_handle) +
                              " xclmap:" + hex_str(xclbin_ref));
  };

  m_api_map["xrt::device::register_xclbin(const xrt::xclbin&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;
    uint64_t  xclbin_ref = std::stoull(args[0].second, nullptr, utils::base_hex);

    /*get axlf handle */
    auto xclbin_hndle = m_xclbin_hndle_map[xclbin_ref];

    /*get device handle */
    auto dev = m_device_hndle_map[msg->m_handle];

    if (dev && xclbin_hndle)
      m_uuid_device_map[dev] = dev->register_xclbin(*xclbin_hndle);
    else
      throw std::runtime_error("Failed to get dev/xclbin_hndle handle" +
                  hex_str(msg->m_handle) + " xclmap:" + hex_str(xclbin_ref));
  };

  m_api_map["xrt::device::reset()"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    /*get device handle */
    auto dev = m_device_hndle_map[msg->m_handle];

    if (dev)
      dev->reset();
    else
      throw std::runtime_error("dev handle not found:" + hex_str(msg->m_handle));
  };

  m_api_map["xrt::device::~device()"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    auto ptr = m_device_hndle_map[msg->m_handle];

    if (ptr)
      m_device_hndle_map.erase(msg->m_handle);
    else
      throw std::runtime_error("Failed to get device handle");
  };
}

}// end of namespace
