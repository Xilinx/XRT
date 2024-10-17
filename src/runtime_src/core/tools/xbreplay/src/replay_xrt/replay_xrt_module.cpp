// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "replay_xrt.hpp"

namespace xrt_core::tools::xbreplay {

/**
 * Replay maintains a map where each member function of the XRT classes is associated
 * with a corresponding lambda function. This API adds a lambda function entry for each
 * corresponding member of the xrt::module class.
 */
void replay_xrt::register_module_class_func()
{
  m_api_map["xrt::module::module(const xrt::elf&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const auto& args = msg->m_args;
    if (args.size() > 0)
    {
      auto elf_ref = std::stoll(args[0].second);
      auto elf_hdl = m_elf_hndle_map[elf_ref];
      if (elf_hdl)
        m_module_hndle_map[msg->m_handle] = std::make_shared<xrt::module>(*elf_hdl);
      else
        throw std::runtime_error("Failed to get elf handle");
    }
    else
      throw std::runtime_error("Missing arguments for xrt::module(const xrt::elf&)");
  };

  m_api_map["xrt::module::module(const xrt::module&, const xrt::hw_context&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    try
    {
      const auto& args = msg->m_args;

      // Check if arguments are valid
      if (!args.empty())
      {
        auto parent = m_module_hndle_map[std::stoull(args[0].second)];
        auto hwctx = m_hwctx_hndle_map[std::stoull(args[1].second)];

        if (parent && hwctx)
          m_module_hndle_map[msg->m_handle] = std::make_shared<xrt::module>(*parent, *hwctx);
        else
          throw std::runtime_error("Failed to get module handle or hwctx handle");
      }
      else
        throw std::runtime_error("Invalid arguments provided for xrt::module constructor with axlf*");
    }
    catch (const std::exception& ex)
    {
      throw std::runtime_error("Exception while creating xrt::module " + std::string(ex.what()));
    }
  };

  m_api_map["xrt::module::~module()"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    auto ptr = m_module_hndle_map[msg->m_handle];

    if (ptr)
      m_module_hndle_map.erase(msg->m_handle);
    else
      throw std::runtime_error("Failed to get module handle");
  };
}

}// end of namespace
