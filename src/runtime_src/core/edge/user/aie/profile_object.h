// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _ZYNQ_PROFILE_OBJECT_H_
#define _ZYNQ_PROFILE_OBJECT_H_

#include "core/common/shim/profile_handle.h"
#include <string>
#include <memory>

namespace ZYNQ {

class shim;

}

namespace zynqaie {

class aie_array;

class profile_object : public xrt_core::profile_handle
{
public:
  static constexpr int invalid_profile_id = -1;
  ZYNQ::shim* m_shim{nullptr};
  std::shared_ptr<aie_array> m_aie_array;
  int m_profile_id;

  profile_object(ZYNQ::shim* shim, std::shared_ptr<aie_array> array);

  int
  start(int option, const char* port1_name, const char* port2_name, uint32_t value) override;

  uint64_t
  read() override;

  void
  stop() override;

}; //profile_object

} // namespace zynqaie
#endif
