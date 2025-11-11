// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrtcore_util_buffer_dumper_h_
#define xrtcore_util_buffer_dumper_h_

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/include/xrt/xrt_bo.h"

namespace xrt_core {

struct dumper_config
{
  size_t chunk_size = 0;
  size_t metadata_size = 0;
  size_t count_offset = 0;
  size_t count_size = 0;
  size_t num_chunks = 0;
  size_t dump_interval_ms = 0;
  std::string dump_file_prefix;
  xrt::bo dump_buffer;
};


class buffer_dumper
{
private:
  dumper_config m_config;
  size_t m_data_size = 0;
  std::vector<size_t> m_dumped_counts;
  std::thread m_dump_thread;
  std::atomic<bool> m_stop_thread{false};
  std::mutex m_dump_mutex;
  std::condition_variable m_cv;
  std::vector<std::ofstream> m_file_streams;

  size_t
  read_logged_count(uint8_t* chunk);

  void
  dump_chunk_data(size_t chunk_index, size_t start, size_t length);
  
  void
  process_chunks();

  void
  dumping_loop();

public :
  explicit
  buffer_dumper(dumper_config config);

  ~buffer_dumper();

  // Delete copy and move operations
  buffer_dumper(const buffer_dumper&) = delete;
  buffer_dumper(buffer_dumper&&) = delete;
  buffer_dumper& operator=(const buffer_dumper&) = delete;
  buffer_dumper& operator=(buffer_dumper&&) = delete;

  void
  flush();
};

} // xrt_core

#endif
