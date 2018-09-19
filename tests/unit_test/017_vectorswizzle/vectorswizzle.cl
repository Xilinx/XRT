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

//swizzle from x y z w ==>> y x w z

__kernel __attribute__ ((reqd_work_group_size(16, 1, 1)))
void vectorswizzle(__global int4 *a)
{
  int4 foo1,foo2;
  int r = get_global_id(0);

  foo1 = a[r];
  foo2 = foo1.zwxy;

 
  a[r] = foo2;
  return;
}
