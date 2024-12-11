// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "Testp2p.h"
#include "TestValidateUtilities.h"
#include "core/common/unistd.h"
#include "core/common/memalign.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"
namespace XBU = XBUtilities;

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {

static bool
p2ptest_set_or_cmp(char *boptr, size_t size, const std::vector<char>& valid_data, bool set)
{
  // Validate the page size against the parameters
  const size_t page_size = static_cast<size_t>(xrt_core::getpagesize());
  assert((size % page_size) == 0);
  assert(page_size >= valid_data.size());

  // Calculate the number of pages that will be accessed
  const size_t num_of_pages = size / page_size;
  assert(size >= valid_data.size());

  // Go through each page to be accessed and perform the desired action
  for (size_t page_index = 0; page_index < num_of_pages; page_index++) {
    const size_t mem_index = page_index * page_size;
    if (set)
      std::memcpy(&(boptr[mem_index]), valid_data.data(), valid_data.size());
    else if (!std::equal(valid_data.begin(), valid_data.end(), &(boptr[mem_index]))) // Continue unless mismatch
      return false;
  }
  return true;
}

static bool
p2ptest_chunk(xrt_core::device* handle, char *boptr, uint64_t dev_addr, uint64_t size)
{
  char *buf = nullptr;

  if (xrt_core::posix_memalign(reinterpret_cast<void **>(&buf), xrt_core::getpagesize(), size))
    return false;

  // Generate the valid data vector
  // Perform a memory write larger than 512 bytes to trigger a write combine
  // The chosen size is 1024
  const size_t valid_data_size = 1024;
  std::vector<char> valid_data(valid_data_size);
  std::fill(valid_data.begin(), valid_data.end(), 'A');

  // Perform one large write
  const auto buf_size = xrt_core::getpagesize();
  p2ptest_set_or_cmp(buf, buf_size, valid_data, true);
  try {
    handle->unmgd_pwrite(buf, buf_size, dev_addr);
  }
  catch (const std::exception&) {
    return false;
  }
  if (!p2ptest_set_or_cmp(boptr, buf_size, valid_data, false))
    return false;

  // Default to testing with small write to reduce test time
  valid_data.clear();
  valid_data.push_back('A');
  p2ptest_set_or_cmp(buf, size, valid_data, true);
  try {
    handle->unmgd_pwrite(buf, size, dev_addr);
  }
  catch (const std::exception&) {
    return false;
  }
  if (!p2ptest_set_or_cmp(boptr, size, valid_data, false))
    return false;

  valid_data.clear();
  valid_data.push_back('B');
  p2ptest_set_or_cmp(boptr, size, valid_data, true);
  try {
    handle->unmgd_pread(buf, size, dev_addr);
  }
  catch (const std::exception&) {
    return false;
  }
  if (!p2ptest_set_or_cmp(buf, size, valid_data, false))
    return false;

  free(buf);
  return true;
}

//Since no DMA platforms don't have a DMA engine, we copy p2p buffer
//to host only buffer and run the test through m2m
static bool
p2ptest_chunk_no_dma(xrt::device& device, xrt::bo bo_p2p, size_t bo_size, int bank)
{
  // testing p2p write flow host -> device
  // Allocate a host only buffer
  auto boh = xrt::bo(device, bo_size, XCL_BO_FLAGS_HOST_ONLY, bank);
  auto boh_ptr = boh.map<char*>();

  // Populate host buffer with 'A'
  p2ptest_set_or_cmp(boh_ptr, bo_size, {'A'}, true);

  // Use m2m IP to move data into p2p (required for no DMA test)
  bo_p2p.copy(boh);

  // Create p2p bo mapping
  auto bo_p2p_ptr = bo_p2p.map<char*>();

  // Verify p2p buffer has 'A' inside
  if(!p2ptest_set_or_cmp(bo_p2p_ptr, bo_size, {'A'}, false))
    return false;

  // testing p2p read flow device -> host
  // Populate p2p buffer with 'B'
  p2ptest_set_or_cmp(bo_p2p_ptr, bo_size, {'B'}, true);

  // Use m2m IP to move data into host buffer (required for no DMA test)
  boh.copy(bo_p2p);

  // Verify host buffer has 'B' inside
  if(!p2ptest_set_or_cmp(boh_ptr, bo_size, {'B'}, false))
    return false;

  return true;
}

} //end anonymous namespace

