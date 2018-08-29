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

#include "kernelparameter.cl"

#if !defined(RUNMODE)
  #error Test RUNMODE Macro not defined
#endif
#if !defined(TYPE)
  #error Test TYPE Macro not defined
#endif
#if !defined(TYPESIZE)
  #error Test TYPESIZE Macro not defined
#endif
#if !defined(TYPEISVECTOR)
  #error Test TYPEISVECTOR Macro not defined
#endif


#define BUFFERSIZE                          1024

//pointer access pipeline loop
#define PIPELINELOOP                        2


__kernel 
__attribute__ ((reqd_work_group_size(1,1,1)))
void bandwidth1(__global TYPE  * __restrict output,__global TYPE  * __restrict input, uint blocks,uint reps)
{

  unsigned int repindex;
  unsigned int blockindex;
  local TYPE buffer0[BUFFERSIZE];
  local TYPE buffer1[BUFFERSIZE];
  event_t ev=0;
  

    repindex=0;
    //prevent loop flattening with do..while loop
    do{
      __attribute__((xcl_pipeline_loop))
      for(blockindex=0;blockindex<blocks;blockindex++){
        TYPE temp;
        temp=input[blockindex];
        output[blockindex]=temp;
      }
      repindex++;
    }while(repindex<reps);
}

__kernel
__attribute__ ((reqd_work_group_size(1,1,1)))
void bandwidth2(__global TYPE  * __restrict output,__global TYPE  * __restrict input, uint blocks,uint reps)
{

  unsigned int repindex;
  unsigned int blockindex;
  local TYPE buffer0[BUFFERSIZE];
  local TYPE buffer1[BUFFERSIZE];
  event_t ev=0;
 

    repindex=0;
    //prevent loop flattening with do..while loop
    do{
      __attribute__((xcl_pipeline_loop))
      for(blockindex=0;blockindex<blocks;blockindex++){
        TYPE temp;
        temp=input[blockindex];
        output[blockindex]=temp;
      }
      repindex++;
    }while(repindex<reps);
}

