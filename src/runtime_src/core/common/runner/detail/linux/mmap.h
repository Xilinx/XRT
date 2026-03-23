// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef ARTIFACTS_DETAIL_LINUX_MMAP_H
#define ARTIFACTS_DETAIL_LINUX_MMAP_H

// This file is not to be included stand-alone

#include <cstddef>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

namespace xrt_core::artifacts::detail {

/// Holds a file mapped with mmap on Linux. Non-copyable, movable.
struct mmap_artifact
{
  int m_fd = -1;
  std::size_t m_size = 0;
  void* m_ptr = nullptr;

  mmap_artifact() = default;

  explicit
  mmap_artifact(const std::string& path)
   : m_fd{open_fd(path)}
   , m_size{get_size(m_fd)}
   , m_ptr{map_region(m_fd, m_size)}
  {}

  ~mmap_artifact()
  {
    if (m_ptr && m_size > 0)
      munmap(m_ptr, m_size);

    if (m_fd >= 0)
      close(m_fd);
  }

  mmap_artifact(mmap_artifact&& other) noexcept
   : m_fd(other.m_fd)
   , m_size(other.m_size)
   , m_ptr(other.m_ptr)
  {
    other.m_fd = -1;
    other.m_size = 0;
    other.m_ptr = nullptr;
  }
  mmap_artifact&
  operator=(mmap_artifact&& other) noexcept
  {
    this->~mmap_artifact();
    m_fd = other.m_fd;
    m_size = other.m_size;
    m_ptr = other.m_ptr;
    other.m_fd = -1;
    other.m_size = 0;
    other.m_ptr = nullptr;
    return *this;
  }
  mmap_artifact(const mmap_artifact&) = delete;
  mmap_artifact& operator=(const mmap_artifact&) = delete;

  span<char>
  get_span() const
  {
    if (!m_ptr || m_size == 0)
      return {};

    return span<char>(static_cast<char*>(m_ptr), m_size);
  }

private:
  static int
  open_fd(const std::string& path)
  {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
      throw std::runtime_error("artifacts::repository: open failed: " + path);

    return fd;
  }

  static std::size_t
  get_size(int fd)
  {
    struct stat st;
    if (fstat(fd, &st) != 0)
    {
      close(fd);
      throw std::runtime_error("artifacts::repository: fstat failed");
    }

    std::size_t size = static_cast<std::size_t>(st.st_size);
    if (size == 0)
    {
      close(fd);
      throw std::runtime_error("artifacts::repository: cannot map empty file");
    }

    return size;
  }

  static void*
  map_region(int fd, std::size_t size)
  {
    void* ptr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED)
    {
      close(fd);
      throw std::runtime_error("artifacts::repository: mmap failed");
    }

    return ptr;
  }
};

}  // namespace artifacts::detail

#endif // ARTIFACTS_DETAIL_LINUX_MMAP_H
