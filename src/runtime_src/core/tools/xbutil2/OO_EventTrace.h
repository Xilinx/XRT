// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "tools/common/OptionOptions.h"
#include "tools/common/SmiWatchMode.h"
#include "EventTrace.h"
#include <optional>
#include <map>
#include <string>
#include <vector>

class OO_EventTrace : public OptionOptions {
public:
  void execute( const SubCmdOptions &_options ) const override;
  void validate_args() const;

public:
  explicit OO_EventTrace(const std::string &_longName, bool _isHidden = false);

private:
  uint32_t 
  parse_categories(const std::vector<std::string>& categories_list,
                   const xrt_core::device* device) const;

  std::map<std::string, uint32_t> 
  get_category_map(const xrt_core::device* device) const;

  std::vector<std::string>
  mask_to_category_names(uint32_t mask, const xrt_core::device* device) const;

  void
  handle_list_categories(const xrt_core::device* device) const;

  void
  handle_status(const xrt_core::device* device) const;

  void
  handle_config(const xrt_core::device* device) const;

private:
  std::string m_device;
  bool m_enable;
  bool m_disable;
  bool m_help;
  bool m_list_categories;
  bool m_status;
  std::vector<std::string> m_categories;
};
