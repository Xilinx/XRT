/**
 * Copyright (C) 2019 Xilinx, Inc
 * Author(s): Min Ma	<min.ma@xilinx.com>
 *          : Larry Liu	<yliu@xilinx.com>
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

#ifndef __SK_TYPES_H_
#define __SK_TYPES_H_

/*
 * Helper functions for kernel to use.
 * getHostBO : create a BO handle from given physical address.
 * mapBO     : map BO handle to process's memory space.
 * freeBO    : free BO handle.
 */
struct sk_operations {
  unsigned int (* getHostBO)(unsigned long paddr, size_t size);
  void *(* mapBO)(unsigned int boHandle, bool write);
  void (* freeBO)(unsigned int boHandle);
};

/*
 * Each soft kernel fucntion has two arguments.
 * args: provide reg file (data input, output, size etc.,
 *       for soft kernel to run.
 * ops:  provide help functions for soft kernel to use.
 */
typedef int (* kernel_t)(void *args, struct sk_operations *ops);

#endif
