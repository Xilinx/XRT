// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef FIRMWARE_LOG_CONFIG_H
#define FIRMWARE_LOG_CONFIG_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "core/common/json/nlohmann/json.hpp"

namespace xrt_core::tools::xrt_smi {
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
    std::string name;
    std::map<std::string, uint32_t> enumerator_to_value;
    std::map<uint32_t, std::string> value_to_enumerator;

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
   * @brief Constructor - loads configuration from JSON file
   * @param json_file_path Path to firmware_log.json file
   *
   * Loads and parses the JSON file, populating enums and structures.
   * Sets valid_ to true if successful, false otherwise.
   */
    firmware_log_config(const std::string& json_file_path); // Initializes using static parse APIs

  /**
   * @brief Get parsed enumerations
   * @return Map of enumeration name to EnumInfo
   */
  const std::map<std::string, enum_info>& 
  get_enums() const 
  { 
    return enums;  
  }

  /**
   * @brief Get parsed structures
   * @return Map of structure name to StructureInfo
   */
  const std::map<std::string, structure_info>& 
  get_structures() const 
  { 
    return structures; 
  }

  /**
   * @brief Calculate the header size based on the ipu_log_message_header structure
   * @param structures Map of structure name to StructureInfo
   * @return Calculated header size in bytes
   */
  static size_t calculate_header_size(const std::map<std::string, structure_info>& structures);

  /**
   * @brief Get the calculated header size
   * @return Header size in bytes
   */
  size_t 
  get_header_size() const 
  { 
    return header_size; 
  }

private:
  /**
   * @brief Parse the root JSON object
   * @param  nlohmann::json object
   */
  void parse_json(const nlohmann::json&);

  /**
   * @brief Parse enumerations section from JSON
   * @param enums_json JSON object for enumerations
   */
  static std::map<std::string, enum_info> parse_enums(const nlohmann::json& config);

  /**
   * @brief Parse structures section from JSON
   * @param structs_json JSON object for structures
   */
  static std::map<std::string, structure_info> parse_structures(const nlohmann::json& config);

private:
  nlohmann::json config; ///< Raw JSON configuration
  std::map<std::string, enum_info> enums; ///< Parsed enumerations
  std::map<std::string, structure_info> structures; ///< Parsed structures
  size_t header_size; // Stores the calculated header size
};

} // namespace xrt_core::tools::xrt_smi

#endif // FIRMWARE_LOG_CONFIG_H
