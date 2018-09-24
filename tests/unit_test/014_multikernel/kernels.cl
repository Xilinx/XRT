/**
 * Copyright (C) 2016-2018 Xilinx, Inc
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

//
// <data source> -> Stage 1 -> Stage 2 -> Stage 3 -> <data sink>
//

// Stage 1
kernel void
input_stage(__global int *src, global int * output)
{
//  output[get_local_id(0)] = src[get_local_id(0)] & 0xf;
//  output[get_local_id(0)*2] = src[get_local_id(0)*2] & 0xf;
//  output[get_local_id(0)*3] = src[get_local_id(0)*3] & 0xf;
//  output[get_local_id(0)*4] = src[get_local_id(0)*4] & 0xf;
  output[get_local_id(0)] = src[get_local_id(0)] & 0x0f0f0f0f;
}

// Stage 2
kernel void
adder_stage(global int *input, __global int *output)
{
  output[get_local_id(0)] = input[get_local_id(0)] + 1000;
}

// Stage 3
kernel void
output_stage(global long *input, __global long *sink)
{
  if (get_local_id(0) < get_local_size(0)/2)
    sink[get_local_id(0)] = input[get_local_id(0)] + 1;
}
