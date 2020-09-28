/**
 * Copyright (C) 2020 Xilinx, Inc
 * Author(s): Himanshu Choudhary <hchoudha@xilinx.com>
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

#ifndef AIE_D_H
#define AIE_D_H

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <thread>
#include "core/common/device.h"
#include "core/edge/user/aie/graph.h"

/*
 * It receives commands from zocl and dispatches back the output.
 * One typical command is get graph status.
 */
namespace zynqaie {

class Aied
{
public:
  Aied(xrt_core::device* device);
  ~Aied();
  void registerGraph(const graph_type *graph);
  void deregisterGraph(const graph_type *graph);

private:
  bool done;
  void pollAIE();
  std::thread mPollingThread;
  xrt_core::device *mCoreDevice;
  std::vector<const graph_type*> mGraphs;
};
} // end namespace

#endif
