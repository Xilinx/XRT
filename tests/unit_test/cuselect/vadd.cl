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

__kernel void vadd (
__global int* A,
__global int* B,
__global int* C,
__global int* D
)
{
  const int length=16;
  int offset = get_global_id(0);
  int stride = get_global_size(0);

  while (offset < length) {
    D[offset] = C[offset] + A[offset] + B[offset];
    offset += stride;
  }

  return;
}
