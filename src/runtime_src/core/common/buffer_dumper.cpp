// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "buffer_dumper.h"
#include "core/common/message.h"
#include "core/common/time.h"
#include "core/common/utils.h"

#include <cstring>
#include <stdexcept>

namespace xrt_core {

buffer_dumper::
buffer_dumper(config cfg)
  : m_config(std::move(cfg))
  , m_data_size(m_config.chunk_size - m_config.metadata_size)
  , m_dumped_counts(m_config.num_chunks, 0)
  , m_file_streams(m_config.num_chunks)
{
  // for each chunk, open a file to dump the data
  for (size_t i = 0; i < m_config.num_chunks; i++) {
    std::string filename = m_config.dump_file_prefix + "_" +
                           xrt_core::get_timestamp_for_filename() + "_" +
                           std::to_string(xrt_core::utils::get_pid()) + "_" +
                           std::to_string(i) + ".bin";

    m_file_streams[i].open(filename, std::ios::out | std::ios::binary);
    if (!m_file_streams[i].is_open()) {
      throw std::runtime_error("Failed to open dump file " + filename);
    }
  }

  // start the background thread to dump the data
  m_dump_thread = std::thread(&buffer_dumper::dumping_loop, this);
}

buffer_dumper::
~buffer_dumper()
{
  // Flush the remaining data
  // catch exceptions to avoid throwing in destructor
  try {
    m_stop_thread = true;
    m_cv.notify_one();
    m_dump_thread.join();
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
  std::ofstream& fs = m_file_streams[chunk_index];

  // Check if stream is in good state before writing
  if (!fs.good()) {
    throw std::runtime_error("File stream for chunk " + std::to_string(chunk_index) +
                             " is in bad state before write");
  }

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

  const size_t start_offset = (start % m_data_size) + m_config.metadata_size;
  const size_t bytes_to_end = m_config.chunk_size - start_offset;

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
}

void
buffer_dumper::
process_chunks()
{
  std::lock_guard lock(m_dump_mutex);

  // Map buffer once for all chunks
  auto* base_ptr = m_config.dump_buffer.map<uint8_t*>();

  for (size_t i = 0; i < m_config.num_chunks; i++)
  {
    const size_t chunk_offset = i * m_config.chunk_size;
    uint8_t* chunk = base_ptr + chunk_offset;

    // sync only the metadata for the current chunk to read the logged count
    m_config.dump_buffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE, m_config.metadata_size, chunk_offset);

    size_t logged_count = read_logged_count(chunk);
    size_t& dumped_count = m_dumped_counts[i];
    size_t logged_wrap = logged_count / m_data_size;
    size_t dumped_wrap = dumped_count / m_data_size;

    if (logged_count > dumped_count && logged_wrap > dumped_wrap)
      throw std::runtime_error("Overwrite detected in chunk: " + std::to_string(i) +
                               ", dump buffer corrupted.");

    if (dumped_count != logged_count) {
      size_t to_dump = logged_count - dumped_count;
      const size_t start_offset = (dumped_count % m_data_size) + m_config.metadata_size;
      const size_t bytes_to_end = m_config.chunk_size - start_offset;

      // Sync only the data range we need to dump
      if (to_dump <= bytes_to_end) {
        // Data doesn't wrap, sync contiguous range
        m_config.dump_buffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE, to_dump, chunk_offset + start_offset);
      } else {
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
dumping_loop()
{
  while (!m_stop_thread) {
    try {
      {
        std::unique_lock lock(m_dump_mutex);
        m_cv.wait_for(lock, std::chrono::milliseconds(m_config.dump_interval_ms),
                      [this] { return m_stop_thread.load(); });
      }

      if (!m_stop_thread)
        process_chunks();
    }
    catch (const std::exception& e) {
      // Log error but keep thread running
      xrt_core::message::send(xrt_core::message::severity_level::warning, "buffer_dumper",
                              std::string{"Error in dumping loop: "} + e.what());
    }
  }
}

void
buffer_dumper::
flush()
{
  // process chunks to dump the remaining data
  process_chunks();
}

} // xrt_core
