// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#pragma once

#include "core/common/smi/smi.h"

#include <string>

namespace xrt_core::smi::alveo {

void
populate_smi_instance(xrt_core::smi::smi* smi_instance);

std::string
get_smi_config();

xrt_core::smi::tuple_vector
get_subcommands_list();

} // namespace xrt_core::smi::alveo
