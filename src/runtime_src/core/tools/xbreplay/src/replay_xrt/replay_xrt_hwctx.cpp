// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "replay_xrt.hpp"

namespace xrt_core::tools::xbreplay {

static bool deserialize_map_data(const std::vector<char>& buffer, xrt::hw_context::cfg_param_type& result) {
  size_t i = 0;
  while (i < buffer.size())
  {
    // Extract key size
    if (i + sizeof(uint32_t) > buffer.size())
      return false;

    uint32_t key_size = 0;
    std::memcpy(&key_size, &buffer[i], sizeof(uint32_t));
    i += sizeof(uint32_t);

    // Extract key
    if (i + key_size > buffer.size())
      return false; // Not enough data for key

    std::string key(&buffer[i], &buffer[i + key_size]);
    i += key_size;

    // Extract value
    if (i + sizeof(uint32_t) > buffer.size())
      return false; // Not enough data for value

    uint32_t value = 0;
    std::memcpy(&value, &buffer[i], sizeof(uint32_t));
    i += sizeof(uint32_t);

    // Insert into map
    result[key] = value;
  }
  return true;
}

/**
 * Replay maintains a map where each member function of the XRT classes is associated
 * with a corresponding lambda function. This API adds a lambda function entry for each
 * corresponding member of the xrt::hw_context class.
 */
void replay_xrt::register_hwctxt_class_func()
{
  m_api_map["xrt::hw_context::hw_context(const xrt::device&, const xrt::uuid&, xrt::hw_context::access_mode)"] =
  [this](std::shared_ptr<utils::message> msg)
  {
    const std::vector<std::pair<std::string, std::string>>& args = msg->m_args;

    auto dev_ref = std::stoull(args[0].second, nullptr, utils::base_hex);
    auto acc_md = static_cast<xrt::hw_context::access_mode>(std::stoul(args[2].second));
    const std::string& uuid_str = args[1].second;
    xrt::uuid input_uid(uuid_str);

    auto dev = m_device_hndle_map[dev_ref];

    if (dev != nullptr)
    {
      auto hwctxt_hdl =
          std::make_shared<xrt::hw_context>(*dev, input_uid, acc_md);

      m_hwctx_hndle_map[msg->m_handle] = hwctxt_hdl;
    }
    else
      throw std::runtime_error("Failed to get Dev Handle ");
  };

m_api_map["xrt::hw_context::hw_context(const xrt::device&, const xrt::uuid&, const xrt::hw_context::cfg_param_type&)"] =
  [this](std::shared_ptr<utils::message> msg)
  {
    const std::vector<std::pair<std::string, std::string>>& args = msg->m_args;

    auto dev_ref = std::stoull(args[0].second, nullptr, utils::base_hex);
    const std::string& uuid_str = args[1].second;
    xrt::uuid input_uid(uuid_str);
    auto dev = m_device_hndle_map[dev_ref];
    const std::string& mem_tag = args[2].second;
    std::vector<char> data = {};
    msg->get_user_data(mem_tag, &data);

    xrt::hw_context::cfg_param_type cfg_param;

    if ((dev != nullptr) && (deserialize_map_data(data, cfg_param)))
    {
      auto hwctxt_hdl =
          std::make_shared<xrt::hw_context>(*dev, input_uid, cfg_param);

      m_hwctx_hndle_map[msg->m_handle] = hwctxt_hdl;
    }
    else
      throw std::runtime_error("Failed to get Dev Handle ");
  };

m_api_map["xrt::hw_context::update_qos(const xrt::hw_context::cfg_param_type&)"] =
[this](std::shared_ptr<utils::message> msg)
  {
    const std::vector<std::pair<std::string, std::string>>& args = msg->m_args;

    const std::string& mem_tag = args[0].second;
    std::vector<char> data = {};
    msg->get_user_data(mem_tag, &data);

    xrt::hw_context::cfg_param_type cfg_param;
    auto hwctxt_hdl = m_hwctx_hndle_map[msg->m_handle];
    if ((hwctxt_hdl != nullptr) && (deserialize_map_data(data, cfg_param)))
      hwctxt_hdl->update_qos(cfg_param);
    else
      throw std::runtime_error("Failed to get hw_context Handle");
  };

m_api_map["xrt::hw_context::~hw_context()"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    auto ptr = m_hwctx_hndle_map[msg->m_handle];

    if (ptr)
      m_hwctx_hndle_map.erase(msg->m_handle);
    else
      throw std::runtime_error("Failed to get hwctxt_hdl handle");
  };
}
}// end of namespace

