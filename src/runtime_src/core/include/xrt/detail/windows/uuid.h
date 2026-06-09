// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019 Xilinx, Inc. All rights reserved.
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef core_include_windows_uuid_h_
#define core_include_windows_uuid_h_

#pragma warning( push )
#pragma warning ( disable : 4244 )

typedef unsigned char xuid_t[16];

#ifdef __cplusplus
#include <cstring>
#include <cstdio>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

inline void
uuid_copy(xuid_t dst, const xuid_t src)
{
  std::memcpy(dst,src,sizeof(xuid_t));
}

inline void
uuid_clear(xuid_t uuid)
{
  std::memset(uuid, 0, sizeof(xuid_t));
}

inline int
uuid_compare(const xuid_t uuid1, const xuid_t uuid2)
{
  return memcmp(uuid1, uuid2, sizeof(xuid_t));
}

inline int
uuid_is_null(const xuid_t uuid)
{
  for (int i=0; i<sizeof(xuid_t) ;++i)
    if (uuid[i])
      return 0;
  return 1;
}

inline void
uuid_unparse_lower(const xuid_t uuid, char* str)
{
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');

  for (int i=0; i<sizeof(xuid_t); ++i) {
    oss << std::setw(2) << static_cast<int>(uuid[i]);
    if (i==3 || i==5 || i==7 || i==9)
      oss << '-';
  }

  std::string s = oss.str();
  std::copy(s.begin(), s.end(), str);
  str[36] = 0;
}

inline void
uuid_parse(const char* str, xuid_t uuid)
{
  //381d7988-e0b4-421b-811a-cdcf83ad2764
  constexpr int uuid_str_sz = 36;

  for (int i=0; i<uuid_str_sz; ++i) {
    if (str[i]==0)
      throw std::runtime_error("invalid uuid: " + std::string(str));
    if ((i==8 || i==13 || i==18 || i==23)) {
      if (str[i] != '-')
        throw std::runtime_error("invalid uuid: " + std::string(str));
    }
    else if (!std::isxdigit(str[i]))
      throw std::runtime_error("invalid uuid: " + std::string(str));
  }

  for (int i=0,u=0; i<uuid_str_sz; i+=2) {
    if (str[i] == '-')
      ++i;
    auto hi = std::stoi(std::string(1,str[i]),0,16);
    auto lo = std::stoi(std::string(1,str[i+1]),0,16);
    uuid[u++] = (hi << 4) | lo;
  }

#if 0
  char cmp[uuid_str_sz+1] = {0};
  uuid_unparse_lower(uuid, cmp);
  if (strncmp(str, cmp, 36) != 0)
    throw std::runtime_error("uuid_parse error (" + std::string(str) + ")(" + cmp + ")");
#endif
}

#endif 

#pragma warning ( pop )
#endif
