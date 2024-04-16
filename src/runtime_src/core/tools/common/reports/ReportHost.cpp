// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

// Local - Include Files
#include "ReportHost.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/Table2D.h"
#include "core/common/sysinfo.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

// System - Include Files
#include <string>

#define BYTES_TO_MEGABYTES 0x100000ll
namespace xq = xrt_core::query;

void
ReportHost::getPropertyTreeInternal( const xrt_core::device * _pDevice,
                                     boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void
ReportHost::getPropertyTree20202( const xrt_core::device * /*_pDevice*/,
                                  boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree pt;
  boost::property_tree::ptree pt_os_info;
  boost::property_tree::ptree pt_xrt_info;

  xrt_core::sysinfo::get_os_info(pt_os_info);
  pt.add_child("os", pt_os_info);

  xrt_core::sysinfo::get_xrt_info(pt_xrt_info);
  pt.add_child("xrt", pt_xrt_info);

  auto dev_pt = XBUtilities::get_available_devices(m_is_user);
  pt.add_child("devices", dev_pt);

  // There can only be 1 root node
  _pt.add_child("host", pt);
}

static void
printAlveoDevices(const boost::property_tree::ptree& available_devices, std::ostream& _output)
{
  const Table2D::HeaderData bdf = {"BDF", Table2D::Justification::left};
  const Table2D::HeaderData colon = {":", Table2D::Justification::left};
  const Table2D::HeaderData vbnv = {"Shell", Table2D::Justification::left};
  const Table2D::HeaderData id = {"Logic UUID", Table2D::Justification::left};
  const Table2D::HeaderData instance = {"Device ID", Table2D::Justification::left};
  const Table2D::HeaderData ready = {"Device Ready*", Table2D::Justification::left};
  const std::vector<Table2D::HeaderData> table_headers = {bdf, colon, vbnv, id, instance, ready};
  Table2D device_table(table_headers);

  for (const auto& kd : available_devices) {
    const boost::property_tree::ptree& dev = kd.second;

    if (dev.get<std::string>("device_class") != xrt_core::query::device_class::enum_to_str(xrt_core::query::device_class::type::alveo))
      continue;

    const std::string bdf_string = "[" + dev.get<std::string>("bdf") + "]";
    const std::string ready_string = dev.get<bool>("is_ready", false) ? "Yes" : "No";
    const std::vector<std::string> entry_data = {bdf_string, ":", dev.get<std::string>("vbnv", "n/a"), dev.get<std::string>("id", "n/a"), dev.get<std::string>("instance", "n/a"), ready_string};
    device_table.addEntry(entry_data);
  }

  if (!device_table.empty()) {
    _output << boost::str(boost::format("%s\n") % device_table);
    _output << "* Devices that are not ready will have reduced functionality when using XRT tools\n";
  }
}

static void
printRyzenDevices(const boost::property_tree::ptree& available_devices, std::ostream& _output)
{
  const Table2D::HeaderData bdf = {"BDF", Table2D::Justification::left};
  const Table2D::HeaderData colon = {":", Table2D::Justification::left};
  const Table2D::HeaderData name = {"Name", Table2D::Justification::left};
  const std::vector<Table2D::HeaderData> table_headers = {bdf, colon, name};
  Table2D device_table(table_headers);

  for (const auto& kd : available_devices) {
    const boost::property_tree::ptree& dev = kd.second;

    if (dev.get<std::string>("device_class") != xrt_core::query::device_class::enum_to_str(xrt_core::query::device_class::type::ryzen))
      continue;

    const std::string bdf_string = "[" + dev.get<std::string>("bdf") + "]";
    const std::vector<std::string> entry_data = {bdf_string, ":", dev.get<std::string>("name", "n/a")};
    device_table.addEntry(entry_data);
  }

  if (!device_table.empty())
    _output << boost::str(boost::format("%s\n") % device_table);
}

void
ReportHost::writeReport(const xrt_core::device* /*_pDevice*/,
                        const boost::property_tree::ptree& _pt,
                        const std::vector<std::string>& /*_elementsFilter*/,
                        std::ostream& _output) const
{
  boost::property_tree::ptree empty_ptree;

  _output << "System Configuration\n";
  const boost::property_tree::ptree& available_devices = _pt.get_child("host.devices", empty_ptree);
  try {
    _output << boost::format("  %-20s : %s\n") % "OS Name" % _pt.get<std::string>("host.os.sysname");
    _output << boost::format("  %-20s : %s\n") % "Release" % _pt.get<std::string>("host.os.release");
    _output << boost::format("  %-20s : %s\n") % "Version" % _pt.get<std::string>("host.os.version");
    _output << boost::format("  %-20s : %s\n") % "Machine" % _pt.get<std::string>("host.os.machine");
    _output << boost::format("  %-20s : %s\n") % "CPU Cores" % _pt.get<std::string>("host.os.cores");
    _output << boost::format("  %-20s : %lld MB\n") % "Memory" % (std::strtoll(_pt.get<std::string>("host.os.memory_bytes").c_str(),nullptr,16) / BYTES_TO_MEGABYTES);
    _output << boost::format("  %-20s : %s\n") % "Distribution" % _pt.get<std::string>("host.os.distribution","N/A");
    const boost::property_tree::ptree& available_libraries = _pt.get_child("host.os.libraries", empty_ptree);
    for(auto& kl : available_libraries) {
      const boost::property_tree::ptree& lib = kl.second;
      std::string lib_name = lib.get<std::string>("name", "N/A");
      boost::algorithm::to_upper(lib_name);
      _output << boost::format("  %-20s : %s\n") % lib_name
          % lib.get<std::string>("version", "N/A");
    }
    _output << boost::format("  %-20s : %s\n") % "Model" % _pt.get<std::string>("host.os.model");
    _output << boost::format("  %-20s : %s\n") % "BIOS vendor" % _pt.get<std::string>("host.os.bios_vendor");
    _output << boost::format("  %-20s : %s\n") % "BIOS version" % _pt.get<std::string>("host.os.bios_version");
    _output << std::endl;
    _output << "XRT\n";
    _output << boost::format("  %-20s : %s\n") % "Version" % _pt.get<std::string>("host.xrt.version", "N/A");
    _output << boost::format("  %-20s : %s\n") % "Branch" % _pt.get<std::string>("host.xrt.branch", "N/A");
    _output << boost::format("  %-20s : %s\n") % "Hash" % _pt.get<std::string>("host.xrt.hash", "N/A");
    _output << boost::format("  %-20s : %s\n") % "Hash Date" % _pt.get<std::string>("host.xrt.build_date", "N/A");
    const boost::property_tree::ptree& available_drivers = _pt.get_child("host.xrt.drivers", empty_ptree);
    for(auto& drv : available_drivers) {
      const boost::property_tree::ptree& driver = drv.second;
      std::string drv_name = driver.get<std::string>("name", "N/A");
      boost::algorithm::to_upper(drv_name);
      std::string drv_hash = driver.get<std::string>("hash", "N/A");
      if (!boost::iequals(drv_hash, "N/A")) {
        _output << boost::format("  %-20s : %s, %s\n") % drv_name
            % driver.get<std::string>("version", "N/A") % driver.get<std::string>("hash", "N/A");
      } else
        _output << boost::format("  %-20s : %s\n") % drv_name % driver.get<std::string>("version", "N/A");
      if (boost::iequals(drv_name, "xclmgmt") && boost::iequals(driver.get<std::string>("version", "N/A"), "unknown"))
        _output << "WARNING: xclmgmt version is unknown. Is xclmgmt driver loaded? Or is MSD/MPD running?" << std::endl;
    }
  }
  catch (const boost::property_tree::ptree_error &ex) {
    throw xrt_core::error(boost::str(boost::format("%s. Please contact your Xilinx representative to fix the issue")
         % ex.what()));
  }

  try {
    if (!available_devices.empty())
      _output << boost::format("  %-20s : %s\n") % "Firmware Version" % available_devices.begin()->second.get<std::string>("firmware_version");
  }
  catch (const xrt_core::query::exception&) {
    //no device available
  }

  _output << std::endl << "Devices present\n";
  if (available_devices.empty())
    _output << "  0 devices found" << std::endl;

  printAlveoDevices(available_devices, _output);
  printRyzenDevices(available_devices, _output);
}
