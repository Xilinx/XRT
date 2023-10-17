// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _NOCDDR_FASTACCESS_HWEMU_H_
#define _NOCDDR_FASTACCESS_HWEMU_H_

#include <string>
#include <vector>
#include <cstdint>
#include <map>
class nocddr_fastaccess_hwemu
{
public:
  nocddr_fastaccess_hwemu();
  bool init(std::string filename, std::string simdir);
  bool isAddressMapped(uint64_t addr, size_t size);
  bool read(uint64_t addr, unsigned char *dest, size_t size);
  bool write(uint64_t addr, unsigned char *src, size_t size);
  virtual ~nocddr_fastaccess_hwemu();

private:
  std::vector<std::tuple<unsigned long long, size_t, unsigned char *>> mDDRMap;
  std::map<unsigned long long, int> mFdMap;
  std::string simdirPath;
};

#endif /* _NOCDDR_FASTACCESS_HWEMU_H_ */
