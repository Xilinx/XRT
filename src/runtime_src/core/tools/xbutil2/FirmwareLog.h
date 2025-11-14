// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef FIRMWARE_LOG_H
#define FIRMWARE_LOG_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "core/common/json/nlohmann/json.hpp"
#include "core/common/device.h"

namespace xrt_core::tools::xrt_smi {

constexpr size_t bits_per_byte = 8;
constexpr size_t byte_alignment = 7;
constexpr size_t bits_per_uint64 = 64;
constexpr size_t hex_width_64 = 16;

/**
 * @brief Parser for firmware_log.json configuration
 *
 * Loads enumerations and structures for firmware log message parsing.
 */

/**
 * @brief FirmwareLogConfig parses firmware_log.json for log message formats
 *
 * Loads enumerations and structures for firmware log message parsing.
 */
class firmware_log_config 
{
public:
  /**
   * @brief EnumInfo holds enumeration name and value mappings
   *
   * name: Name of the enumeration
   * name_to_value: Map from enumerator name to value
   * value_to_name: Map from value to enumerator name
   */
  struct enum_info {
    std::string m_name;
    std::unordered_map<std::string, uint32_t> enumerator_to_value;
    std::unordered_map<uint32_t, std::string> value_to_enumerator;

    // Helper to get enumerator name from value
    std::string get_enumerator_name(uint32_t value) const;
    // Helper to get value from enumerator name
    uint32_t get_enumerator_value(const std::string& name) const;
  };

  /**
   * @brief FieldInfo describes a field in a structure
   *
   * name: Field name
   * type: Field type (e.g., uint32_t)
   * width: Bit width of the field
   * format: Format string for display
   * enumeration: Associated enumeration name (if any)
   */
  struct field_info {
    std::string name;
    std::string type;
    uint32_t width;
    std::string format;
    std::string enumeration;
  };

  /**
   * @brief StructureInfo describes a structure and its fields
   *
   * name: Structure name
   * fields: List of FieldInfo for each field
   */
  struct structure_info {
    std::string name;
    std::vector<field_info> fields;
  };

public:
  /**
   * @brief Constructor - loads configuration from JSON content
   * @param json_content JSON content as string
   *
   * Parses the JSON content directly, populating enums and structures.
   */
    explicit
    firmware_log_config(nlohmann::json json_config); // Initializes using static parse APIs

  /**
   * @brief Load firmware log configuration from device
   * @param device Device to load configuration from
   * @return Optional firmware log config (nullopt if loading fails)
   */
  static std::optional<firmware_log_config>
  load_config(const xrt_core::device* device);

  /**
   * @brief Get parsed enumerations
   * @return Map of enumeration name to EnumInfo
   */
  const std::unordered_map<std::string, enum_info>& 
  get_enums() const 
  { 
    return m_enums;  
  }

  /**
   * @brief Get parsed structures
   * @return Map of structure name to StructureInfo
   */
  const std::unordered_map<std::string, structure_info>& 
  get_structures() const 
  { 
    return m_structures; 
  }

  /**
   * @brief Calculate the header size based on the ipu_log_message_header structure
   * @param structures Map of structure name to StructureInfo
   * @return Calculated header size in bytes
   */
  size_t 
  calculate_header_size(const std::unordered_map<std::string, structure_info>& structures);

  /**
   * @brief Get the calculated header size
   * @return Header size in bytes
   */
  size_t 
  get_header_size() const 
  { 
    return m_header_size; 
  }

  /**
   * @brief Get the log message header structure
   * @return Reference to the ipu_log_message_header structure info
   * @throws std::runtime_error if header structure not found
   */
  const structure_info& 
  get_log_header() const;

private:
  /**
   * @brief Parse the root JSON object
   * @param  nlohmann::json object
   */
  void 
  parse_json(const nlohmann::json&);

  /**
   * @brief Parse enumerations section from JSON
   * @param enums_json JSON object for enumerations
   */
  std::unordered_map<std::string, enum_info> 
  parse_enums(const nlohmann::json& config);

