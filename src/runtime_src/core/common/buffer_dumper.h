// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrtcore_util_buffer_dumper_h_
#define xrtcore_util_buffer_dumper_h_
#include "core/include/xrt/xrt_bo.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace xrt_core {

/**
 * buffer_dumper - Asynchronously dumps device buffer contents periodically
 *
 * Monitors a device buffer organized into chunks and incrementally writes new
 * data to timestamped binary files. Each chunk is dumped to a separate file
 * with its metadata header followed by data payload.
 *
 * Key features:
 * - Takes in metadata as input that explains how to parse the data
 * - Operates asynchronously as background thread periodically checks for new data
 * - Granular device-to-host synchronization for efficiency
 * - Handles circular buffer wrapping within chunks
 * - Dynamically updates metadata header as data accumulates
 * - Thread-safe with mutex protection
 */
class buffer_dumper
{
public:
  // Configuration struct for buffer dumper behavior and layout
  struct config
  {
    size_t chunk_size = 0;            // Total chunk size (metadata + data)
    size_t metadata_size = 0;         // Metadata header size
    size_t count_offset = 0;          // Offset of count field in metadata
    size_t count_size = 0;            // Count field size
    size_t num_chunks = 0;            // Number of chunks to monitor
    size_t dump_interval_ms = 0;      // Polling interval in ms
    std::string dump_file_prefix;     // Output file prefix
    xrt::bo dump_buffer;              // xrt buffer object to dump
  };


private:
  config m_config;
  size_t m_data_size = 0;
  std::vector<size_t> m_dumped_counts;
  std::thread m_dump_thread;
  std::atomic<bool> m_stop_thread{false};
  std::mutex m_dump_mutex;
  std::condition_variable m_cv;
  std::vector<std::ofstream> m_file_streams;

  // Read the logged count from chunk metadata
  size_t
  read_logged_count(uint8_t* chunk);

  // Write chunk data to file (metadata header + new data payload)
  void
  dump_chunk_data(size_t chunk_index, size_t start, size_t length, uint8_t* chunk);

  // Process all chunks without acquiring lock (caller must hold m_dump_mutex)
  void
  process_chunks_no_lock();

  // Process all chunks with lock acquisition
  void
  process_chunks();

  // Background thread function that periodically checks for new data
  void
  dumping_loop();

public:
  // Reads configuration and opens files for dumping
  // Starts background dumping thread
  explicit
  buffer_dumper(config cfg);

  // Stop background thread and flush remaining data
  ~buffer_dumper();

  // Delete copy and move operations
  buffer_dumper(const buffer_dumper&) = delete;
  buffer_dumper(buffer_dumper&&) = delete;
  buffer_dumper& operator=(const buffer_dumper&) = delete;
  buffer_dumper& operator=(buffer_dumper&&) = delete;

  // Synchronously flush all pending data
  void
  flush();
};

} // xrt_core

#endif
