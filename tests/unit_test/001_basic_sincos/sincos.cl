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


#define NUM_POINTS 768

//__kernel __attribute__ ((reqd_work_group_size(NUM_POINTS, 1, 1)))
__kernel void test_sincos(__global float* a, __global float* b, __global float2* output)
{
  int iter = get_local_id(0);
  
  output[iter].x = (float)cos((float)M_PI_F * 2 * -1 * (float)iter / (NUM_POINTS));
  output[iter].y = (float)sin((float)M_PI_F * 2 * -1 * (float)iter / (NUM_POINTS));

  return;
}
