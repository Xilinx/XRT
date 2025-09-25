// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestRunner_h_
#define TestRunner_h_

// Local - Include Files
#include "core/common/query_requests.h"
#include "core/common/runner/runner.h"
#include "JSONConfigurable.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_bo.h"
#include "xrt/experimental/xrt_ext.h"
#include "xrt/experimental/xrt_module.h"
#include "xrt/experimental/xrt_elf.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/ptree.hpp>

// System - Include Files
#include <memory>
#include <filesystem>
#include <string>
#include <vector>
#include <map>

// Forward declarations
namespace xrt_core { 
  class archive; 
}

class TestRunner : public JSONConfigurable {
  public:
    virtual boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&) = 0;
    
    // Overloaded version with archive support for individual test implementations
    // We'll remove the default run implementation once all tests overload this version
    virtual boost::property_tree::ptree 
    run(const std::shared_ptr<xrt_core::device>& dev, 
        const xrt_core::archive* /*archive*/) {
      // Default implementation ignores archive and calls original run method
      return run(dev);
    }
    
    boost::property_tree::ptree startTest(const std::shared_ptr<xrt_core::device>&, 
                                          const xrt_core::archive* archive = nullptr);
    
    virtual void set_param(const std::string&, const std::string&) {}
    bool is_explicit() const { return m_explicit; };
    virtual bool getConfigHidden() const { return is_explicit(); };
    const void set_xclbin_path(std::string path) { m_xclbin = path; };
    const std::string & get_name() const { return m_name; };
    const std::string & getConfigName() const { return get_name(); };
    virtual const std::string& getConfigDescription() const { return m_description; };
    boost::property_tree::ptree get_test_header();

  // Child class helper methods
  protected:
    TestRunner(const std::string & test_name, const std::string & description,
               const std::string & xclbin = "", bool is_explicit = false);
    void runPyTestCase( const std::shared_ptr<xrt_core::device>& _dev, const std::string& py,
             boost::property_tree::ptree& _ptTest);
    std::vector<std::string> findDependencies( const std::string& test_path,
                      const std::string& ps_kernel_name);
    xrt::kernel get_kernel(const xrt::hw_context& hwctx, const std::string& kernel_or_elf);
    xrt::kernel get_kernel(const xrt::hw_context& hwctx, const std::string& kernel_name, 
      const std::string& elf_path); 

    // Archive helper method for extracting artifacts
    xrt_core::runner::artifacts_repository 
    extract_artifacts_from_archive(const xrt_core::archive* archive, 
                                   const std::vector<std::string>& artifact_names,
                                   boost::property_tree::ptree& ptree);

    std::string m_xclbin;
 
  //variables
  private:
    xrt_core::query::xclbin_name::type m_xclbin_type;
    std::string m_name;
    std::string m_description;
    bool m_explicit;
  
  public:
    virtual ~TestRunner() = default;

};
  
#endif
