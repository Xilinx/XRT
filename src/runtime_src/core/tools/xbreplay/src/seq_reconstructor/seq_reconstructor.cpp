// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "seq_reconstructor.hpp"

#include <tuple>

namespace xrt_core::tools::xbreplay {

/**
 * This function is used to find the line attributes such as
 *  TID, PID, API_ID etc. of entry/exit marker lines.
 */
std::string
find_attribute(const std::string& line, uint8_t offset, const std::string& pattern)
{
  std::regex Regex(pattern);
  std::smatch match;

  if (std::regex_search(line, match, Regex))
    return match[offset].str();

  return "";
}

/**
 * This function is used to retrive Entry/Exit Marker line attributes which
 * are used to match the Entry and corresponding Exit Marker line.
 * It Retrives TID, Object Handle, API_ID(Func Signature)
 *
 */
std::tuple<std::string, std::string, std::string>
get_line_attributes(const std::string& line, const std::string& regex_pattern)
{
  /* TID, handle, API ID */
  std::tuple<std::string, std::string, std::string> entry_id = {};

  /* update TID */
  std::get<0>(entry_id) = find_attribute(line, utils::match_idx_tid, regex_pattern);

  /* Update handle */
  std::get<1>(entry_id) = find_attribute(line, utils::match_idx_handle, regex_pattern);
  std::string api = find_attribute(line, utils::match_idx_api, regex_pattern);
  std::string api_id;

  size_t pos = api.find(")");
  if (pos != std::string::npos)
    api_id = api.substr(0, pos+1);
  else
    XBREPLAY_WARN("Failed to find API ID", api);

  /* Update API ID */
  std::get<2>(entry_id) = api_id;

  return entry_id;
}

/*
 * This is seq Reconstructor thread, this thread
 * will find Entry and corresponding Exit marker lines and
 * extract the function and its attributes and pass it to
 * replay_master thread.
 */
void xrt_seq_reconstructor::start_reconstruction()
{
  XBREPLAY_INFO("th:Seq Reconstruction start");
  std::string line;
  m_replay_master.start();

  try
  {
    while (std::getline(m_trace_file, line))
    {
      std::pair<std::string, std::string> trace = {};

      /* Save current position */
      std::streampos cur_pos = m_trace_file.tellg();

      if (line.find("ENTRY") != std::string::npos)
      {
        /* store the Entry marker*/
        trace.first = line;
        auto entry_id = get_line_attributes(line, utils::regex_entry_pattern);
        bool exit_found = false;
        while (std::getline(m_trace_file, line))
        {
          if (line.find("EXIT") != std::string::npos)
          {
            auto exit_id = get_line_attributes(line, utils::regex_exit_pattern);
            if (exit_id == entry_id)
            {
               /* found match, send to replay master */
               trace.second = line;
               exit_found = true;
               break;
            }
          }
          else
            continue;
        }
        if (!exit_found)
          XBREPLAY_ERROR("Cannot find exit line for entry:", trace.first);

        auto msg = std::make_shared<utils::message>(trace, m_mem_file_path,
                      m_is_mem_file_available);

        if (msg->is_success())
          m_msgq.send(msg);
        else
          throw std::runtime_error("Failed to send message: Invalid line\n" +
                      trace.first + "\n" + trace.second);
      }

      /* While searching entry and corresponding exit the file seek postion has
       * moved, we need to set it back to the right starting point.
       */
      m_trace_file.seekg(cur_pos);
    } /* end of  while loop */
  }
  catch (const std::runtime_error& e)
  {
    XBREPLAY_ERROR("Runtime error: ", e.what());
  }

  auto msg = std::make_shared<utils::message>();
  msg->set_msgtype(utils::message_type::stop_replay);
  m_msgq.send(msg);

  XBREPLAY_INFO("th:Seq Reconstruction exit");
}

}// end of namespace
