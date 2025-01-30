// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#include "smi.h"

namespace shim_edge::smi {

const std::vector<std::tuple<std::string, std::string, std::string>>& 
smi_edge::
get_validate_test_desc() const 
{
  static const std::vector<std::tuple<std::string, std::string, std::string>> validate_test_desc = {
    {"aux-connection", "Check if auxiliary power is connected", "common"},
    {"dma", "Run dma test", "common"},
    {"thostmem-bw", "Run 'bandwidth kernel' when host memory is enabled", "common"},
    {"m2m", "Run M2M test", "common"},
    {"mem-bw", "Run 'bandwidth kernel' and check the throughput", "common"},
    {"p2p", "Run P2P test", "common"},
    {"pcie-link", "Check if PCIE link is active", "common"},
    {"sc-version","Check if SC firmware is up-to-date", "common"},
    {"verify", "Run 'Hello World' kernel test", "common"}
  };
  return validate_test_desc;
}

const std::vector<std::tuple<std::string, std::string, std::string>>& 
smi_edge::
get_examine_report_desc() const 
{
  static const std::vector<std::tuple<std::string, std::string, std::string>> examine_report_desc = {
    {"aie", "AIE metadata in xclbin", "common"},
    {"aiemem", "AIE memory tile information", "common"},
    {"aieshim", "AIE shim tile status", "common"},
    {"debug-ip-status", "Status of Debug IPs present in xclbin loaded on device", "common"},
    {"dynamic-regions", "Information about the xclbin and the compute units", "common"},
    {"electrical", "Electrical and power sensors present on the device", "common"},
    {"error", "Asyncronus Error present on the device", "common"},
    {"firewall", "Firewall status", "common"},
    {"mailbox", "Mailbox metrics of the device", "common"},
    {"mechanical", "Mechanical sensors on and surrounding the device", "common"},
    {"memory", "Memory information present on the device", "common"},
    {"pcie-info", "Pcie information of the device", "common"},
    {"qspi-status", "QSPI write protection status", "common"},
    {"thermal", "Thermal sensors present on the device", "common"}
  };
  return examine_report_desc;
}

std::string
get_smi_config()
{
  // Create an instance of the derived class
  shim_edge::smi::smi_edge smi_instance;

  // Call the get_smi_config method
  return smi_instance.get_smi_config();
}
} // namespace shim_edge::smi
