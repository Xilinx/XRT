// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "replay_xrt.hpp"

namespace xrt_core::tools::xbreplay {

/**
 * Replay maintains a map where each member function of the XRT classes is associated
 * with a corresponding lambda function. This API adds a lambda function entry for each
 * corresponding member of the xrt::run class.
 */
void replay_xrt::register_run_class_func()
{
  m_api_map["xrt::run::run(const xrt::kernel&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;
    auto krnl_hndle = std::stoull(args[0].second, nullptr, utils::base_hex);
    auto pkern = m_kernel_hndle_map[krnl_hndle];

    if (pkern)
    {
      const xrt::kernel& krnl = *pkern;
      auto run_hdl = std::make_shared<xrt::run>(krnl);
      m_run_hndle_map[msg->m_ret_val] = run_hdl;
    }
    else
      throw std::runtime_error("Failed to get kernal handle");
  };

  m_api_map["xrt::run::set_arg_at_index(int, const xrt::bo&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    auto run_ptr = m_run_hndle_map[msg->m_handle];
    auto index = std::stoi(args[0].second);

    auto bo_hndle = std::stoull(args[1].second, nullptr, utils::base_hex);
    auto bo_ptr = m_bo_hndle_map[bo_hndle];

    if (run_ptr && bo_ptr)
      run_ptr->set_arg(index, *bo_ptr.get());
    else
      throw std::runtime_error("Failed to get run/bo handle");
  };

  m_api_map["xrt::run::set_arg_at_index(int, const void*, size_t)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;
    auto run_ptr = m_run_hndle_map[msg->m_handle];

    int index = std::stoi(args[0].second);
    int size  = std::stoi(args[1].second);
    auto bytes = static_cast<size_t>(std::stoul(args[2].second));

    if (run_ptr)
    {
      const void *temp = (void*) &size;
      run_ptr->set_arg(index, temp, bytes);
    }
    else
      throw std::runtime_error("Failed to get handle");
  };

  m_api_map["xrt::run::start()"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    auto run_ptr = m_run_hndle_map[msg->m_handle];

    if (run_ptr)
      run_ptr->start();
    else
      throw std::runtime_error("Failed to get run handle");
  };

  m_api_map["xrt::run::wait(const std::chrono::milliseconds&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    auto run_ptr = m_run_hndle_map[msg->m_handle];
    auto time = std::stoll(args[0].second, nullptr, utils::base_hex);
    const std::chrono::milliseconds timeout(time);
    if (run_ptr)
      run_ptr->wait(timeout);
    else
      throw std::runtime_error("Failed to get run handle");
  };

  m_api_map["xrt::run::wait2(const std::chrono::milliseconds&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;
    auto run_ptr = m_run_hndle_map[msg->m_handle];
    auto time = std::stoll(args[0].second);
    const std::chrono::milliseconds timeout(time);
    if (run_ptr)
      run_ptr->wait2(timeout);
    else
      throw std::runtime_error("Failed to get run handle");
  };

  m_api_map["xrt::run::stop()"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    //const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;
    auto run_ptr = m_run_hndle_map[msg->m_handle];
    if (run_ptr)
      run_ptr->stop();
    else
      throw std::runtime_error("Failed to get run handle");
  };

  m_api_map["xrt::run::abort()"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    //const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;
    auto run_ptr = m_run_hndle_map[msg->m_handle];
    if (run_ptr)
      run_ptr->abort();
    else
      throw std::runtime_error("Failed to get run handle");
  };

  m_api_map["xrt::run::update_arg_at_index(int, const void*, size_t)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;
    auto run_ptr = m_run_hndle_map[msg->m_handle];

    int index = std::stoi(args[0].second);
    auto value = std::stol(args[1].second);
    auto bytes = static_cast<size_t>(std::stoul(args[2].second));

    if (run_ptr)
    {
      run_ptr->update_arg(index, &value, bytes);
    }
    else
      throw std::runtime_error("Failed to get run handle");
  };

  m_api_map["xrt::run::update_arg_at_index(int, const xrt::bo&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    auto run_ptr = m_run_hndle_map[msg->m_handle];
    auto index = std::stoi(args[0].second);

    auto bo_hndle = std::stoull(args[1].second, nullptr, utils::base_hex);
    auto bo_ptr = m_bo_hndle_map[bo_hndle];

    if (run_ptr && bo_ptr)
      run_ptr->update_arg(index, *bo_ptr.get());
    else
      throw std::runtime_error("Failed to get run/bo handle");
  };

  m_api_map["xrt::run::~run()"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    auto ptr = m_run_hndle_map[msg->m_handle];

    if (ptr)
      m_run_hndle_map.erase(msg->m_handle);
    else
      throw std::runtime_error("Failed to get run handle");
  };
}
}
