// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/experimental/xrt_xclbin.h"
#include "xrt/experimental/xrt_ext.h"
#include "xrt/deprecated/xrt.h"
#include "xrt/detail/xclbin.h"
#include "utils/message.hpp"

#include <functional>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <filesystem>

namespace xrt_core::tools::xbreplay {

/**
 * Replay XRT class.
 * This class performs following.
 *  - Registers XRT methods of different modules  so that they can be invoked.
 *  - Maintain database map between Captured trace log and current sequence
 *    execution.
 *
 */
class replay_xrt
{
  private:
  /*Map between handle from tracelog and device */
  std::unordered_map<uint64_t, std::shared_ptr<xrt::device>> m_device_hndle_map;

  /*Map between handle from tracelog and kernel */
  std::unordered_map<uint64_t, std::shared_ptr<xrt::kernel>> m_kernel_hndle_map;

  /*Map between handle from tracelog and xcldevice handle */
  std::unordered_map<uint64_t, std::shared_ptr<xclDeviceHandle>> m_xcldev_hndle_map;

  /*Map between handle from tracelog and xcldevice handle */
  std::unordered_map<uint64_t, std::shared_ptr<xclBufferExportHandle>> m_xclBufExp_hndle_map;

  /*Map between handle from tracelog and xcldevice handle */
  std::unordered_map<uint64_t, std::shared_ptr<axlf>> m_axlf_hndle_map;

  /*Map between handle from  tracelog and xcldevice handle */
  std::unordered_map<uint64_t, std::shared_ptr<xrt::hw_context>> m_hwctx_hndle_map;

  /*Map between handle from log and run */
  std::unordered_map<uint64_t, std::shared_ptr<xrt::run>> m_run_hndle_map;

  /*Map between handle from log and bo */
  std::unordered_map<uint64_t, std::shared_ptr<xrt::bo>> m_bo_hndle_map;

  /*Map between handle from log and bo */
  std::unordered_map<uint64_t, std::shared_ptr<xrt::xclbin>> m_xclbin_hndle_map;

  /*Map between group id */
  std::unordered_map<uint64_t, xrt::memory_group> m_kernel_grp_id;

  /* Map betgween uuid & device handle */
  std::unordered_map<std::shared_ptr<xrt::device>, xrt::uuid> m_uuid_device_map;

  std::map <std::string, std::function < void (std::shared_ptr<utils::message>)>> m_api_map;

  /*Map between handle from log and xrt::module */
  std::unordered_map<uint64_t, std::shared_ptr<xrt::module>> m_module_hndle_map;

  /*Map between handle from log and xrt::elf */
  std::unordered_map<uint64_t, std::shared_ptr<xrt::elf>> m_elf_hndle_map;

  /* Registers device class API's */
  void register_device_class_func();

  /* Registers kernel class API's */
  void register_kernel_class_func();

  /* Registers run class API's */
  void register_run_class_func();

  /* Registers bo class API's */
  void register_bo_class_func();

  /*Register Xclbin class API's */
  void register_xclbin_class_func();

  /*Register hw_ctx class API's*/
  void register_hwctxt_class_func();

  /*Register module class API s */
  void register_module_class_func();

  /*Register elf class API s */
  void register_elf_class_func();

  /* Validate's file path */
  bool is_file_exist(const std::string& fileName)
  {
    std::ifstream file(fileName);
    return file.good();
  }

  std::string get_file_path(std::shared_ptr<utils::message> msg, const std::string file_ext)
  {
    if (is_file_exist(msg->m_args[0].second))
      return msg->m_args[0].second;
    else if ((msg->m_is_mem_file_available) && (msg->m_buf.size() > 0))
      return save_buf_to_file(msg, file_ext);
    else
      return "";
  }

  public:
  replay_xrt()
  {
    /* register device class API's */
    register_device_class_func();

    /* Register Kernel class API's */
    register_kernel_class_func();

    /* Register Bo class API's */
    register_bo_class_func();

    /*Register Run class API's */
    register_run_class_func();

    /*Register Xclbin class API's */
    register_xclbin_class_func();

    /* Register HW CTX Class APIs*/
    register_hwctxt_class_func();
  }

  /*
   * This method will invoke the API received from
   * Replay Worker thread if it is registered.
   */
  void invoke (std::shared_ptr<utils::message> msg)
  {
    if (m_api_map.find (msg->m_api_id) != m_api_map.end ())
    {
      msg->print_args();
      m_api_map[msg->m_api_id] (msg);
    }
    else
    {
      XBREPLAY_WARN("===================================================");
      XBREPLAY_WARN("No API MAPPED FOR:|", msg->m_api_id,"|");
      msg.reset();
    }
  }

 /**
  * Clear all data bases
  */
  void clear_map()
  {
    m_bo_hndle_map.clear();
    m_run_hndle_map.clear();
    m_kernel_hndle_map.clear();
    m_xcldev_hndle_map.clear();
    m_xclBufExp_hndle_map.clear();
    m_axlf_hndle_map.clear();
    m_xclbin_hndle_map.clear();
    m_hwctx_hndle_map.clear();
    m_device_hndle_map.clear();
  }

  /*
   * This function is used to save data from memory dump file into a file.
   */
  std::string save_buf_to_file(std::shared_ptr<utils::message> msg, std::string file_ext)
  {
    /* To create unique file name */
    static uint64_t i = 0;

    // Define the file path
    std::filesystem::path currentpath = std::filesystem::current_path();
    std::filesystem::path filepath = currentpath /
      ("replay_file_" + std::to_string(i++) + file_ext);

    // Create an output file stream in binary mode
    std::ofstream outputFile(filepath.string(), std::ios::binary);

    // Check if the file was opened successfully
    if (!outputFile)
    {
      XBREPLAY_ERROR("Failed to open the file ",filepath.string());
      return "";
    }

    // Write the contents of the vector to the file
    outputFile.write(reinterpret_cast<const char*>(msg->m_buf.data()), msg->m_buf.size());

    // Check if the write was successful
    if (!outputFile)
    {
      XBREPLAY_ERROR("Failed to write to the file",filepath);
      return "";
    }

    // Close the file
    outputFile.close();
    return filepath.string();
  }
};

}// end of namespace
