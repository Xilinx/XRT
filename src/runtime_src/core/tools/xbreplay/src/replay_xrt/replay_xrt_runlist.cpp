// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "replay_xrt.hpp"

namespace xrt_core::tools::xbreplay {

/**
 * Replay maintains a map where each member function of the XRT classes is associated
 * with a corresponding lambda function. This API adds a lambda function entry for each
 * corresponding member of the xrt::runlist class.
 */
void replay_xrt::register_runlist_class_func()
{
  m_api_map["xrt::runlist::runlist(const xrt::hw_context&)"] =
  [this] (std::shared_ptr<utils::message> msg)
  {
    const std::vector<std::pair<std::string, std::string>>& args = msg->m_args;
    auto hwctx_handle = std::stoull(args[0].second, nullptr, utils::base_hex);
    auto phwctx = m_hwctx_hndle_map[hwctx_handle];

    if (phwctx)
    {
      const xrt::hw_context& hwctx = *phwctx;
      auto runlist_ptr = std::make_shared<xrt::runlist>(hwctx);
      m_runlist_hndle_map[msg->m_ret_val] = runlist_ptr;
    }
    else
      throw std::runtime_error("Failed to get hardware context handle");
  };

  m_api_map["xrt::runlist::add(const xrt::run&)"] =
  [this] (std::shared_ptr<utils::message> msg)
  {
    const std::vector<std::pair<std::string, std::string>>& args = msg->m_args;
    auto runlist_handle = std::stoull(args[0].second, nullptr, utils::base_hex);
    auto run_handle = std::stoull(args[1].second, nullptr, utils::base_hex);

    auto runlist_ptr = m_runlist_hndle_map[runlist_handle];
    auto run_ptr = m_run_hndle_map[run_handle];

    if (runlist_ptr && run_ptr)
    {
      const xrt::run& run = *run_ptr;
      runlist_ptr->add(run);
    }
    else
      throw std::runtime_error("Failed to get runlist or run handle");
  };

  m_api_map["xrt::runlist::execute()"] =
  [this] (std::shared_ptr<utils::message> msg)
  {
    auto runlist_ptr = m_runlist_hndle_map[msg->m_handle];

    if (runlist_ptr)
      runlist_ptr->execute();
    else
      throw std::runtime_error("Failed to get runlist handle");
  };

  m_api_map["xrt::runlist::wait(const std::chrono::milliseconds&)"] =
  [this] (std::shared_ptr<utils::message> msg)
  {
    const std::vector<std::pair<std::string, std::string>>& args = msg->m_args;
    auto runlist_ptr = m_runlist_hndle_map[msg->m_handle];
    auto time = std::stoll(args[0].second);
    const std::chrono::milliseconds timeout(time);
    
    if (runlist_ptr)
      runlist_ptr->wait(timeout);
    else
      throw std::runtime_error("Failed to get runlist handle");
  };

  m_api_map["xrt::runlist::reset()"] =
  [this] (std::shared_ptr<utils::message> msg)
  {
    auto runlist_ptr = m_runlist_hndle_map[msg->m_handle];

    if (runlist_ptr)
      runlist_ptr->reset();
    else
      throw std::runtime_error("Failed to get runlist handle");
  };
}
}
