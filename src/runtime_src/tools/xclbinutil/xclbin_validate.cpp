// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
//
// xclbin_validate - Walk and validate the section table of an xclbin file.
//
// Usage:  xclbin_validate <file.xclbin> [<file2.xclbin> ...]
//
// Exit codes:
//   0  all files valid
//   1  one or more files had validation errors
//   2  usage / I/O error

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

// Pull in the xclbin layout structures without dragging in kernel headers.
// Define the minimal stubs needed so the header compiles standalone.
#ifndef __linux__
#  define __linux__
#  define STUB_LINUX
#endif
#include <cstdlib>   // must precede xclbin.h when not in kernel
#include <uuid/uuid.h>
#include "xrt/detail/xclbin.h"
#ifdef STUB_LINUX
#  undef __linux__
#  undef STUB_LINUX
#endif

// ---- helpers ---------------------------------------------------------------

static const char* section_kind_name(uint32_t kind)
{
  // Matches axlf_section_kind enum order.
  static const char* names[] = {
    "BITSTREAM",              // 0
    "CLEARING_BITSTREAM",     // 1
    "EMBEDDED_METADATA",      // 2
    "FIRMWARE",               // 3
    "DEBUG_DATA",             // 4
    "SCHED_FIRMWARE",         // 5
    "MEM_TOPOLOGY",           // 6
    "CONNECTIVITY",           // 7
    "IP_LAYOUT",              // 8
    "DEBUG_IP_LAYOUT",        // 9
    "DESIGN_CHECK_POINT",     // 10
    "CLOCK_FREQ_TOPOLOGY",    // 11
    "MCS",                    // 12
    "BMC",                    // 13
    "BUILD_METADATA",         // 14
    "KEYVALUE_METADATA",      // 15
    "USER_METADATA",          // 16
    "DNA_CERTIFICATE",        // 17
    "PDI",                    // 18
    "BITSTREAM_PARTIAL_PDI",  // 19
    "PARTITION_METADATA",     // 20
    "EMULATION_DATA",         // 21
    "SYSTEM_METADATA",        // 22
    "SOFT_KERNEL",            // 23
    "ASK_FLASH",              // 24
    "AIE_METADATA",           // 25
    "ASK_GROUP_TOPOLOGY",     // 26
    "ASK_GROUP_CONNECTIVITY", // 27
    "SMARTNIC",               // 28
    "AIE_RESOURCES",          // 29
    "OVERLAY",                // 30
    "VENDER_METADATA",        // 31
    "AIE_PARTITION",          // 32
    "IP_METADATA",            // 33
    "AIE_RESOURCES_BIN",      // 34
    "AIE_TRACE_METADATA",     // 35
  };
  constexpr uint32_t count = sizeof(names) / sizeof(names[0]);
  return (kind < count) ? names[kind] : "<unknown>";
}

// Returns true if [offset, offset+size) fits strictly within [0, file_length).
static bool range_valid(uint64_t offset, uint64_t size, uint64_t file_length)
{
  // Guard against offset+size overflow before the comparison.
  if (size > file_length)
    return false;
  if (offset > file_length - size)
    return false;
  return true;
}

// ---- per-file validator ----------------------------------------------------

