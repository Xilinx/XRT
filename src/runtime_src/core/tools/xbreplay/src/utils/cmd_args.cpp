// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "utils/cmd_args.hpp"

namespace xrt_core::tools::xbreplay::utils {
/*
 * This funciton is used to print replay usage.
 */
void cmd_args::print_usage()
{
  std::cout << "Please find below usage information" << "\n";
  for (const auto& opt : m_args)
  {
    std::string str_opt_type(1, opt.type);
    std::string log = "-" + str_opt_type + " " + opt.info;
    std::cout << log << "\n";
  }
}

/*
 * This funciton is used to validate the input cmd args.
 */
int cmd_args::validate_args(const std::string& opt, cmd_args_opt& arg)
{
  cmd_args_opt unknown = {'?', false, ""};
  for (const auto& option : m_args)
  {
    if (option.type == opt.at(0))
    {
      arg = option;
      return 0;
    }
  }
  arg = unknown;
  return -1;
}

/*
 * This funciton is used to parse the input cmd line arguments
 */
int cmd_args::parse(const std::vector<std::string>& argv,
                      cmd_args_opt &arg, const std::string& optstring)
{
  auto argc = argv.size();

  if ((m_optind >= argc) || (argv[m_optind][0] != '-'))
    return -1;

  // Get the option string without the '-'
  std::string current_opt = argv[m_optind].substr(1);
  int res = validate_args(current_opt, arg);

  if (res == -1)
  {
    if (m_opterr)
      XBREPLAY_ERROR("Unknown option: ", argv[m_optind]);

    m_optopt = argv[m_optind][1];
    return 0;
  }

  if (arg.has_val)
  {
    ++m_optind;
    if (m_optind >= argc)
    {
      if (m_opterr)
        XBREPLAY_ERROR("cmd_args_opt requires an argument: ", argv[m_optind - 1]);

      return (optstring[0] == ':') ? ':' : '?';
    }
    arg.value = argv[m_optind];
  }
  else
    m_optarg = nullptr;

  ++m_optind;
  return 0;
}

}// end of namespace
