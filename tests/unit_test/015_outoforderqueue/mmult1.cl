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

__kernel __attribute__ ((reqd_work_group_size(16, 16, 1)))
void mmult(__global int* a, __global int* b, __global int* output)
{
  int r = get_local_id(0);
  int c = get_local_id(1);
  int rank = get_local_size(0);
  int running = 0;

  for (int index=0; index<16; index++) {
    int aIndex = r*rank + index;
    int bIndex = index*rank + c;
    running +=  a[aIndex] * b[bIndex];
  }
  
  output[r*rank + c] = running;
  return;
}
