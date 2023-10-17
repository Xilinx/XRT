// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include "nocddr_fastaccess_hwemu.h"
#include <stdio.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sys/mman.h>
#include <sstream>
#include <tuple>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

nocddr_fastaccess_hwemu::nocddr_fastaccess_hwemu()
{
  // TODO Auto-generated constructor stub
}

bool nocddr_fastaccess_hwemu::isAddressMapped(uint64_t addr,
                                              size_t size)
{
  for (auto itr = mDDRMap.begin(); itr < mDDRMap.end(); itr++)
  {
    uint64_t addrI = std::get<0>(*itr);
    uint64_t sizeI = std::get<1>(*itr);
    if (addr >= addrI && (addr + size) <= (addrI + sizeI))
    {
      return true;
    }
  }
  return false;
}

bool nocddr_fastaccess_hwemu::read(uint64_t addr, unsigned char *dest,
                                   size_t size)
{
  for (auto itr = mDDRMap.begin(); itr < mDDRMap.end(); itr++)
  {
    uint64_t addrI = std::get<0>(*itr);
    uint64_t sizeI = std::get<1>(*itr);
    unsigned char *ptrI = std::get<2>(*itr);
    if (addr >= addrI && (addr + size) <= (addrI + sizeI))
    {
      uint64_t offset = addr - addrI;
      ptrI = ptrI + offset;
      memcpy((void *)dest, (const void *)ptrI, (unsigned long int)size);
      return true;
    }
  }
  return false;
}

bool nocddr_fastaccess_hwemu::write(uint64_t addr, unsigned char *src,
                                    size_t size)
{
  for (auto itr = mDDRMap.begin(); itr < mDDRMap.end(); itr++)
  {
    uint64_t addrI = std::get<0>(*itr);
    uint64_t sizeI = std::get<1>(*itr);
    unsigned char *ptrI = std::get<2>(*itr);
    if (addr >= addrI && (addr + size) <= (addrI + sizeI))
    {
      uint64_t offset = addr - addrI;
      ptrI = ptrI + offset;
      memcpy((void *)ptrI, (const void *)src, (unsigned long int)size);
      return true;
    }
  }
  return false;
}

bool nocddr_fastaccess_hwemu::init(std::string filename, std::string simdir)
{
  simdirPath = simdir;
  std::ifstream mapFile(filename.c_str());
  if (!mapFile.good() || !mapFile.is_open())
  {
    return false;
  }
  //Open the file and parse it
  std::string line;

  while (std::getline(mapFile, line))
  {
    std::stringstream ss(line);
    std::string fname;
    std::string foffset_str;
    std::string fsize_str;
    std::getline(ss, fname, ',');
    std::getline(ss, foffset_str, ',');
    std::getline(ss, fsize_str, ',');
    uint64_t foffset = 0;
    uint64_t fsize = 0;
    std::stringstream ss1(foffset_str);
    std::stringstream ss2(fsize_str);
    ss1 >> foffset;
    ss2 >> fsize;
    fname = simdir + "/" + fname;
    //mmap the file create a tuple and add to the array
    int fd = open(fname.c_str(), (O_CREAT | O_RDWR), 0666);
    if(fd >= 0)
    {
      int rf = ftruncate(fd, fsize);
      if (rf == -1)
      {
        close(fd);      // ftruncate failed, close fd
        return false;
      }
    }
    else
    {
      close(fd);         // open failed, close fd
      return false;
    }
    // fd is open if and only ftruncate successful
    unsigned char *memP = (unsigned char *)mmap(NULL, fsize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd, 0);
    if (memP != NULL)
    {
      std::tuple<unsigned long long, size_t, unsigned char *> t(foffset, fsize, memP);
      mDDRMap.push_back(t);
    }
    else
    {
      close(fd);
    }
  }
  mapFile.close();
  return true;
}

nocddr_fastaccess_hwemu::~nocddr_fastaccess_hwemu()
{
  for (auto itr = mDDRMap.begin(); itr < mDDRMap.end(); itr++)
  {
    uint64_t size = std::get<1>(*itr);
    unsigned char *ptr = std::get<2>(*itr);
    munmap((void *)ptr, size);
    close(mFdMap[std::get<0>(*itr)]);
  }
  mDDRMap.clear();
  mFdMap.clear();
}
