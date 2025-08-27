/**
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

// This file contains the callbacks to the various XDP plugins that deal
// with retrieving data from the device, which need to be called from
// the shim at various times.  Since all of the plugins are independent and
// may or may not be loaded in different executions, we must call each plugins
// function sequentially.  The functions themselves will simply return if
// the plugin was not loaded, so there is minimal overhead when profiling
// is turned off.

#ifndef SHIM_CALLBACKS_DOT_H
#define SHIM_CALLBACKS_DOT_H

#include "aie_trace.h"
#include "aie_profile.h"
#include "hal_device_offload.h"

namespace xdp {

// The update_device callback should be called when a new xclbin has been
// loaded onto a device.  It will call the profiling code to update the
// profiling data structures with the information from the new xclbin.
inline
void update_device(void* handle, bool hw_context_flow)
{
  hal::update_device(handle);
  aie::update_device(handle, hw_context_flow); //trace
  aie::ctr::update_device(handle, hw_context_flow); //profile
}

// The flush_device callback should be called just before a new xclbin
// is loaded.  In the case where multiple xclbins are loaded in a single
// application execution, this callback makes sure that all profiling
// information is collected from a device before it is wiped out by the
// xclbin reconfiguration and stored in the profiling data structures.
inline
void flush_device(void* handle)
{
  hal::flush_device(handle);
  aie::flush_device(handle);
}

// The finish_flush_device callback should be called in the destructor of
// the shim object.  When the application is finishing and static objects are
// being cleaned up, it is possible that the shim object is destroyed before
// the profiling data structures are destroyed.  In that case, we make sure
// that the final profiling data is flushed from the device into the profiling
// data structures before the shim connection is destroyed so the profiling
// side can process and dump the data.  If the profiling objects are destroyed
// before the shim, these functions just return.
inline
void finish_flush_device(void* handle)
{
  hal::flush_device(handle);
  aie::finish_flush_device(handle);
  aie::ctr::end_poll(handle);
}

} // end namespace xdp

#endif
