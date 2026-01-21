// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "core/common/error.h"
#include <iostream>
#include <fstream>
#include <optional>
#include <string>
#include <functional>

namespace XBUtilities {

/**
 * @brief Helper class to manage output stream redirection for raw data
 * 
 * This class provides a unified way to handle output streams that can be
 * directed either to console (stdout) or to a file, based on an optional
 * file path parameter. It's used for --raw options in firmware logging
 * and event tracing commands.
 * 
 * Usage patterns:
 * - raw_option is nullopt -> Not in raw mode
 * - raw_option is empty string -> Raw mode to console
 * - raw_option is filename -> Raw mode to file
 */
class OutputStreamHelper {
public:
  /**
   * @brief Construct output stream helper
   * @param raw_option Optional string containing filename, or empty for console output
   */
  explicit OutputStreamHelper(const std::optional<std::string>& raw_option)
    : m_raw_option(raw_option)
    , m_is_raw(raw_option.has_value())
    , m_has_output_file(m_is_raw && !raw_option->empty())
    , m_stream_ref(init_stream())
  {
  }
  
  /**
   * @brief Destructor - ensures file stream is properly closed and flushed
   */
  ~OutputStreamHelper() {
    if (m_file_stream.is_open()) {
      m_file_stream.flush();
      m_file_stream.close();
    }
  }

  // Delete copy and move constructors/assignment operators
  OutputStreamHelper(const OutputStreamHelper&) = delete;
  OutputStreamHelper& operator=(const OutputStreamHelper&) = delete;
  OutputStreamHelper(OutputStreamHelper&&) = delete;
  OutputStreamHelper& operator=(OutputStreamHelper&&) = delete;

  /**
   * @brief Check if raw mode is enabled
   * @return true if raw mode is requested (console or file)
   */
  bool is_raw_mode() const { return m_is_raw; }

  /**
   * @brief Check if output is directed to a file
   * @return true if output goes to file, false if to console
   */
  bool has_output_file() const { return m_has_output_file; }

  /**
   * @brief Get the output stream reference
   * @return Reference to the appropriate output stream (cout or file)
   */
  std::ostream& get_stream() { return m_stream_ref.get(); }

  /**
   * @brief Get the filename (if writing to file)
   * @return Optional string containing filename
   */
  const std::optional<std::string>& get_filename() const { return m_raw_option; }

  /**
   * @brief Flush the output stream
   */
  void flush() {
    m_stream_ref.get().flush();
  }

private:
  std::optional<std::string> m_raw_option;
  bool m_is_raw;
  bool m_has_output_file;
  std::ofstream m_file_stream;
  std::reference_wrapper<std::ostream> m_stream_ref;

  // Helper to initialize the stream reference
  std::reference_wrapper<std::ostream> init_stream() {
    if (m_has_output_file) {
      // Try to open the file - create it if it doesn't exist
      m_file_stream.open(m_raw_option.value(), std::ios::out | std::ios::binary | std::ios::trunc);
      if (!m_file_stream.is_open()) {
        throw xrt_core::error(std::errc::io_error, 
                             "Failed to open output file: " + m_raw_option.value());
      }
      return std::ref(m_file_stream);
    }
    return std::ref(std::cout);
  }
};

} // namespace XBUtilities
