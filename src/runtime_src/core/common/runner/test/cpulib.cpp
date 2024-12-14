// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#include "experimental/xrt_runner.h"
#include "xrt/xrt_bo.h"

#include <any>
#include <map>
#include <iostream>
#include <string>
#include <vector>

#pragma warning(disable: 4100 4505)



namespace cpux {

static void
convert_ifm(std::vector<std::any>& args)
{
  auto src = std::any_cast<xrt::bo>(args.at(0));
  auto dst = std::any_cast<xrt::bo>(args.at(1));

  if (src.size() != dst.size())
    throw std::runtime_error("src and dst size mismatch");

  auto src_data = src.map<const uint8_t*>();
  auto dst_data = dst.map<uint8_t*>();

  // convert
  std::memcpy(dst_data, src_data, src.size());
}

static void
convert_ofm(std::vector<std::any>& args)
{
  auto src = std::any_cast<xrt::bo>(args.at(0));
  auto dst = std::any_cast<xrt::bo>(args.at(1));

  if (src.size() != dst.size())
    throw std::runtime_error("src and dst size mismatch");

  auto src_data = src.map<const uint8_t*>();
  auto dst_data = dst.map<uint8_t*>();

  // convert
  std::memcpy(dst_data, src_data, src.size());
}

static void
hello(const std::vector<std::any>& args)
{
  auto value = std::any_cast<int>(args.at(0));
  auto str = std::any_cast<std::string>(args.at(1));
  auto out = std::any_cast<std::string*>(args.at(2));

  if (!out)
    throw std::runtime_error("output argument is null");

  *out = "hello out " + std::to_string(value) + " " + str;
}

static void
lookup(const std::string& fnm, xrt::cpu::lookup_args* args)
{
  using function_info = xrt::cpu::lookup_args;
  static std::map<std::string, function_info> function_map = 
  {
    { "convert_ifm", {2, convert_ifm} },
    { "convert_ofm", {2, convert_ofm} },
    { "hello", {3, hello} },
  };

  if (auto it = function_map.find(fnm); it != function_map.end()) {
    const auto& [num_args, fn] = it->second;
    args->num_args = num_args;
    args->callable = fn;
    return;
  }

  throw std::runtime_error("function '" + std::string(fnm) + "' not found");
}

} // cpux

extern "C" {

__declspec(dllexport)
void
library_init(xrt::cpu::library_init_args* args)
{
  args->lookup_fn = &cpux::lookup;
}

} // extern "C"
