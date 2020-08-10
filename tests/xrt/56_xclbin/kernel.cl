/**
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
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

__attribute__ ((reqd_work_group_size(1024, 1, 1)))
kernel void simple(global int *restrict s1,
                   global const int *s2,
                   int foo)
{
    const int id = get_local_id(0);
    s1[id] = s2[id] + id * foo;
}
