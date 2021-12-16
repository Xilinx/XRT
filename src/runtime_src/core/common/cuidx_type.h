/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 */
#ifndef XRT_CORE_CUIDX_TYPE_H
#define XRT_CORE_CUIDX_TYPE_H

#include <cstdint>

#ifdef _WIN32
# pragma warning( push )
# pragma warning( disable : 4201 )
#endif

namespace xrt_core {

// cuidx_type - encode cuidx and domain
//
// @domain_index: index within domain
// @domain:       domain identifier
// @index:        combined encoded index
//
// The domain_index is used in command cumask in exec_buf
// The combined index is used in context creation in open_context
struct cuidx_type {
  union {
    std::uint32_t index;
    struct {
      std::uint16_t domain_index; // [15-0]
      std::uint16_t domain;       // [31-16]
    };
  };

  // Ensure consistent use of domain and index types
  using domain_type = uint16_t;
  using domain_index_type = uint16_t;
};

} // xrt_core

#ifdef _WIN32
# pragma warning( pop )
#endif

#endif
