// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#define XCL_DRIVER_DLL_EXPORT  // in same dll as exported xrt apis
#define XRT_API_SOURCE         // in same dll as coreutil
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil

#include "buffer_dumper.h"

#include "core/common/message.h"
#include "core/common/time.h"
#include "core/common/utils.h"

#include <cstring>
#include <stdexcept>

namespace xrt_core {
  buffer_dumper::
  buffer_dumper(dumper_config config)
    : m_config(std::move(config))
    , m_data_size(m_config.chunk_size - m_config.metadata_size)
    , m_dumped_counts(m_config.num_chunks, 0)
    , m_file_streams(m_config.num_chunks)
  {
    for (size_t i = 0; i < m_config.num_chunks; i++) {
      std::string filename = m_config.dump_file_prefix +
                             xrt_core::get_timestamp_for_filename() + "_" +
                             std::to_string(xrt_core::utils::get_pid()) + "_" +
                             std::to_string(i) + ".bin";

      m_file_streams[i].open(filename, std::ios::out | std::ios::binary);
      if (!m_file_streams[i].is_open()) {
        throw std::runtime_error("Failed to open dump file " + filename);
      }
    }

    m_dump_thread = std::thread(&buffer_dumper::dumping_loop, this);
  }

  buffer_dumper::
  ~buffer_dumper()
  {
    m_stop_thread = true;
    m_cv.notify_one();
    
    if (m_dump_thread.joinable()) {
      m_dump_thread.join();
    }

    // Flush the remaining data
    // catch exceptions to avoid throwing in destructor
    try {
      flush();
    }
    catch (const std::exception& e) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "buffer_dumper",
          std::string{"Failed to flush dump buffer: "} + e.what());
    }

    for (auto& fs : m_file_streams) {
      if (fs.is_open()) {
        fs.flush();
        fs.close();
      }
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
  dump_chunk_data(size_t chunk_index, size_t start, size_t length)
  {
    auto chunk = m_config.dump_buffer.map<uint8_t*>() + (chunk_index * m_config.chunk_size);
    std::ofstream& fs = m_file_streams[chunk_index];
    const size_t start_offset = (start % m_data_size) + m_config.metadata_size;
    const size_t bytes_to_end = m_config.chunk_size - start_offset;

    if (length <= bytes_to_end) {
      fs.write(reinterpret_cast<const char*>(chunk + start_offset),
               static_cast<std::streamsize>(length));
    } else {
      fs.write(reinterpret_cast<const char*>(chunk + start_offset),
               static_cast<std::streamsize>(bytes_to_end));
      fs.write(reinterpret_cast<const char*>(chunk + m_config.metadata_size),
               static_cast<std::streamsize>(length - bytes_to_end));
    }

    fs.flush();
  }

  void
  buffer_dumper::
  process_chunks()
  {
    std::lock_guard<std::mutex> lock(m_dump_mutex);
    
    // Map buffer once for all chunks
    auto* base_ptr = m_config.dump_buffer.map<uint8_t*>();
    
    for (size_t i = 0; i < m_config.num_chunks; i++)
    {
      uint8_t* chunk = base_ptr + (i * m_config.chunk_size);

      size_t logged_count = read_logged_count(chunk);
      size_t& dumped_count = m_dumped_counts[i];
      size_t logged_wrap = logged_count / m_data_size;
      size_t dumped_wrap = dumped_count / m_data_size;

      if (logged_count > dumped_count && logged_wrap > dumped_wrap)
        throw std::runtime_error("Overwrite detected in chunk : " + std::to_string(i) +
                                 ", dump buffer corrupted.");

      if (dumped_count != logged_count) {
        size_t to_dump = logged_count - dumped_count;
        dump_chunk_data(i, dumped_count, to_dump);
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
          std::unique_lock<std::mutex> lock(m_dump_mutex);
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