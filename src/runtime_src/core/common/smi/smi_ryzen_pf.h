// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025-2026 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "core/common/smi/smi_ryzen_npu3.h"

namespace xrt_core::smi::ryzen {

class config_gen_npu3_pf : public config_gen_npu3 {
public:
  config_gen_npu3_pf();

  xrt_core::smi::subcommand
  create_configure_subcommand() override;
};

} // namespace xrt_core::smi::ryzen
