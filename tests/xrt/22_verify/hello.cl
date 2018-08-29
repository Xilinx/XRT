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

//------------------------------------------------------------------------------
//
// kernel:  hello  
//
// Purpose: Copy "Hello World" into a global array to be read from the host
//
// output: char buf vector, returned to host to be printed
//

__kernel void __attribute__ ((reqd_work_group_size(1, 1, 1)))
    hello(__global char* buf) {
  // Get global ID
    
 int glbId = get_global_id(0);

 
  // Only one work-item should be responsible
  // for copying into the buffer.
   if (glbId == 0) {
     buf[0]  = 'H';
     buf[1]  = 'e';
     buf[2]  = 'l';
     buf[3]  = 'l';
     buf[4]  = 'o';
     buf[5]  = ' ';
     buf[6]  = 'W';
     buf[7]  = 'o';
     buf[8]  = 'r';
     buf[9]  = 'l';
     buf[10] = 'd';
     buf[11] = '\n';
     buf[12] = '\0';
     }

   //return;
}
