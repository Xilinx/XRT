// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "replay_xrt.hpp"

namespace xrt_core::tools::xbreplay {

/**
 * Replay maintains a map where each member function of the XRT classes is associated
 * with a corresponding lambda function. This API adds a lambda function entry for each
 * corresponding member of the xrt::xclbin class.
 */
void replay_xrt::register_xclbin_class_func()
{
  m_api_map["xrt::xclbin::xclbin(const std::string&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const auto& args = msg->m_args;
    if (args.size() > 0)
    {
      std::string str_xclbin = get_file_path(msg, ".xclbin");
      auto xclbin_hdl = std::make_shared<xrt::xclbin>(str_xclbin);
      m_xclbin_hndle_map[msg->m_handle] = xclbin_hdl;
    }
    else
      throw std::runtime_error("Missing arguments for xrt::xclbin(const string&)\n");
  };

  m_api_map["xrt::xclbin::xclbin(const std::vector<char>&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    try
    {
      const auto& args = msg->m_args;

      // Check if arguments are valid
      if (!args.empty() && !args[0].second.empty())
      {
        // Ensure buffer is not empty
        if (msg->m_buf.empty())
          throw std::runtime_error("Buffer is empty for xrt::xclbin constructor with std::vector<char>.\n");

        // Create a std::vector<char> with the size of the buffer;
        std::vector<char> data(msg->m_buf.size());
        std::memcpy(data.data(), msg->m_buf.data(), msg->m_buf.size());

        // Create xrt::xclbin instance with the vector of chars
        auto xclbin_hdl = std::make_shared<xrt::xclbin>(data);

        // Store the handle in the map
        m_xclbin_hndle_map[msg->m_handle] = xclbin_hdl;
      }
      else
        throw std::runtime_error("Invalid arguments provided for xrt::xclbin(const std::vector<char>&).\n");
    }
    catch (const std::exception& ex)
    {
      throw std::runtime_error("Exception while creating xrt::xclbin with std::vector<char>: " + std::string(ex.what()) + "\n");
    }
  };

  m_api_map["xrt::xclbin::xclbin(const axlf*)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    try
    {
      const auto& args = msg->m_args;

      // Check if arguments are valid
      if (!args.empty() && !args[0].second.empty())
      {
        try
        {
          // Define the size of axlf object
          constexpr std::size_t axlf_size = sizeof(axlf);

          if (msg->m_buf.size() != axlf_size)
          {
            throw std::runtime_error("Buffer size too small for axlf object.\n");
          }

          // Create axlf object and copy data from buffer
          auto maxlf = std::make_shared<axlf>();
          std::memcpy(maxlf.get(), msg->m_buf.data(), axlf_size);

          // Create xrt::xclbin instance with axlf object
          auto xclbin_hdl = std::make_shared<xrt::xclbin>(maxlf.get());

          // Store the handle in the map
          m_xclbin_hndle_map[msg->m_handle] = xclbin_hdl;
        }
        catch (const std::exception& e)
        {
          throw std::runtime_error("Exception while creating xrt::xclbin with axlf*: " + std::string(e.what()) + "\n");
        }
      }
      else
        throw std::runtime_error("Invalid arguments provided for xrt::xclbin constructor with axlf*.\n");
    }
    catch (const std::exception& ex)
    {
      throw std::runtime_error("Exception while creating xrt::xclbin with axlf*: " + std::string(ex.what()) + "\n");
    }
  };

  m_api_map["xrt::xclbin::~xclbin()"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    auto ptr = m_xclbin_hndle_map[msg->m_handle];

    if (ptr)
      m_xclbin_hndle_map.erase(msg->m_handle);
    else
      throw std::runtime_error("Failed to get xclbin handle");
  };
}

}// end of namespace
