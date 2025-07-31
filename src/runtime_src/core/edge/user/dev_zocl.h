// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights

#ifndef DEV_ZOCL_H_
#define DEV_ZOCL_H_

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include "dev.h"

namespace xrt_core::edge {

class dev_zocl : public dev, public std::enable_shared_from_this<dev_zocl> {
public:
    explicit dev_zocl(const std::string& root);
    ~dev_zocl() override;
    dev_zocl(const dev_zocl&&) = delete;
    dev_zocl(const dev_zocl&) = delete;
    dev_zocl& operator=(const dev_zocl&) = delete;
    dev_zocl& operator=(const dev_zocl&&) = delete;

    std::shared_ptr<xrt_core::device>
    create_device(device::handle_type handle, device::id_type id) const override;

    device::handle_type
    create_shim(device::id_type id) const override;
};

} //namespace xrt_core::edge
#endif
