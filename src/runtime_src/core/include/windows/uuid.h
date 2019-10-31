/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef core_include_windows_uuid_h_
#define core_include_windows_uuid_h_

#pragma warning( push )
#pragma warning ( disable : 4100 4996 )
#include <cstring>
#include <cstdio>
typedef unsigned char xuid_t[16];

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

inline void
uuid_unparse_lower(const xuid_t uuid, char* str)
{
  std::sprintf(str,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	       uuid[0], uuid[1], uuid[2], uuid[3],
	       uuid[4], uuid[5],
	       uuid[6], uuid[7],
	       uuid[8], uuid[9],
	       uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
}

#pragma warning ( pop )

#endif
