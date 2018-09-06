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

__attribute__ ((reqd_work_group_size(1, 1, 1)))
kernel void mysequence(__global unsigned *a)
{
    a[0] = 0X586C0C6C;
    a[1] = 'X';
    a[2] = 0X586C0C6C;
    a[3] = 'I';
    a[4] = 0X586C0C6C;
    a[5] = 'L';
    a[6] = 0X586C0C6C;
    a[7] = 'I';
    a[8] = 0X586C0C6C;
    a[9] = 'N';
    a[10] = 0X586C0C6C;
    a[11] = 'X';
    a[12] = 0X586C0C6C;
    a[13] = '\0';
    a[14] = 0X586C0C6C;
    a[15] = '\0';
}
