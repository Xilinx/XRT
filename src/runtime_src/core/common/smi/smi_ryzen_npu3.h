// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025-2026 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "core/common/smi/smi_ryzen.h"

namespace xrt_core::smi::ryzen {

class config_gen_npu3 : public config_gen_ryzen {
protected:
  std::vector<xrt_core::smi::basic_option> examine_report_desc;

public:
  config_gen_npu3();
  const std::vector<xrt_core::smi::basic_option>&
  get_examine_report_desc() const override
  {
    return examine_report_desc;
  }

  xrt_core::smi::subcommand
  create_validate_subcommand() override;

  xrt_core::smi::subcommand
  create_examine_subcommand() override;

  xrt_core::smi::subcommand
  create_configure_subcommand() override;
};

} // namespace xrt_core::smi::ryzen
