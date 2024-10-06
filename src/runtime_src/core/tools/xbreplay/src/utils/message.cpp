// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#include "utils/message.hpp"

namespace xrt_core::tools::xbreplay::utils {

void trim_spaces(std::string& entry)
{
  entry.erase(entry.begin(), std::find_if(entry.begin(), entry.end(), [](int ch) {
    return !std::isspace(ch);
  }));
  entry.erase(std::find_if(entry.rbegin(), entry.rend(), [](int ch)
  {
    return !std::isspace(ch);
  }).base(), entry.end());
}

/*
 * This function is used to retrive arguments from given string.
 */
replay_status message::update_args(const std::pair <std::string&, std::string&> args)
{
  replay_status estatus = replay_status::success;
  std::stringstream ss(args.first);
  std::string entry;

  /* updating args type */
  while (std::getline(ss, entry, ','))
  {
    trim_spaces(entry);
    m_args.emplace_back(entry, "");
  }

  /* get args value */
  std::stringstream ss2(args.second);
  std::string token;
  std::vector<std::string> val;
  while (std::getline(ss2, token, ','))
  {
    val.push_back(token);
  }

  uint32_t i = 0;
  for (auto &arg : m_args)
  {
    std::string& arg_value = val[i];
    trim_spaces(arg_value);
    arg.second = arg_value;
    i++;
  }

  return estatus;
}

/*
 * This function is used to remove return type in function signature
 * in string format.
 */
void message::rmv_return_type(std::string& str)
{
  /* Regular expression to match function signature (excluding return type)*/
  std::regex pattern(regex_func_pattern);

  /* Check if the input string contains a function signature */
  std::smatch match;

  if (std::regex_search(str, match, pattern))
    str = match[0];
}

/*
 * This function is used to decode function API signature
 */
replay_status message::decode_api(const std::string& input)
{
  replay_status estatus = replay_status::success;

  size_t pos = input.find(")");
  if (pos != std::string::npos)
  {
    m_api_id = input.substr(0, pos+1);
    rmv_return_type(m_api_id);
  }
  else
    estatus = replay_status::failure;

  return estatus;
}

/*
 * This function is used to decode arguments of the function.
 */
replay_status message::decode_args(const std::string& line)
{
  replay_status estatus = replay_status::success;

  if (line.find("...") != std::string::npos)
  {
    std::regex pattern(regex_decode_args_pattern);
    std::smatch matches;
    if (std::regex_search(line, matches, pattern))
    {
      std::string args_type = matches[match_idx_arg_type].str();
      std::string args_value = matches[match_idx_arg_value].str();

      /* In some cases there will be no arguments
       * do not error out
       */
      if (!args_type.empty())
      {
        std::istringstream iss1(args_type);
        std::istringstream iss2(args_value);
        std::string token1, token2;
        while (std::getline(iss1, token1, ',') && std::getline(iss2, token2, ','))
        {
          trim_spaces(token1);
          trim_spaces(token2);
          m_args.emplace_back(token1, token2);
        }
      }
    }
    else
     XBREPLAY_WARN("Pattern do not match for args.");
  }
  else
  {
    /* Received string is of the format
      * "mops::mops(int, std::string)(num=2, tag=object_a)"
      * step1: we create two strings from given string
      *        first one contains the arguments
      *        second one contains the values.
      *
      * step2: update the arguments first and then
      *        update the values.
      *
      */
    std::regex regexFirst(regex_args_type_pattern);
    std::regex regexSecond(regex_args_value_pattern);

    std::smatch match_firstline, match_secondline;

    if (std::regex_search(line, match_firstline, regexFirst) &&
              std::regex_search(line, match_secondline, regexSecond))
    {
      std::string args_type = match_firstline[match_idx_arg_type].str();
      std::string args_value = match_secondline[match_idx_arg_value].str();

      /* arguments type, arguments value */
      const std::pair <std::string&, std::string&> args = {args_type, args_value};

      /* In some cases there will be no arguments
        * do not error out
        */
      if (!args_type.empty())
        return update_args(args);
    }
  }
  return estatus;
}

/*
 * This function is used to decode Function entry marker line.
 */
replay_status message::decode_entry_line(const std::string& line)
{
  replay_status estatus = replay_status::success;
  std::smatch match;

  /*
   * Entry trace marker is of below format and correspondigly update regex
   * ENTRY <number> <number> <number> <hex-value> ClassName::MethodName(arguments).
   **/
  std::regex pattern(regex_entry_pattern);
  if (std::regex_search(line, match, pattern))
  {
    /* get thread ID */
    m_tid = std::stoul(match[match_idx_tid], nullptr, base_hex);

    /*get object handle */
    m_handle = std::stoull(match[match_idx_handle], nullptr, base_hex);

    /* get arguments */
    std::string arguments = match[match_idx_args];

    /* create hash code with arguments */
    estatus = decode_api(arguments);

    if (replay_status::success == estatus)
    {
      /* decode arguments */
      estatus = decode_args(arguments);
    }
  }
  else
  {
    XBREPLAY_ERROR( "Invalid entry format: ",line);
    estatus = replay_status::failure;
  }
  return estatus;
}

/*
 * This function is used to find the tag which is associated with memdump
 * in the function exit marker line. If the tag is found then the
 * corresponding memory dump is read and stored.
 */
void message::get_user_data(const std::string& tag, std::vector<char>* user_data)
{
  /* Start Marker */
  const std::string start_marker = "mem@";
  const std::string end_marker = "[";

  //Find the position of subString in mainString
  size_t start_pos = tag.find(start_marker);
  size_t end_pos = 0;
  if (start_pos != std::string::npos)
  {
    //Adjust the start position to skip the start marker
    start_pos += start_marker.length();

    // Find the position of the end marker, starting from the adjusted start position
    end_pos = tag.find(end_marker, start_pos);
    if (end_pos == std::string::npos)
    {
      XBREPLAY_ERROR("Mem Tag invalid format: ", tag);
      return;
    }

    std::string offset_str = tag.substr(start_pos, end_pos - start_pos);
    m_mem_offset = std::stoul(offset_str, nullptr, base_hex);
    load_user_data(m_mem_offset, user_data);
  }
  else
  {
    m_is_mem_file_available = false;
  }
}

/*
 * This function is used to load user data from memory dump file.
 */
void message::load_user_data(const uint32_t offset, std::vector<char>* user_data)
{
  std::ifstream file(m_mem_file_path, std::ios::binary);

  if (!file.is_open())
    throw std::runtime_error("Failed to open file: " + m_mem_file_path);

  // Seek to a specific position (e.g., position 100)
  std::streampos seek_pos = offset;
  file.seekg(seek_pos);

  if (!file)
    throw std::runtime_error("Failed to seek to position " + std::to_string(seek_pos));

  // Read the first 4 bytes
  uint32_t tag_value = 0;
  std::array<char, tag_read_len> buf_tag_val = {0};
  file.read(buf_tag_val.data(), tag_read_len);

  if (!file)
    throw std::runtime_error("Error: Could not read tag value");

  memcpy(&tag_value, buf_tag_val.data(), tag_read_len);

  /* check if the TAG value matches */
  if (tag_value == mem_tag_value)
  {
    // Read the next 4 bytes
    uint32_t mem_size = 0;
    std::array<char, tag_read_len> buf = {0};

    file.read(buf.data(), tag_read_len);
    if (!file)
      throw std::runtime_error("Error: Could not read memory size");

    memcpy(&mem_size, buf.data(), tag_read_len);

    /* While reading Param data from memdump, update in user provided pointer
    * else update in the member variable
    */
    std::vector<char>& target = user_data ? *user_data : m_buf;
    target.resize(mem_size, 0);

    if (mem_size < read_block_size)
    {
      file.read(target.data(), mem_size);
      if (!file)
        throw std::runtime_error("Error: Could not read buffer");
    }
    else
    {
      /* Read in  4k blocks */
      uint32_t bytes_read = 0;
      while (bytes_read < mem_size)
      {
        uint32_t bytes_to_read = std::min(static_cast<uint32_t>(read_block_size), mem_size - bytes_read);
        file.read(&target[bytes_read], bytes_to_read);

        if (!file)
           throw std::runtime_error("Error reading from file " + m_mem_file_path);

        bytes_read += bytes_to_read;
      }
    }
  }
  else
    throw std::runtime_error("Tag value does not match: " + std::to_string(tag_value));
}

/*
 * This function is used decode parameters such as TID, return handle etc.,
 * from the function exit marker line.
 */
replay_status message::decode_exit_line(const std::string& line)
{
  std::regex pattern(regex_exit_pattern);
  std::smatch match;
  replay_status estatus = replay_status::success;

  if (std::regex_search(line, match, pattern))
  {
    std::string mem_tag = match[match_idx_memtag].str();

    std::regex return_val_pattern(regex_ret_val_pattern);
    std::smatch ret_match;
    const std::string& api = match[match_idx_api].str();
    const std::string substr = ")=";
    m_ret_val = 0;

    // Check if the substring exists in the input string
    if (api.find(substr) != std::string::npos)
    {
      if (std::regex_search(api, ret_match, return_val_pattern))
      {
        if (ret_match.size() > 1)
          m_ret_val = std::stoull(ret_match[1].str());
      }
    }
    /* update handle to return value if no return found in  API */
    if(m_ret_val == 0)
      m_ret_val = std::stoull(match[match_idx_handle], nullptr, base_hex);

    if (m_is_mem_file_available)
      get_user_data(mem_tag);
  }
  else
  {
    // some cases we may not find an exit line if the
    // program is terminated, proceed with the invocation.
    //estatus = replay_status::failure;
    XBREPLAY_ERROR("Invalid exit marker format ", line);
  }
  return estatus;
}

}// end of namespace

