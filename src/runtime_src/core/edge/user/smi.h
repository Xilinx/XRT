// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <string>
#include "core/common/smi.h"

namespace shim_edge::smi {

class smi_edge : public xrt_core::smi::smi_base {
public:
  smi_edge();
};

/* This API can be device specific since this is used by the shim*/
std::string get_smi_config();

const xrt_core::smi::tuple_vector&
get_validate_tests();

const xrt_core::smi::tuple_vector&
get_examine_reports();
} // namespace shim_edge::smi
