// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "replay_xrt.hpp"

namespace xrt_core::tools::xbreplay {

/**
 * Replay maintains a map where each member function of the XRT classes is associated
 * with a corresponding lambda function. This API adds a lambda function entry for each
 * corresponding member of the xrt::bo class.
 */
void replay_xrt::register_bo_class_func()
{
  m_api_map["xrt::bo::bo(const xrt::device&, void*, size_t, xrt::bo::flags, xrt::memory_group)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    /* get dev handle */
    auto dev_handle = std::stoull(args[0].second, nullptr, utils::base_hex);

    /* From handle obtained from log get the corresponding device
     * handle from map
     */
    std::shared_ptr<xrt::device> dev = m_device_hndle_map[dev_handle];

    if (dev)
    {
      /*
       * clang warning: avoid using reinterpret_cast
       * The cast is necessary and correct for the application's requirements
       * we tried to break down the conversion as shown below but still we are getting warning
       * Ex: unsigned long addr = std::stoul(args[1].second);
       *     std::uintptr_t uintptr_addr = static_cast<std::uintptr_t>(addr);
       *      void* user_ptr = reinterpret_cast<void*>(uintptr_addr);
       */
      auto user_ptr = reinterpret_cast<void*>(std::stoull(args[1].second)); //NOLINT
      size_t sz = std::stoi(args[2].second);
      auto bo_flags = static_cast<xrt::bo::flags>(std::stoul(args[3].second));
      auto mgrp = std::stoi(args[4].second);
      auto mgroup = m_kernel_grp_id[mgrp];

      m_bo_hndle_map[msg->m_handle] =
              std::make_shared<xrt::bo>(*dev, user_ptr, sz, bo_flags, mgroup);
    }
    else
      throw std::runtime_error("failed to get dev handle");
  };

  m_api_map["xrt::bo::bo(const xrt::device&, void*, size_t, xrt::memory_group)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    /* get dev handle */
    auto dev_handle = std::stoull(args[0].second, nullptr, utils::base_hex);

    /* From handle obtained from log get the corresponding device
     * handle from map
     */
    auto dev = m_device_hndle_map[dev_handle];

    if (dev)
    {
      auto user_ptr = std::stoull(args[1].second);
      size_t sz = std::stoi(args[2].second);
      auto mgrp = std::stoi(args[3].second);
      auto mgroup = m_kernel_grp_id[mgrp];

      m_bo_hndle_map[msg->m_handle] =
                std::make_shared<xrt::bo>(*dev, user_ptr, sz, mgroup);
    }
    else
      throw std::runtime_error( "failed to get dev handle");
  };

  m_api_map["xrt::bo::bo(const xrt::device&, size_t, xrt::bo::flags, xrt::memory_group)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    /* get dev handle */
    auto dev_handle = std::stoull(args[0].second,nullptr, utils::base_hex);

    /* From handle obtained from log get the corresponding device
     * handle from map
     */
    std::shared_ptr<xrt::device> dev = m_device_hndle_map[dev_handle];

    if (dev)
    {
      size_t sz = std::stoi(args[1].second);
      auto bo_flags = static_cast<xrt::bo::flags>(std::stoi(args[2].second));
      auto mgrp = std::stoi(args[3].second);
      auto mgroup = m_kernel_grp_id[mgrp];
      m_bo_hndle_map[msg->m_handle] =
                std::make_shared<xrt::bo>(*dev, sz, bo_flags, mgroup);
    }
    else
      throw std::runtime_error("failed to get dev handle");
  };

  m_api_map["xrt::bo::bo(const xrt::device&, size_t, xrt::memory_group)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    /* get dev handle */
    auto dev_handle = std::stoull(args[0].second,nullptr, utils::base_hex);

    /* From handle obtained from log get the corresponding device
     * handle from map
     */
    std::shared_ptr<xrt::device> dev = m_device_hndle_map[dev_handle];

    if (dev)
    {
      size_t sz = std::stoi(args[1].second);
      auto mgrp = std::stoi(args[2].second);
      auto mgroup = m_kernel_grp_id[mgrp];
      m_bo_hndle_map[msg->m_handle] = std::make_shared<xrt::bo>(*dev,sz, mgroup);
    }
    else
      throw std::runtime_error("failed to get dev handle");
  };

  m_api_map["xrt::bo::bo(const xrt::hw_context&, void*, size_t, xrt::memory_group)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    /* get hwctx handle */
    auto hwctx_hdl = std::stoull(args[0].second, nullptr, utils::base_hex);

    /* From handle obtained from log get the corresponding device
     * handle from map
     */
    auto hw_ctx = m_hwctx_hndle_map[hwctx_hdl];

    if (hw_ctx)
    {
      /*
       * clang warning: avoid using reinterpret_cast
       * The cast is necessary and correct for the application's requirements
       * we tried to break down the conversion as shown below but still
       * we are getting warning
       *  Ex: unsigned long addr = std::stoul(args[1].second);
       *     std::uintptr_t uintptr_addr = static_cast<std::uintptr_t>(addr);
       *      void* user_ptr = reinterpret_cast<void*>(uintptr_addr);
       */
      auto  user_ptr = reinterpret_cast<void*>(std::stoull(args[1].second)); //NOLINT
      size_t sz = std::stoi(args[2].second);
      auto mgrp = std::stoi(args[3].second);
      auto mgroup = m_kernel_grp_id[mgrp];
      m_bo_hndle_map[msg->m_handle] =
                 std::make_shared<xrt::bo>(*hw_ctx, user_ptr, sz, mgroup);
    }
    else
      throw std::runtime_error("failed to get hw_ctx handle");
  };

  m_api_map["xrt::bo::bo(const xrt::hw_context&, size_t, xrt::bo::flags, xrt::memory_group)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    /* get hwctx handle */
    auto hwctx_hdl = std::stoull(args[0].second, nullptr, utils::base_hex);

    /* From handle obtained from log get the corresponding hw_context
     * handle from map
     */
    std::shared_ptr<xrt::hw_context> hw_ctx = m_hwctx_hndle_map[hwctx_hdl];

    if (hw_ctx)
    {
      size_t sz = std::stoi(args[1].second);
      auto bo_flags = static_cast<xrt::bo::flags>(std::stoi(args[2].second));
      auto mgrp = std::stoi(args[3].second);
      auto mgroup = m_kernel_grp_id[mgrp];
      m_bo_hndle_map[msg->m_handle] =
                std::make_shared<xrt::bo>(*hw_ctx, sz, bo_flags, mgroup);
    }
    else
      throw std::runtime_error("failed to get hw_ctx handle");
  };

  m_api_map["xrt::bo::bo(const xrt::hw_context&, size_t, xrt::memory_group)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    /* get hwctx handle */
    auto hwctx_hdl = std::stoull(args[0].second, nullptr, utils::base_hex);

    /* From handle obtained from log get the corresponding hw_ctxice
     * handle from map
     */
    auto hw_ctx = m_hwctx_hndle_map[hwctx_hdl];

    if (hw_ctx)
    {
      size_t sz = std::stoi(args[1].second);
      auto mgrp = std::stoi(args[2].second);
      auto mgroup = m_kernel_grp_id[mgrp];
      m_bo_hndle_map[msg->m_handle] = std::make_shared<xrt::bo>(*hw_ctx, sz, mgroup);
    }
    else
      throw std::runtime_error("failed to get hw_ctx handle");
  };

  m_api_map["xrt::bo::bo(const xrt::bo&, size_t, size_t)"] =
  [this](std::shared_ptr<utils::message> msg)
  {
    const std::vector<std::pair<std::string, std::string>> &args = msg->m_args;

    /* get hwctx handle */
    auto bo_hdl = std::stoull(args[0].second, nullptr, utils::base_hex);
    auto pbo = m_bo_hndle_map[bo_hdl];
    if (pbo)
    {
      auto size = std::stoull(args[1].second);
      auto offset = std::stoull(args[2].second);
      m_bo_hndle_map[msg->m_handle] = std::make_shared<xrt::bo>(*pbo, size, offset);
    }
    else
      throw std::runtime_error("failed to get pbo handle");
  };

  m_api_map["xrt::bo::async(xclBOSyncDirection, size_t, size_t)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    /* From handle obtained from log get the corresponding device
     * handle from map
     */
    auto pbo = m_bo_hndle_map[msg->m_handle];
    auto sync_dir = static_cast<xclBOSyncDirection>(std::stoi (args[0].second));
    auto size = static_cast<uint64_t>(std::stoull (args[1].second));
    auto offset = static_cast<uint64_t>(std::stoull (args[2].second));

    if (pbo)
    {
    /*
     * clang warning: avoid using reinterpret_cast
     * The cast is necessary and correct for the application's requirements
     * we tried to break down the conversion but still we are getting warning
     */
    auto *bo_map = reinterpret_cast<uint64_t*>(pbo->map()); //NOLINT

      /* Special handling for sync to Device */
      if (XCL_BO_SYNC_BO_TO_DEVICE == sync_dir)
      {
        /* Check if memory buffer data is available */
        if (msg->m_is_mem_file_available && msg->m_buf.size())
        {
          if (bo_map)
          {
            std::memcpy(bo_map, msg->m_buf.data(), msg->m_buf.size());
            pbo->async(XCL_BO_SYNC_BO_TO_DEVICE, size, offset);
          }
          else
            throw std::runtime_error("failed to get bo_map handle");
        }
        else
          throw std::runtime_error("buffer data not available");

      }
      else
        pbo->async(sync_dir, size, offset);
    }
    else
        throw std::runtime_error("failed to get pbo handle");

  };

  m_api_map["xrt::bo::sync(xclBOSyncDirection, size_t, size_t)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    /* From handle obtained from log get the corresponding device
     * handle from map
     */
    std::shared_ptr<xrt::bo> pbo = m_bo_hndle_map[msg->m_handle];

    auto sync_dir = static_cast<xclBOSyncDirection>(std::stoi (args[0].second));

    auto size = static_cast<uint64_t>(std::stoull (args[1].second));
    auto offset = static_cast<uint64_t>(std::stoull (args[2].second));

    if (pbo)
    {
      /*
       * clang warning: avoid using reinterpret_cast
       * The cast is necessary and correct for the application's requirements
       * we tried to break down the conversion but still we are getting warning
       */
      auto bo_map = pbo->map<char*>(); //NOLINT

      /* Special handling for sync to Device */
      if (XCL_BO_SYNC_BO_TO_DEVICE == sync_dir)
      {
        /* Check if memory buffer data is available */
        if (msg->m_is_mem_file_available && msg->m_buf.size())
        {
          if (bo_map)
          {
            std::memcpy(bo_map, msg->m_buf.data(), msg->m_buf.size());
            pbo->sync(XCL_BO_SYNC_BO_TO_DEVICE, size, offset);
          }
          else
            throw std::runtime_error("failed to get bo_map handle");
        }
        else
          throw std::runtime_error("data not available for sync");
      }
      else
        pbo->sync(sync_dir, size, offset);
    }
    else
      throw std::runtime_error("failed to get pbo handle");
  };

  m_api_map["ext::bo::bo(constxrt::hw_context&, size_t, xrt::ext::bo::access_mode)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    /* get hwctx handle */
    auto hwctx_hdl = std::stoull(args[0].second, nullptr, utils::base_hex);

    /* From handle obtained from log get the corresponding hw_ctxice
     * handle from map
     */
    auto hw_ctx = m_hwctx_hndle_map[hwctx_hdl];

    if (hw_ctx)
    {
      size_t sz = std::stoi(args[1].second);
      auto acc_mode = static_cast<xrt::ext::bo::access_mode>(stol(args[2].second));

      m_bo_hndle_map[msg->m_handle] = std::make_shared<xrt::ext::bo>(*hw_ctx, sz, acc_mode);
    }
    else
      throw std::runtime_error("failed to get hw_ctx handle");
  };

  m_api_map["xrt::bo::~bo()"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    auto ptr = m_bo_hndle_map[msg->m_handle];

    if (ptr)
      m_bo_hndle_map.erase(msg->m_handle);
    else
      throw std::runtime_error("failed to get bo handle");
  };
}

}// end of namespace
