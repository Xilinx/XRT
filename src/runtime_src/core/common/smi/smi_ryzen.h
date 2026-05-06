// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <string>
#include <vector>

#include "core/common/device.h"
#include "core/common/smi/smi.h"

namespace xrt_core::smi::ryzen {

// Ryzen / NPU xrt-smi JSON config generators (shared by XDNA shim, MCDM, etc.).
class config_gen_ryzen : public xrt_core::smi::config_generator {
  std::vector<xrt_core::smi::basic_option> validate_test_desc;
  std::vector<xrt_core::smi::basic_option> examine_report_desc;

public:
  config_gen_ryzen();

  virtual const std::vector<xrt_core::smi::basic_option>&
  get_validate_test_desc() const
  {
    return validate_test_desc;
  }

  virtual const std::vector<xrt_core::smi::basic_option>&
  get_examine_report_desc() const
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

class config_gen_phoenix : public config_gen_ryzen {
  std::vector<xrt_core::smi::basic_option> validate_test_desc;

public:
  config_gen_phoenix();

  const std::vector<xrt_core::smi::basic_option>&
  get_validate_test_desc() const override
  {
    return validate_test_desc;
  }
};

class config_gen_strix : public config_gen_ryzen {
};

class config_gen_npu3 : public config_gen_ryzen {
  std::vector<xrt_core::smi::basic_option> examine_report_desc;
  std::vector<xrt_core::smi::basic_option> validate_test_desc;

public:
  config_gen_npu3();

  const std::vector<xrt_core::smi::basic_option>&
  get_examine_report_desc() const override
  {
    return examine_report_desc;
  }

  const std::vector<xrt_core::smi::basic_option>&
  get_validate_test_desc() const override
  {
    return validate_test_desc;
  }
};

void
populate_smi_instance(xrt_core::smi::smi* smi_instance, const xrt_core::device* device);

std::string
get_smi_config(const xrt_core::device* device);

} // namespace xrt_core::smi::ryzen
