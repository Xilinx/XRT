// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#define XCL_DRIVER_DLL_EXPORT  // in same dll as xrt_bo.h
#define XRT_API_SOURCE         // in same dll as api
#include "buffer_dumper.h"
#include "core/common/message.h"
#include "core/common/time.h"
#include "core/common/utils.h"
#include "core/common/uc_log_schema.h"

#include <cstring>
#include <stdexcept>
#include <sstream>
#include <array>

namespace xrt_core {

buffer_dumper::
buffer_dumper(config cfg)
  : m_config(std::move(cfg))
  , m_data_size(m_config.chunk_size - m_config.metadata_size)
  , m_dumped_counts(m_config.num_chunks, 0)
  , m_file_streams(m_config.num_chunks)
{
  // Files are opened lazily in get_or_open_stream() when first data is available
  // start the background thread to dump the data
  m_done_future = m_done_promise.get_future();
  m_dump_thread = std::thread(&buffer_dumper::dumping_loop, this);
}

std::ofstream&
buffer_dumper::
get_or_open_stream(size_t chunk_index)
{
  std::ofstream& fs = m_file_streams[chunk_index];
  if (fs.is_open())
    return fs;

  if (m_session_timestamp.empty())
    m_session_timestamp = xrt_core::get_timestamp_for_filename();

  std::string filename = m_config.dump_file_prefix + "_" + m_session_timestamp + "_" +
                         std::to_string(xrt_core::utils::get_pid()) + "_" +
                         std::to_string(chunk_index) + (m_config.dump_bin_format ? ".bin" : ".txt");

  fs.open(filename, std::ios::out | std::ios::binary);
  if (!fs.is_open())
    throw std::runtime_error("Failed to open dump file " + filename);

  return fs;
}

buffer_dumper::
~buffer_dumper()
{
  // Flush the remaining data
  // catch exceptions to avoid throwing in destructor
  try {
    m_stop_thread = true;
    m_cv.notify_one();
    // Wait for dump thread to finish before detaching.
    m_done_future.wait();
    // detach instead of join to avoid deadlock under DLL_PROCESS_DETACH
    m_dump_thread.detach();
    flush();
  }
  catch (const std::exception& e) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "buffer_dumper",
        std::string{"Error during cleanup: "} + e.what());
  }
}

size_t
buffer_dumper::
read_logged_count(uint8_t* chunk)
{
  size_t count = 0;
  std::memcpy(&count, chunk + m_config.count_offset, m_config.count_size);
  return count;
}

void
buffer_dumper::
dump_chunk_data(size_t chunk_index, size_t start, size_t length, uint8_t* chunk)
{
  std::ofstream& fs = get_or_open_stream(chunk_index);

  // Check if stream is in good state before writing
  if (!fs.good()) {
    throw std::runtime_error("File stream for chunk " + std::to_string(chunk_index) +
                             " is in bad state before write");
  }

  // Calculate start offset and bytes to end for circular buffer wrapping
  const size_t start_offset = (start % m_data_size) + m_config.metadata_size;
  const size_t bytes_to_end = m_config.chunk_size - start_offset;

  // UC log dump in binary format
  if (m_config.dump_bin_format) {
    // Always write/update metadata since this function is called when there's new data
    // and metadata(count) gets updated when there is new data
    // Seek to beginning and write/update metadata
    fs.seekp(0);
    fs.write(reinterpret_cast<const char*>(chunk),
            static_cast<std::streamsize>(m_config.metadata_size));

    if (!fs)
      throw std::runtime_error("Failed to write metadata for chunk " + std::to_string(chunk_index));

    // Seek to end to append actual data
    fs.seekp(0, std::ios::end);

    if (length <= bytes_to_end) { // data doesn't wrap around
      fs.write(reinterpret_cast<const char*>(chunk + start_offset),
              static_cast<std::streamsize>(length));

      if (!fs)
        throw std::runtime_error("Failed to write " + std::to_string(length) +
                                " bytes to chunk " + std::to_string(chunk_index));
    }
    else {
      // data wraps around
      // write the first part
      fs.write(reinterpret_cast<const char*>(chunk + start_offset),
              static_cast<std::streamsize>(bytes_to_end));

      if (!fs)
        throw std::runtime_error("Failed to write first part (" + std::to_string(bytes_to_end) +
                                " bytes) to chunk " + std::to_string(chunk_index));

      // write the wrapped part
      fs.write(reinterpret_cast<const char*>(chunk + m_config.metadata_size),
              static_cast<std::streamsize>(length - bytes_to_end));

      if (!fs)
        throw std::runtime_error("Failed to write wrapped part (" +
                                std::to_string(length - bytes_to_end) +
                                " bytes) to chunk " + std::to_string(chunk_index));
    }

    fs.flush();
    if (!fs)
      throw std::runtime_error("Failed to flush chunk " + std::to_string(chunk_index));

    return;
  }

  // UC log parsing and writing log messages to file using log schema
  try {
    std::ostringstream parsed_stream;
    size_t parsed_bytes = 0;
    // Iterate and parse each log entry in dumped data
    while (parsed_bytes < length) {
      size_t entry_offset = 0;

      // Handle circular buffer wrapping
      if (parsed_bytes < bytes_to_end)
        entry_offset = start_offset + parsed_bytes; // No wrap around
      else
        entry_offset = m_config.metadata_size + (parsed_bytes - bytes_to_end); // Wrapped around

      // Current log entry in chunk
      const uint8_t* entry = chunk + entry_offset;
      // Extract log entry fields
      log_entry log;
      std::memcpy(&log, entry, sizeof(log_entry));

      // Format log message using log schema
      // If log_id is not found in log schema, use default format string
      constexpr std::array<const char*, 3> default_formats = {
        "unknown !\n",
        "unknown %d !!\n",
        "unknown %d unknown %d !!!\n"
      };
      auto log_schema_it = uc_log_schema.logs.find(log.log_id);
      const char* log_format = (log_schema_it != uc_log_schema.logs.end())
        ? log_schema_it->second.c_str()
        : default_formats[std::min(static_cast<std::size_t>(log.length - (offsetof(log_entry, argument1) / sizeof(uint32_t))),
                          (default_formats.size() - 1))];

      // log marker [<seconds>.<nanoseconds>] [CERT]
      uint64_t timestamp_ns = (static_cast<uint64_t>(log.ts_high) << 32) | log.ts_low; // NOLINT(cppcoreguidelines-avoid-magic-numbers)
      parsed_stream << "[" << xrt_core::get_timestamp_for_uc_log(timestamp_ns) << "] [CERT] ";

      std::array<char, 1024> log_message{}; // NOLINT(cppcoreguidelines-avoid-magic-numbers)
      if (log.length == (offsetof(log_entry, argument1) / sizeof(uint32_t)))
      { // Log message without arguments
        parsed_stream << log_format;
      }
      else if (log.length == (offsetof(log_entry, argument2) / sizeof(uint32_t)))
      { // Log message with one argument
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
        static_cast<void>(std::snprintf(log_message.data(), log_message.size(), log_format, log.argument1));
        parsed_stream << log_message.data();
      }
      else if (log.length == (sizeof(log_entry) / sizeof(uint32_t)))
      { // Log message with two arguments
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
        static_cast<void>(std::snprintf(log_message.data(), log_message.size(), log_format, log.argument1, log.argument2));
        parsed_stream << log_message.data();
      }
      else
      { // Unexpected entry length
        xrt_core::message::send(xrt_core::message::severity_level::warning, "buffer_dumper",
                                "Invalid UC log entry length: " + std::to_string(log.length));
      }

      parsed_bytes += m_config.metadata_size;
    }

    // Separator between dumper reads; the separator marks read boundaries and indicates
    // log entries may have been lost at the boundary before the next read.
    parsed_stream << "[0.000000000] [CERT] [Dumper]--------------[Separator]--------------\n";

    // Append parsed output to file
    fs.seekp(0, std::ios::end);
    fs << parsed_stream.str();

    if (!fs)
      throw std::runtime_error("Failed to write parsed UC log for chunk " + std::to_string(chunk_index));
  }
  catch (const std::exception& e) {
    // Log parsing error, log warning and continue
    xrt_core::message::send(xrt_core::message::severity_level::warning, "buffer_dumper",
                            std::string{"UC log parsing failed: "} + e.what());
  }

  fs.flush();
  if (!fs)
    throw std::runtime_error("Failed to flush chunk " + std::to_string(chunk_index));
}

