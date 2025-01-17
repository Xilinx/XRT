// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#include "smi.h"

namespace xrt_core::smi {

static constexpr std::string_view xrt_smi_config =
 R"(
 {
  "subcommands":
  [{
    "name" : "validate",
    "description" : "Validates the given device by executing the platform's validate executable.",
    "tag" : "basic",
    "options" :
    [
      {
        "name": "device",
        "alias": "d",
        "description": "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest",
        "tag": "basic",
        "default_value": "",
        "option_type": "common", 
        "value_type" : "string"
      },
      {
        "name": "format",
        "alias": "f",
        "description": "Report output format",
        "tag": "basic",
        "default_value": "JSON",
        "option_type": "common", 
        "value_type" : "string"
      },
      {
        "name": "output",
        "alias": "o",
        "description" : "Direct the output to the given file",
        "tag": "basic",
        "default_value": "",
        "option_type": "common", 
        "value_type" : "string"
      },
      {
        "name": "help",
        "alias": "h",
        "description" : "Help to use this sub-command",
        "tag": "basic",
        "default_value": "",
        "option_type": "common", 
        "value_type" : "none"
      },
      {
        "name" : "run",
        "alias" : "r",
        "description" : "Run a subset of the test suite",
        "tag" : "basic",
        "option_type": "common",
        "value_type" : "array",
        "options" : [
          {
            "name" : "latency",
            "tag" : "basic",
            "description" : "Run end-to-end latency test"
          },
          {
            "name" : "throughput",
            "tag" : "basic",
            "description" : "Run end-to-end throughput test"
          },
          {
            "name" : "cmd-chain-latency",
            "tag" : "basic",
            "description" : "Run command chain latency test"
          },
          {
            "name" : "cmd-chain-throughput",
            "tag" : "basic",
            "description" : "Run end-to-end throughput test using command chaining"
          },
          {
            "name" : "df-bw",
            "tag" : "basic",
            "description" : "Run dataflow bandwidth test"
          },
          {
            "name" : "tct-one-col",
            "tag" : "basic",
            "description" : "Run TCT test with one column"
          },
          {
            "name" : "tct-all-col",
            "tag" : "basic",
            "description" : "Run TCT test with all columns"
          },
          {
            "name" : "gemm",
            "tag" : "basic",
            "description" : "Run GEMM test"
          },
          {
            "name" : "aie-reconfig-overhead",
            "tag" : "advanced",
            "description" : "Run AIE reconfiguration overhead test"
          },
          {
            "name" : "spatial-sharing-overhead",
            "tag" : "advanced",
            "description" : "Run spatial sharing overhead test"
          },
          {
            "name" : "temporal-sharing-overhead",
            "tag" : "advanced",
            "description" : "Run temporal sharing overhead test"
          }
        ]
      },
      {
        "name" : "path",
        "alias" : "p",
        "description" : "Path to the directory containing validate xclbins",
        "tag" : "basic",
        "default_value": "",
        "option_type": "hidden",
        "value_type" : "string"
      },
      {
        "name" : "param",
        "description" : "Extended parameter for a given test. Format: <test-name>:<key>:<value>",
        "tag" : "basic",
        "option_type": "hidden",
        "default_value": "",
        "value_type" : "string"
      },
      {
        "name" : "pmode",
        "description" : "Specify which power mode to run the benchmarks in. Note: Some tests might be unavailable for some modes",
        "tag" : "basic",
        "option_type": "hidden",
        "default_value": "",
        "value_type" : "string"
      }
    ]
  },
  {
    "name" : "examine",
    "tag" : "basic",
    "description": "This command will 'examine' the state of the system/device and will generate a report of interest in a text or JSON format.",
    "options":
    [
      {
        "name": "device",
        "alias": "d",
        "description": "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest",
        "tag": "basic",
        "default_value": "",
        "option_type": "common",
        "value_type": "string"
      },
      {
        "name": "format",
        "alias": "f",
        "description": ["Report output format. Valid values are:",
                        "\n\tJSON        - Latest JSON schema",
                        "\n\tJSON-2020.2 - JSON 2020.2 schema"
                        ],
        "tag": "basic",
        "default_value": "",
        "option_type": "common",
        "value_type": "string"
      },
      {
        "name": "output",
        "alias": "o",
        "description" : "Direct the output to the given file",
        "tag": "basic",
        "default_value": "",
        "option_type": "common", 
        "value_type" : "string"
      },
      {
        "name": "help",
        "alias": "h",
        "description" : "Help to use this sub-command",
        "tag": "basic",
        "default_value": "",
        "option_type": "common", 
        "value_type" : "none"
      },
      {
        "name": "report",
        "alias": "r",
        "description": ["The type of report to be produced. Reports currently available are:",
                         "\n\taie             - AIE metadata in xclbin",
                         "\n\taiemem          - AIE memory tile information",
                         "\n\taieshim         - AIE shim tile status",
                         "\n\tdebug-ip-status - Status of Debug IPs present in xclbin loaded on device",
                         "\n\tdynamic-regions - Information about the xclbin and the compute units",
                         "\n\telectrical      - Electrical and power sensors present on the device",
                         "\n\terror           - Asyncronus Error present on the device",
                         "\n\tfirewall        - Firewall status",
                         "\n\tmailbox         - Mailbox metrics of the device",
                         "\n\tmechanical      - Mechanical sensors on and surrounding the device",
                         "\n\tmemory          - Memory information present on the device",
                         "\n\tpcie-info       - Pcie information of the device",
                         "\n\tqspi-status     - QSPI write protection status",
                         "\n\tthermal         - Thermal sensors present on the device"
                        ],
        "tag": "basic",
        "option_type": "common",
        "value_type": "array",
        "options": [
          {
            "name": "aie",
            "tag": "basic",
            "description": "AIE metadata in xclbin"
          },
          {
            "name": "aiemem",
            "tag": "basic",
            "description": "AIE memory tile information"
          },
          {
            "name": "aieshim",
            "tag": "basic",
            "description": "AIE shim tile status"
          },
          {
            "name": "debug-ip-status",
            "tag": "basic",
            "description": "Status of Debug IPs present in xclbin loaded on device"
          },
          {
            "name": "dynamic-regions",
            "tag": "basic",
            "description": "Information about the xclbin and the compute units"
          },
          {
            "name": "electrical",
            "tag": "basic",
            "description": "Electrical and power sensors present on the device"
          },
          {
            "name": "error",
            "tag": "basic",
            "description": "Asyncronus Error present on the device"
          },
          {
            "name": "firewall",
            "tag": "basic",
            "description": "Firewall status"
          },
          {
            "name": "mailbox",
            "tag": "basic",
            "description": "Mailbox metrics of the device"
          },
          {
            "name": "mechanical",
            "tag": "basic",
            "description": "Mechanical sensors on and surrounding the device"
          },
          {
            "name": "memory",
            "tag": "basic",
            "description": "Memory information present on the device"
          },
          {
            "name": "pcie-info",
            "tag": "basic",
            "description": "Pcie information of the device"
          },
          {
            "name": "qspi-status",
            "tag": "basic",
            "description": "QSPI write protection status"
          },
          {
            "name": "thermal",
            "tag": "basic",
            "description": "Thermal sensors present on the device"
          }
        ]
      },
      {
        "name": "element",
        "alias": "e",
        "description" : "Filters individual elements(s) from the report. Format: '/<key>/<key>/...'",
        "tag": "basic",
        "option_type": "hidden", 
        "value_type" : "array"
      } 
    ]
  },
  {
    "name" : "configure",
    "tag" : "devl",
    "description" : "Device and host configuration.",
    "options" :
    [
      {
        "name": "device",
        "description": "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest",
        "tag": "devl",
        "value_type" : "string"
      },
      {
        "name": "pmode",
        "description": "Modes: default, powersaver, balanced, performance, turbo",
        "tag": "basic",
        "value_type": "string"
      }
    ]
  }]
}
)"; 


std::string 
get_smi_config()
{
  return std::string(xrt_smi_config);
}
} // namespace xrt_core::smi
