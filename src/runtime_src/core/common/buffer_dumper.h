// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025-2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrtcore_util_buffer_dumper_h_
#define xrtcore_util_buffer_dumper_h_
#include "core/common/json/nlohmann/json.hpp"
#include "core/common/uc_log.h"
#include "core/include/xrt/xrt_bo.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <future>
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
    bool dump_bin_format = false;     // Dump in binary format when enabled
    bool enable_dumper_thread = false; // Enable background dumper thread
    // Sink for parsed uC log text output:
    // "file" (default), "syslog", "console", or "null"
    // Ignored when dump_bin_format=true (binary always writes to file)
    std::string uc_log_dump = "file";
  };

  // Log entry layout: shared definition in uc_log.h
  using log_entry = uc_log_entry;

private:
  config m_config;
  size_t m_data_size = 0;
  std::vector<size_t> m_dumped_counts;
  std::thread m_dump_thread;
  std::promise<void> m_done_promise;
  std::future<void>  m_done_future;
  std::atomic<bool> m_stop_thread{false};
  std::atomic<bool> m_thread_created{false};
  std::mutex m_dump_mutex;
  std::condition_variable m_cv;
  std::vector<std::ofstream> m_file_streams;
  std::string m_session_timestamp;  // Set on first file open for consistent naming

  bool
  needs_file_streams() const
  { return m_config.dump_bin_format || m_config.uc_log_dump == "file" || m_config.uc_log_dump.empty(); }

  // Open file for chunk lazily when first data is available; returns stream
  std::ofstream&
  get_or_open_stream(size_t chunk_index);

  // Read the logged count from chunk metadata
  size_t
  read_logged_count(uint8_t* chunk);

  // Write raw binary data (metadata + payload) to file
  void
  dump_chunk_data_binary(size_t chunk_index, size_t start_offset, size_t bytes_to_end,
                         size_t length, uint8_t* chunk);

  // Parse log entries from chunk data into a formatted string
  std::string
  parse_log_entries(size_t start_offset, size_t bytes_to_end, size_t length, uint8_t* chunk);

  // Route parsed log text to the configured sink (file, syslog, console, or null)
  void
  dispatch_parsed_log(size_t chunk_index, const std::string& text);

  // Coordinate binary or parsed log dump for a chunk
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

/**
 * dtrace_buffer_dumper - Coalesces per-run dtrace JSON results for a hw context.
 *
 * Accumulates JSON in memory up to max_bytes. On overflow, spills buffered runs
 * to disk, logs a warning, and disables further appends for this instance.
 */
class dtrace_buffer_dumper
{
public:
  struct config
  {
    uint32_t slot_idx = 0;
    size_t max_bytes = 0;
  };

  explicit
  dtrace_buffer_dumper(config cfg);

  // Flush result on destruction
  ~dtrace_buffer_dumper();

  // Delete copy and move operations
  dtrace_buffer_dumper(const dtrace_buffer_dumper&) = delete;
  dtrace_buffer_dumper(dtrace_buffer_dumper&&) = delete;
  dtrace_buffer_dumper& operator=(const dtrace_buffer_dumper&) = delete;
  dtrace_buffer_dumper& operator=(dtrace_buffer_dumper&&) = delete;

  // Append run's JSON result under the given key
  // Spill buffered results on overflow and disable further appends
  void
  append(const std::string& key, const std::string& result_json);

  // Write buffered results to file at end of hw context lifetime
  void
  flush();

private:
  config m_config;
  nlohmann::ordered_json m_results;
  bool m_spilled = false;

  // Write buffered JSON to file, log success, and clear the buffer
  void
  write_to_file();
};

} // xrt_core

#endif