void
buffer_dumper::
process_chunks_no_lock()
{
  // Map buffer once for all chunks
  auto base_ptr = m_config.dump_buffer.map<uint8_t*>();

  for (size_t i = 0; i < m_config.num_chunks; i++)
  {
    const size_t chunk_offset = i * m_config.chunk_size;
    auto chunk = base_ptr + chunk_offset;

    // sync only the metadata for the current chunk to read the logged count
    m_config.dump_buffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE, m_config.metadata_size, chunk_offset);

    size_t logged_count = read_logged_count(chunk);
    size_t& dumped_count = m_dumped_counts[i];
    size_t logged_wrap = logged_count / m_data_size;
    size_t dumped_wrap = dumped_count / m_data_size;

    // Overwrite detected; catch up and resume dumping from current position.
    if (logged_count > dumped_count && logged_wrap > dumped_wrap)
      dumped_count = logged_count;

    if (dumped_count != logged_count) {
      size_t to_dump = logged_count - dumped_count;
      const size_t start_offset = (dumped_count % m_data_size) + m_config.metadata_size;
      const size_t bytes_to_end = m_config.chunk_size - start_offset;

      // Sync only the data range we need to dump
      if (to_dump <= bytes_to_end) {
        // Data doesn't wrap, sync contiguous range
        m_config.dump_buffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE, to_dump, chunk_offset + start_offset);
      }
      else {
        // Data wraps around, sync two ranges
        m_config.dump_buffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE, bytes_to_end, chunk_offset + start_offset);
        m_config.dump_buffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE, to_dump - bytes_to_end, chunk_offset + m_config.metadata_size);
      }

      dump_chunk_data(i, dumped_count, to_dump, chunk);
      dumped_count = logged_count;
    }
  }
}

void
buffer_dumper::
process_chunks()
{
  std::lock_guard lock(m_dump_mutex);
  process_chunks_no_lock();
}

void
buffer_dumper::
dumping_loop()
{
  while (!m_stop_thread) {
    try {
      std::unique_lock lock(m_dump_mutex);
      m_cv.wait_for(lock, std::chrono::milliseconds(m_config.dump_interval_ms),
                    [this] { return m_stop_thread.load(); });

      if (!m_stop_thread)
        process_chunks_no_lock();
    }
    catch (const std::exception& e) {
      // Log error but keep thread running
      xrt_core::message::send(xrt_core::message::severity_level::warning, "buffer_dumper",
                              std::string{"Error in dumping loop: "} + e.what());
    }
  }

  m_done_promise.set_value();
}

void
buffer_dumper::
flush()
{
  // process chunks to dump the remaining data
  process_chunks();
}

} // xrt_core
