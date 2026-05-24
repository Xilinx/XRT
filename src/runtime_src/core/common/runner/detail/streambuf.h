// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_COMMON_RUNNER_DETAIL_STREAMBUF_H_
#define XRT_COMMON_RUNNER_DETAIL_STREAMBUF_H_

#include <streambuf>

namespace xrt_core::detail {

// struct streambuf - wrap a std::streambuf around an external buffer
//
// This is used create elf files from memory through a std::istream
struct streambuf : public std::streambuf
{
  streambuf(char* begin, char* end)
  {
    setg(begin, begin, end);
  }

  template <typename T>
  streambuf(T* begin, T* end)
    : streambuf(reinterpret_cast<char*>(begin), reinterpret_cast<char*>(end))
  {}

  template <typename T>
  streambuf(const T* begin, const T* end) // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    : streambuf(const_cast<T*>(begin), const_cast<T*>(end))
  {}

  std::streampos
  seekpos(std::streampos pos, std::ios_base::openmode which) override
  {
    if (pos < 0 || pos > (egptr() - eback()))
      return std::streampos(std::streamoff(-1));
    
    setg(eback(), eback() + pos, egptr());
    return pos;
  }

  std::streampos
  seekoff(std::streamoff off, std::ios_base::seekdir way, std::ios_base::openmode which) override
  {
    char* new_gptr = nullptr;
  
    if (way == std::ios_base::cur)
      new_gptr = gptr() + off;
    else if (way == std::ios_base::end)
      new_gptr = egptr() + off;
    else if (way == std::ios_base::beg)
      new_gptr = eback() + off;
    else
      return std::streampos(std::streamoff(-1));
  
    if (new_gptr < eback() || new_gptr > egptr())
      return std::streampos(std::streamoff(-1));
  
    setg(eback(), new_gptr, egptr());
    return std::streampos(new_gptr - eback());
  }
};

} // xrt_core::detail

#endif
