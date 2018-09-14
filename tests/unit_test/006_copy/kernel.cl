/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

// Copyright 2017 Xilinx, Inc. All rights reserved.

#define width 32

__kernel
__attribute__ ((reqd_work_group_size(width, 1, 1)))
void myCopy(__global int* in, __global int* out)
{
  unsigned int x=get_local_id(0);

  __local int buf_in[width];
  __local int buf_out[width];
  event_t ev = 0;
  int i;
  int dummy = 0;

  if (dummy == 0)
  {
    ev = async_work_group_copy(buf_in, in, width, ev);

    barrier(CLK_LOCAL_MEM_FENCE);

  //  x = get_local_id(0);
    if (x != 0 && x != width)
    {
      buf_out[x] = buf_in[x] + 1;
    }
    
    barrier(CLK_LOCAL_MEM_FENCE);
    ev = async_work_group_copy(out, buf_out, width, ev);
  }
  return;
}
