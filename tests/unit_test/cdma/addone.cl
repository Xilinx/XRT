/**
 * Copyright (C) 2018 Xilinx, Inc
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

/*
  OpenCL Task (1 work item)
  512 bit wide add one
  512 bits = 8 vector of 64 bit unsigned
    Add one to first element in vector
    Copy through remaining elements
*/

__kernel __attribute__ ((reqd_work_group_size(1, 1 , 1)))
void add0 (__global ulong8 *a, __global ulong8 * b, unsigned int  elements)
{
  ulong8 temp;
  unsigned int i;

  for(i=0;i< elements;i++){
    temp=a[i];
    //add one to first element in vector
    temp.s0=temp.s0+1;
    b[i]=temp;
  }
  return;
}

__kernel __attribute__ ((reqd_work_group_size(1, 1 , 1)))
void add1 (__global ulong8 *a, __global ulong8 * b, unsigned int  elements)
{
  ulong8 temp;
  unsigned int i;

  for(i=0;i< elements;i++){
    temp=a[i];
    //add one to first element in vector
    temp.s0=temp.s0+1;
    b[i]=temp;
  }
  return;
}

__kernel __attribute__ ((reqd_work_group_size(1, 1 , 1)))
void add2 (__global ulong8 *a, __global ulong8 * b, unsigned int  elements)
{
  ulong8 temp;
  unsigned int i;

  for(i=0;i< elements;i++){
    temp=a[i];
    //add one to first element in vector
    temp.s0=temp.s0+1;
    b[i]=temp;
  }
  return;
}

__kernel __attribute__ ((reqd_work_group_size(1, 1 , 1)))
void add3 (__global ulong8 *a, __global ulong8 * b, unsigned int  elements)
{
  ulong8 temp;
  unsigned int i;

  for(i=0;i< elements;i++){
    temp=a[i];
    //add one to first element in vector
    temp.s0=temp.s0+1;
    b[i]=temp;
  }
  return;
}
