// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "replay_xrt.hpp"

namespace xrt_core::tools::xbreplay {

/**
 * Replay maintains a map where each member function of the XRT classes is associated
 * with a corresponding lambda function. This API adds a lambda function entry for each
 * corresponding member of the xrt::elf class.
 */
void replay_xrt::register_elf_class_func()
{
  m_api_map["xrt::elf::elf(const std::string&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    if (msg->m_args.size() > 0)
    {
      std::string elf_data = get_file_path(msg, "");
      auto elf_hdl = std::make_shared<xrt::elf>(elf_data);
      m_elf_hndle_map[msg->m_handle] = elf_hdl;
    }
    else
      throw std::runtime_error("Missing arguments for xrt::elf(const xrt::elf&)");
  };

  m_api_map["xrt::elf::elf(std::istream&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    if (msg->m_args.size() > 0)
    {
      std::string elf_data = get_file_path(msg, "");
      std::ifstream file(elf_data, std::ios::binary);
      auto elf_hdl = std::make_shared<xrt::elf>(file);
      m_elf_hndle_map[msg->m_handle] = elf_hdl;
    }
    else
      throw std::runtime_error("Missing arguments for xrt::elf(const xrt::elf&)");
  };

  m_api_map["xrt::elf::~elf()"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    auto ptr = m_elf_hndle_map[msg->m_handle];

    if (ptr)
      m_elf_hndle_map.erase(msg->m_handle);
    else
      throw std::runtime_error("Failed to get elf handle");
  };
}

}// end of namespace
