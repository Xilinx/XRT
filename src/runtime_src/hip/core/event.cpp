// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.

#include "event.h"

namespace xrt::core::hip {

bool
command::
submit()
{}

bool
command::
wait()
{}

bool
event::
submit() override
{}

bool
event::
wait() override
{}

kernel_start::
kernel_start(xrt::kernel &, void* args)
{}

bool
kernel_start::
submit() override
{}

bool
kerel_start::
wait() override
{}

bool
buffer_copy::
submit() override
{}

bool
buffer_copy::
wait() override
{}

}

