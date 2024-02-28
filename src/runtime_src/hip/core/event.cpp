// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.

#include "event.h"

namespace xrt::core::hip {

bool
command::
submit()
{
    return true; //temporary
}

bool
command::
wait()
{
    return true; //temporary
}

bool
event::
submit()
{
    return true; //temporary
}

bool
event::
wait()
{
    return true; //temporary
}

kernel_start::
kernel_start(function &f, void* args)
{}

bool
kernel_start::
submit()
{
    return true; //temporary
}

bool
kernel_start::
wait()
{
    return true; //temporary
}

bool
copy_buffer::
submit()
{
    return true; //temporary
}

bool
copy_buffer::
wait()
{
    return true; //temporary
}

}

