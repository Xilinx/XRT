// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "replay_xrt.hpp"

namespace xrt_core::tools::xbreplay {

/**
 * Replay maintains a map where each member function of the XRT classes is associated
 * with a corresponding lambda function. This API adds a lambda function entry for each
 * corresponding member of the xrt::kernel class.
 */
void replay_xrt::register_kernel_class_func()
{
 m_api_map["xrt::kernel::kernel(const xrt::device&, const xrt::uuid&, "
          "const std::string&, xrt::kernel::cu_access_mode)"] =
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
      /* get correspoding uuid from dev */
      auto  uuid = m_uuid_device_map[dev];
      std::string str = args[2].second;
      auto mode  =
        static_cast<xrt::kernel::cu_access_mode>(std::stoul(args[3].second));
      m_kernel_hndle_map[msg->m_handle] =
                      std::make_shared<xrt::kernel>(*dev, uuid, str, mode);
    }
    else
      throw std::runtime_error("failed to get dev handle");
  };

  m_api_map["xrt::kernel::kernel(const xrt::hw_context&, const std::string&)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    /* get dev handle */
    auto hwctx_handle = std::stoull(args[0].second, nullptr, utils::base_hex);

    /* From handle obtained from log get the corresponding device
     * handle from map
     */
    auto hwctx = m_hwctx_hndle_map[hwctx_handle];

    if (nullptr != hwctx)
    {
      const std::string str = args[1].second;
      m_kernel_hndle_map[msg->m_handle] = std::make_shared<xrt::kernel>(*hwctx, str);
    }
    else
      throw std::runtime_error("failed to get kernel handle" + hex_str(hwctx_handle));
  };

 m_api_map["xrt::kernel::group_id(int)"] =
 [this] (std::shared_ptr<utils::message>msg)
 {
   const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

   /* From handle obtained from log get the corresponding device
    * handle from map
    */
   auto krnl_hdl = m_kernel_hndle_map[msg->m_handle];

   if (krnl_hdl)
   {
     auto arg = std::stoi(args[0].second);
     auto grp_id = krnl_hdl->group_id(arg);
     m_kernel_grp_id[msg->m_ret_val]= grp_id;
   }
   else
     throw std::runtime_error("failed to get kernel handle"+ hex_str(msg->m_handle));
 };

/*TBD: API write_register is deprecated to be removed */
#if 0
  m_api_map["xrt::kernel::write_register(unsigned int, unsigned int)"] =
  [this] (std::shared_ptr<utils::message>msg)
  {

    const std::vector <std::pair<std::string, std::string>> &args = msg->m_args;

    /* From handle obtained from log get the corresponding device
     * handle from map
     */
    auto krnl_hdl = m_kernel_hndle_map[msg->m_handle];

    if (krnl_hdl)
    {
      auto offset  = std::stoul(args[0].second);
      auto data  = std::stoul(args[1].second);
      krnl_hdl->write_register(offset, data);
    }
    else
    {
      throw std::runtime_error("failed to get kernel handle", hex_str(msg->m_handle));
    }
  };
#endif

  m_api_map["xrt::kernel::~kernel()"] =
  [this] (std::shared_ptr<utils::message>msg)
  {
    auto ptr = m_kernel_hndle_map[msg->m_handle];

    if (ptr)
      m_kernel_hndle_map.erase(msg->m_handle);
    else
      throw std::runtime_error("Failed to get kernel handle");
  };
}
}// end of namespace

