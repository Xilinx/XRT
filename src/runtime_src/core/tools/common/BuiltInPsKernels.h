// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef __BuiltInPsKernels_h_
#define __BuiltInPsKernels_h_

// ------ I N C L U D E   F I L E S -------------------------------------------
#include "core/common/device.h"

#include <boost/property_tree/ptree.hpp>



boost::property_tree::ptree
get_ps_instance_data(const xrt_core::device* device);

#endif