static bool validate_file(const std::string& path)
{
  std::cout << "=== " << path << " ===\n";
  bool ok = true;

  // ---- open and read -------------------------------------------------------
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) {
    std::cerr << "  ERROR: cannot open file\n";
    return false;
  }

  const uint64_t file_size = static_cast<uint64_t>(f.tellg());
  f.seekg(0);

  // Minimum: fixed axlf fields before the flexible section-header array.
  // sizeof(axlf) includes one sentinel axlf_section_header[] element in
  // userspace builds, but the real minimum is everything up to m_sections[0].
  // Compute it as: offsetof(axlf, m_sections).
  const uint64_t axlf_fixed_size = offsetof(axlf, m_sections);

  if (file_size < axlf_fixed_size) {
    std::cerr << "  ERROR: file too small to contain axlf header ("
              << file_size << " < " << axlf_fixed_size << " bytes)\n";
    return false;
  }

  // Read the entire file into memory so every offset check is just a
  // comparison rather than a seek — and we don't risk partial reads later.
  std::vector<char> buf(file_size);
  if (!f.read(buf.data(), static_cast<std::streamsize>(file_size))) {
    std::cerr << "  ERROR: read failed\n";
    return false;
  }

  // ---- magic ---------------------------------------------------------------
  const auto* hdr = reinterpret_cast<const axlf*>(buf.data());
  if (std::strncmp(hdr->m_magic, "xclbin2", 7) != 0) {
    // Not necessarily fatal — print the actual bytes so the user can see.
    char magic[9] = {};
    std::memcpy(magic, hdr->m_magic, 8);
    std::cerr << "  WARNING: unexpected magic '" << magic << "' (expected 'xclbin2')\n";
    ok = false;
  }

  // ---- header-declared total length ----------------------------------------
  const uint64_t declared_length = hdr->m_header.m_length;
  std::cout << "  declared length : " << declared_length << " bytes\n";
  std::cout << "  actual size     : " << file_size << " bytes\n";

  if (declared_length != file_size) {
    std::cerr << "  WARNING: declared m_length (" << declared_length
              << ") != actual file size (" << file_size << ")\n";
    // Use the smaller of the two as the safe bound for all subsequent checks.
    ok = false;
  }
  const uint64_t safe_bound = std::min(declared_length, file_size);

  // ---- section count -------------------------------------------------------
  const uint32_t num_sections = hdr->m_header.m_numSections;
  std::cout << "  num_sections    : " << num_sections << "\n";

  if (num_sections == 0) {
    std::cerr << "  ERROR: m_numSections is 0\n";
    return false;
  }
  if (num_sections > XCLBIN_MAX_NUM_SECTION) {
    std::cerr << "  ERROR: m_numSections (" << num_sections
              << ") exceeds XCLBIN_MAX_NUM_SECTION (" << XCLBIN_MAX_NUM_SECTION << ")\n";
    return false;
  }

  // Section header table must fit within the file.
  const uint64_t section_table_size =
      static_cast<uint64_t>(num_sections) * sizeof(axlf_section_header);

  if (!range_valid(axlf_fixed_size, section_table_size, safe_bound)) {
    std::cerr << "  ERROR: section header table [" << axlf_fixed_size << ", "
              << axlf_fixed_size + section_table_size
              << ") exceeds safe bound (" << safe_bound << ")\n";
    return false;
  }

  // ---- walk each section header --------------------------------------------
  std::cout << "\n"
            << "  " << std::left
            << std::setw(4)  << "idx"
            << std::setw(26) << "kind"
            << std::setw(17) << "name"
            << std::setw(18) << "offset"
            << std::setw(18) << "size"
            << "status\n"
            << "  " << std::string(95, '-') << "\n";

  const auto* sections = reinterpret_cast<const axlf_section_header*>(
      buf.data() + axlf_fixed_size);

  for (uint32_t i = 0; i < num_sections; ++i) {
    const auto& s = sections[i];

    // Safely extract the null-terminated section name (field is 16 bytes).
    char name[17] = {};
    std::memcpy(name, s.m_sectionName, 16);

    const uint64_t offset = s.m_sectionOffset;
    const uint64_t size   = s.m_sectionSize;
    const char* kind_str  = section_kind_name(s.m_sectionKind);

    std::string status = "OK";
    bool section_ok = true;

    // An empty section (offset==0 && size==0) is allowed.
    if (offset == 0 && size == 0) {
      status = "empty (skipped)";
    }
    else {
      // Offset must be at least past the fixed header.
      if (offset < axlf_fixed_size) {
        status = "ERROR: offset overlaps axlf header";
        section_ok = false;
      }
      // [offset, offset+size) must lie within the file.
      else if (!range_valid(offset, size, safe_bound)) {
        status = "ERROR: section data out of bounds";
        section_ok = false;
      }
      // 8-byte alignment required by the xclbin spec.
      else if (offset % 8 != 0) {
        status = "WARNING: offset not 8-byte aligned";
        section_ok = false;
      }
    }

    if (!section_ok)
      ok = false;

    std::cout << "  "
              << std::left  << std::setw(4)  << i
              << std::setw(26) << kind_str
              << std::setw(17) << name
              << std::right << std::setw(16) << std::hex << std::showbase << offset << "  "
              << std::setw(16) << size
              << std::dec << std::noshowbase
              << "  " << status << "\n";
  }

  std::cout << "\n  Result: " << (ok ? "VALID" : "INVALID") << "\n\n";
  return ok;
}

// ---- main ------------------------------------------------------------------

int main(int argc, char* argv[])
{
  if (argc < 2) {
    std::cerr << "Usage: xclbin_validate <file.xclbin> [<file2.xclbin> ...]\n";
    return 2;
  }

  bool all_ok = true;
  for (int i = 1; i < argc; ++i)
    all_ok &= validate_file(argv[i]);

  return all_ok ? 0 : 1;
}
