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

#if 0
// AutoESL intrinsics
void kernel _ssdm_RegionBegin(char *) __attribute__ ((nothrow));
void kernel _ssdm_RegionEnd(char*) __attribute__ ((nothrow));
void kernel _ssdm_op_SpecPipeline(int, int, int, char *) __attribute__ ((nothrow));

#define PIPELINE_BEGIN(label) {\
  _ssdm_RegionBegin(label);\
  _ssdm_op_SpecPipeline(1, 1, 1, "");
#define PIPELINE_END(label)\
  _ssdm_RegionEnd(label);}

#else
#define PIPELINE_BEGIN(label) {
#define PIPELINE_END(label) }
#endif

__kernel __attribute__ ((reqd_work_group_size(16, 16, 1)))
void mmult(__global int* a, __global int* b, __global int* output)
{
  int r = get_global_id(0);
  int c = get_global_id(1);
  int rank = get_global_size(0);
  int running = 0;
  for (int index=0; index<rank; index++) {
    PIPELINE_BEGIN("pipe0") {
      int aIndex = r*rank + index;
      int bIndex = index*rank + c;
      running +=  a[aIndex] * b[bIndex];
    } PIPELINE_END("pipe0")
  }
  barrier(CLK_LOCAL_MEM_FENCE); //not required
  output[r*rank + c] = running;
  //output[r*rank + c] = b[r*rank+c]+1;
  return;
}