  /**
   * @brief Parse structures section from JSON
   * @param structs_json JSON object for structures
   */
  std::unordered_map<std::string, structure_info> 
  parse_structures(const nlohmann::json& config);

private:
  nlohmann::json m_config; ///< Raw JSON configuration
  std::unordered_map<std::string, enum_info> m_enums; ///< Parsed enumerations
  std::unordered_map<std::string, structure_info> m_structures; ///< Parsed structures
  size_t m_header_size; // Stores the calculated header size
};

/**
 * @brief Firmware log parser for XRT devices
 * 
 * This class provides an interface for parsing firmware log data.
 * It encapsulates all parsing logic. The parser is configured with a firmware_log_config object
 * that defines the structure and format of the log entries.
 */
class firmware_log_parser {
public:
  /**
   * @brief Construct a new firmware log parser
   * 
   * @param config Firmware log configuration object containing structure definitions,
   *               field layouts, and enumeration mappings
   */
  explicit 
  firmware_log_parser(firmware_log_config config);

  /**
   * @brief Parse firmware log buffer and generate formatted string directly
   * 
   * @param data_ptr Pointer to the firmware log data buffer
   * @param buf_size Total size of the data buffer in bytes
   * @param prefix String prefix for each line (default: empty)
   * @return std::string Tab-separated formatted log output
   * 
   * This method processes the entire firmware log buffer, parsing each entry
   * and outputting formatted text directly for maximum performance.
   */
  std::string
  parse(const uint8_t* data_ptr, size_t buf_size) const;

  /**
   * @brief Get formatted header row as string
   * @return std::string Formatted header row
   */
  std::string
  get_header_row() const;

private:

  /**
   * @brief Format a single parsed entry as string
   * @param entry_data Parsed entry data from parse_entry
   * @return std::string Formatted row string
   */
  std::string
  format_entry_row(const std::vector<std::string>& entry_data) const;


  /**
   * @brief Extract field value from bit-packed firmware log header
   * 
   * @param data_ptr Pointer to raw data buffer
   * @param byte_offset Starting byte offset
   * @param bit_offset Starting bit offset
   * @param bit_width Number of bits to extract
   * @return uint64_t Extracted field value
   */
  uint64_t 
  extract_value(const uint8_t* data_ptr, 
                size_t byte_offset, 
                size_t bit_offset, 
                size_t bit_width) const;

  /**
   * @brief Format field value with enum resolution if applicable
   * 
   * @param field Field information containing name and enumeration
   * @param value Raw field value
   * @return std::string Formatted field value with enum name if applicable
   */
  std::string 
  format_value(const firmware_log_config::field_info& field, 
               uint64_t value) const;

  /**
   * @brief Parse message data from log entry payload
   * 
   * @param data_ptr Pointer to the raw data buffer
   * @param msg_offset Offset where message data begins
   * @param argc Argument count field value (unused but kept for compatibility)
   * @param buf_size Total buffer size for bounds checking
   * @return std::string Parsed message with trailing newlines removed
   * 
   * This method extracts null-terminated string messages from log entries,
   * with newline cleanup.
   */
  std::string 
  parse_message(const uint8_t* data_ptr, 
                size_t msg_offset, 
                size_t buf_size) const;

  /**
   * @brief Parse a complete log entry (header + message)
   * 
   * @param data_ptr Pointer to the raw data buffer
   * @param offset Offset where log entry begins
   * @param buf_size Total buffer size for bounds checking
   * @return std::vector<std::string> Vector of parsed field values and message
   * 
   * This method parses a complete log entry by:
   * 1. Extracting all header fields according to structure definition
   * 2. Parsing the message payload
   * 3. Returning organized field data for table display
   */
  std::vector<std::string> 
  parse_entry(const uint8_t* data_ptr, size_t offset, 
                  size_t buf_size) const;



  /**
   * @brief Calculate entry size for buffer traversal
   * 
   * @param argc Argument count field value
   * @param format Log format field value (full vs concise)
   * @return uint32_t Total size of log entry in bytes
   * 
   */
  uint32_t 
  calculate_entry_size(uint32_t argc, uint32_t format) const;

private:
  firmware_log_config m_config;  
  firmware_log_config::structure_info m_header; 
  uint32_t m_header_size; // Size of log entry header in bytes
  
  // Field indices computed from config
  std::unordered_map<std::string, size_t> m_field_indices;

  // Column headers mapping for display
  std::unordered_map<std::string, std::string> m_columns;
  
  // Column widths for alignment
  std::unordered_map<std::string, size_t> m_column_widths;

  /**
   * @brief Create field indices map from config
   */
  std::unordered_map<std::string, size_t>
  create_field_indices(const firmware_log_config& config);

  /**
   * @brief Create column widths map for alignment
   */
  std::unordered_map<std::string, size_t>
  create_column_widths(const std::unordered_map<std::string, std::string>& columns);
};

} // namespace xrt_core::tools::xrt_smi

#endif // FIRMWARE_LOG_H
