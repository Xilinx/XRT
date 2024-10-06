// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "utils/logger.hpp"

#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace xrt_core::tools::xbreplay::utils {

struct cmd_args_opt
{
  char type;
  bool has_val;
  std::string value;
  std::string info;
};

class cmd_args
{
  public:
  char* m_optarg;
  cmd_args(std::vector<cmd_args_opt>&& arguments)
   : m_optarg(nullptr)
   , m_args(std::move(arguments))
   , m_optind(1)
   , m_opterr(true)
   , m_optopt('?')
  {}

 /*
  * This funciton is used to print replay usage.
  */
  void print_usage();

 /*
  * This funciton is used to parse the input cmd line arguments
  */
  int parse(const std::vector<std::string>& argv,
                cmd_args_opt &arg, const std::string& optstring);

  private:
  std::vector<cmd_args_opt> m_args;
  uint32_t m_optind;
  bool m_opterr;
  int8_t m_optopt;

  /*
   * This funciton is used validate the input cmd args.
   */
  int validate_args(const std::string&  opt, cmd_args_opt& arg);
};

}// end of namespace

