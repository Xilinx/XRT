// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "utils/logger.hpp"

#include <string>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <regex>
#include <iomanip>
#include <fstream>
#include <memory>
#include <array>

namespace xrt_core::tools::xbreplay::utils {

constexpr const char* regex_entry_pattern =
              (R"(\|ENTRY\|([\w.]+)\|([\w.]+)\|([\w.]+)\|([\w.]+)\|(.*?)\|)");
constexpr const char* regex_exit_pattern =
              (R"(\|EXIT\|([\w.]+)\|([\w.]+)\|([\w.]+)\|([\w.]+)\|(.*?)\|(.*?)\|)");
constexpr const char* regex_decode_args_pattern =
              ("\\{([^{}]+)\\}.*\\(([^()]+)\\)(?!.*\\([^()]*\\))");
constexpr const char* regex_args_type_pattern = ("\\(([^)]+)\\)");
constexpr const char* regex_args_value_pattern = ("\\(([^)]+)\\)\\(([^)]+)\\)");
constexpr const char* regex_func_pattern =
              (R"((?:\b\w+\s*::\s*)?\w+\s*::\s*\w+\s*\([^)]*\))");
constexpr const char* regex_ret_val_pattern = (R"(=(\d+))");

constexpr uint32_t mem_tag_value = 0x6d656du;
constexpr uint32_t match_idx_arg_type = 1u;
constexpr uint32_t match_idx_arg_value = 2u;
constexpr uint32_t match_idx_tid = 3u;
constexpr uint32_t match_idx_handle = 4u;
constexpr uint32_t match_idx_memtag = 6u;
constexpr uint32_t match_idx_args = 5u;
constexpr uint32_t match_idx_api = 5u;
constexpr uint32_t base_hex = 16u;
constexpr uint32_t tag_read_len = 4u;
constexpr uint32_t read_block_size = 4096u;

enum class message_type {
  unknown = 0,
  api_invocation,
  stop_replay
};


enum class replay_status {
  failure = 0,
  success
};

class message
{
  public:

  std::string m_api_id;
  uint64_t  m_ret_val;
  uint64_t  m_handle;
  uint64_t  m_tid;
  std::vector<char>m_buf;
  bool m_is_mem_file_available;
  std::vector<std::pair<std::string, std::string>> m_args;

  message(std::pair<std::string, std::string> trace,
                      const std::string& file_path, bool file_available)
  : m_is_mem_file_available(file_available)
  , m_mem_file_path(file_path)
  {
    m_status = replay_status::success;
    m_status = decode_entry_line(trace.first);

    if (replay_status::success == m_status) {
     m_status = decode_exit_line(trace.second);
    }
  }

  message() = default;
  ~message() = default;

  void print_args() const
  {
    if (l.get_log_level() != log_level::debug)
      return;

    XBREPLAY_DEBUG("========================================================");
    XBREPLAY_DEBUG("|func_id |" + m_api_id + "|");
    XBREPLAY_DEBUG("|Handle  |", hex_str(m_handle), "|");
    XBREPLAY_DEBUG("|ret_val |", hex_str(m_ret_val), "|");
    for (auto i: m_args)
    {
      XBREPLAY_DEBUG("|", i.first + "  |" + i.second + "|");
    }
  }

  bool is_success() const
  {
    return (m_status == replay_status::success);
  }

  message_type get_msgtype() const
  {
    return m_type;
  }

  void set_msgtype(message_type type)
  {
    m_type = type;
  }

  /*
   * This function is used to find the tag which is associated with memdump
   * in the function exit marker line. If the tag is found then the
   * corresponding memory dump is read and stored.
   */
  void get_user_data(const std::string& tag,
                              std::vector<char>* user_data = nullptr);

  private:
  std::string m_mem_file_path;
  replay_status m_status;
  uint32_t  m_mem_offset;
  message_type m_type;

  /*
   * This function is used to remove spaces from given string
   */
  void rm_spaces_frm_str(std::string& str1, std::string& str2);

  /*
   * This function is used to retrive arguments from given string.
   */
  replay_status update_args(const std::pair <std::string&, std::string&> args);

  /*
   * This function is used to remove return type in function signature
   * in string format.
   */
  void rmv_return_type(std::string& str);

  /*
   * This function is used to decode function API signature
   */
  replay_status decode_api(const std::string& input);

  /*
   * This function is used to decode arguments of the function.
   */
  replay_status decode_args(const std::string& line);

  /*
   * This function is used to decode Function entry marker line.
   */
  replay_status decode_entry_line(const std::string& line);

  /*
   * This function is used to retrive store data from memory dump file.
   */
  void load_user_data(const uint32_t offset,
                            std::vector<char>* user_data = nullptr);

  /*
   * This function is used decode parameters such as TID, return handle etc.,
   * from the function exit marker line.
   */
  replay_status decode_exit_line(const std::string& line);
};

}// end of namespace
