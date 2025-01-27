// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <string>
#include "core/common/smi.h"

namespace shim_edge::smi {

class smi_edge : public xrt_core::smi::smi_base {
protected:
  const std::vector<std::tuple<std::string, std::string, std::string>>& get_validate_test_desc() const override;
  const std::vector<std::tuple<std::string, std::string, std::string>>& get_examine_report_desc() const override;
};

/* This API can be device specific since this is used by the shim*/
std::string get_smi_config();
} // namespace shim_edge::smi