// ----- C L A S S   M E T H O D S -------------------------------------------
Testp2p::Testp2p()
  : TestRunner("p2p", 
                "Run P2P test", 
                "bandwidth.xclbin"){}

boost::property_tree::ptree
Testp2p::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  auto no_dma = xrt_core::device_query_default<xrt_core::query::nodma>(dev, 0);

  if (!XBValidateUtils::search_and_program_xclbin(dev, ptree)) {
    return ptree;
  }

  std::string msg;
  XBU::xclbin_lock xclbin_lock(dev.get());
  std::vector<std::string> config = xrt_core::device_query_default<xrt_core::query::p2p_config>(dev, {});

  std::tie(std::ignore, msg) = xrt_core::query::p2p_config::parse(config);

  if (msg.find("Error") == 0) {
    XBValidateUtils::logger(ptree, "Error", msg.substr(msg.find(':')+1));
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }
  else if (msg.find("Warning") == 0) {
    XBValidateUtils::logger(ptree, "Warning", msg.substr(msg.find(':')+1));
    ptree.put("status", XBValidateUtils::test_token_skipped);
    return ptree;
  }
  else if (!msg.empty()) {
    XBValidateUtils::logger(ptree, "Details", msg);
    ptree.put("status", XBValidateUtils::test_token_skipped);
    return ptree;
  }

  auto membuf = xrt_core::device_query<xrt_core::query::mem_topology_raw>(dev);
  auto mem_topo = reinterpret_cast<const mem_topology*>(membuf.data());
  std::string name = xrt_core::device_query<xrt_core::query::rom_vbnv>(dev);

  for (auto& mem : boost::make_iterator_range(mem_topo->m_mem_data, mem_topo->m_mem_data + mem_topo->m_count)) {
    auto midx = std::distance(mem_topo->m_mem_data, &mem);
    std::vector<std::string> sup_list = { "HBM", "bank", "DDR" };
    //p2p is not supported for DDR on u280
    if (name.find("_u280_") != std::string::npos)
      sup_list.pop_back();

    const std::string mem_tag(reinterpret_cast<const char *>(mem.m_tag));
    for (const auto& x : sup_list) {
      if (mem_tag.find(x) != std::string::npos && mem.m_used) {
        if (!p2ptest_bank(dev.get(), ptree, mem_tag, static_cast<unsigned int>(midx), 
                          mem.m_base_address, mem.m_size << 10, no_dma))
           break;
        else
          XBValidateUtils::logger(ptree, "Details", mem_tag +  " validated");
      }
    }
  }

  return ptree;
}

bool
Testp2p::p2ptest_bank(xrt_core::device* device, boost::property_tree::ptree& ptree, const std::string&,
             unsigned int mem_idx, uint64_t addr, uint64_t bo_size, uint32_t no_dma)
{
  const size_t chunk_size = 16 * 1024 * 1024; //16 MB
  const size_t mem_size = 256 * 1024 * 1024 ; //256 MB

  // Allocate p2p buffer
  auto xrt_device = xrt::device(device->get_device_id());
  auto boh = xrt::bo(xrt_device, bo_size, XCL_BO_FLAGS_P2P, mem_idx);
  auto boptr = boh.map<char*>();

  if (no_dma != 0) {
     if (!p2ptest_chunk_no_dma(xrt_device, boh, mem_size, mem_idx)) {
       ptree.put("status", XBValidateUtils::test_token_failed);
      XBValidateUtils::logger(ptree, "Error", boost::str(boost::format("P2P failed  on memory index %d")  % mem_idx));
      return false;
     }
  } else {
    for (uint64_t c = 0; c < bo_size; c += chunk_size) {
      if (!p2ptest_chunk(device, boptr + c, addr + c, chunk_size)) {
        ptree.put("status", XBValidateUtils::test_token_failed);
        XBValidateUtils::logger(ptree, "Error", boost::str(boost::format("P2P failed at offset 0x%x, on memory index %d") % c % mem_idx));
        return false;
      }
    }
  }
  ptree.put("status", XBValidateUtils::test_token_passed);
  return true;
}