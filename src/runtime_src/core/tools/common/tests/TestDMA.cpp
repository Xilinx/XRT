// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestDMA.h"
#include "core/common/utils.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "core/tools/common/BusyBar.h"

#include "core/pcie/common/dmatest.h"
namespace XBU = XBUtilities;


constexpr uint64_t operator"" _gb(unsigned long long v)  { return 1024u * 1024u * 1024u * v; }

// ----- C L A S S   M E T H O D S -------------------------------------------
TestDMA::TestDMA()
  : TestRunner("dma", 
                "Run dma test", 
                "bandwidth.xclbin"){}

boost::property_tree::ptree
TestDMA::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  XBU::BusyBar busy_bar("Running Test", std::cout);
  busy_bar.start(XBU::is_escape_codes_disabled());

  ptree.put("status", test_token_skipped);
  if (!search_and_program_xclbin(dev, ptree))
    return ptree;

  // get DDR bank count from mem_topology if possible
  auto membuf = xrt_core::device_query<xrt_core::query::mem_topology_raw>(dev);
  auto mem_topo = reinterpret_cast<const mem_topology*>(membuf.data());

  auto dma_thr = xrt_core::device_query_default<xrt_core::query::dma_threads_raw>(dev, {});

  if (dma_thr.size() == 0)
    return ptree;

  auto is_host_mem = [](std::string tag) {
    return tag.compare(0,4,"HOST") == 0;
  };

  for (auto& mem : boost::make_iterator_range(mem_topo->m_mem_data, mem_topo->m_mem_data + mem_topo->m_count)) {
    auto midx = std::distance(mem_topo->m_mem_data, &mem);
    if (is_host_mem(std::string(reinterpret_cast<const char*>(mem.m_tag))))
      continue;

    if (mem.m_type == MEM_STREAMING)
      continue;

    if (!mem.m_used)
      continue;

    std::stringstream run_details;
    logger(ptree, "Details", (boost::format("Buffer size - '%s'") % xrt_core::utils::unit_convert(m_block_size)).str());

    // check if the bank has enough memory to allocate
    // m_size is in KB so convert block_size (bytes) to KB for comparison
    if (mem.m_size < (m_block_size/1024)) {
      logger(ptree, "Details", boost::str(boost::format(
	      "The bank does not have enough memory to allocate. Use lower '%s' value. \n") % "block-size"));
      continue;
    }

    size_t totalSize = 0;
    if (xrt_core::device_query<xrt_core::query::pcie_vendor>(dev) == ARISTA_ID)
      totalSize = 0x20000000; // 512 MB
    else
      totalSize = std::min((mem.m_size * 1024), 2_gb); // minimum of mem size in bytes and 2 GB

    xcldev::DMARunner runner(dev, m_block_size, static_cast<unsigned int>(midx), totalSize);
    try {
      runner.run(run_details);
      ptree.put("status", test_token_passed);
      std::string line;
      while(std::getline(run_details, line))
        logger(ptree, "Details", line);
    }
    catch (xrt_core::error& ex) {
      ptree.put("status", test_token_failed);
      logger(ptree, "Error", ex.what());
    }
  }
  busy_bar.finish();
  return ptree;
}

/*
 * Pass in custom parameters for dma test
*/
void 
TestDMA::set_param(const std::string key, const std::string value)
{
  if(key == "block-size") {
    try {
        m_block_size = static_cast<size_t>(std::stoll(value, nullptr, 0));
      }
      catch (const std::invalid_argument&) {
        std::cerr << boost::format(
          "ERROR: The parameter '%s' value '%s' is invalid for the test '%s'. Please specify and integer byte block-size.'\n")
          % "block-size" % value % "dma" ;
        throw xrt_core::error(std::errc::operation_canceled);
      }
  }
  
}
