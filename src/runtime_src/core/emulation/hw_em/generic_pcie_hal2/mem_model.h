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

#ifndef OCL_PLATFORM_H
#define OCL_PLATFORM_H
#include <math.h>
#include <iostream>

#include <string.h> // memcpy
#include <sstream> // memcpy
#include <stdlib.h> //realloc
#include <map> //realloc
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "rpc_messages.pb.h"

#define ONE_KB (0x400)
#define ONE_MB (ONE_KB * ONE_KB)
#define PAGESIZE (ONE_MB)
#define ADDRBITS (20)
#define N_1MBARRAYS 4096

class mem_model{
public:
unsigned int writeDevMem(uint64_t offset, const void* src, unsigned int size);
unsigned int readDevMem(uint64_t offset, void* dest, unsigned int size);

protected:
private:
  unsigned char* get_page(uint64_t offset);
  std::string get_mem_file_name(uint64_t pageIdx);
  std::map<uint64_t,unsigned char*> pageCache;
  std::map<uint64_t,unsigned char*>::iterator pageCacheItr;

  ddr_mem_msg serialize_msg;
  ddr_mem_msg deserialize_msg;
  void serialize();
  std::string mDeviceName;
  std::string module_name;
public:
  mem_model(std::string deviceName);
  ~ mem_model();
};

#endif


