// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestRunner_h_
#define __TestRunner_h_

// Local - Include Files
#include "core/common/query_requests.h"
#include "JSONConfigurable.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_bo.h"
#include "xrt/experimental/xrt_ext.h"
#include "xrt/experimental/xrt_module.h"
#include "xrt/experimental/xrt_elf.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/ptree.hpp>

// System - Include Files
#include <filesystem>
#include <string>
#include <vector>

class TestRunner : public JSONConfigurable {
  public:
    virtual boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev) = 0;
    boost::property_tree::ptree startTest(std::shared_ptr<xrt_core::device> dev);
    virtual void set_param(const std::string key, const std::string value){}
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
