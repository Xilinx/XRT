// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __TestRunner_h_
#define __TestRunner_h_

// Local - Include Files
#include "JSONConfigurable.h"
#include "xrt/xrt_device.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/ptree.hpp>

// System - Include Files
#include <string>

class TestRunner : public JSONConfigurable {
  public:
    virtual boost::property_tree::ptree run(std::shared_ptr<xrt_core::device> dev) = 0;
    virtual void set_param(const std::string key, const std::string value){}
    bool is_explicit() const { return m_explicit; };
    virtual bool getConfigHidden() const { return is_explicit(); };
    const void set_xclbin_path(std::string path) { m_xclbin = path; };
    const std::string & get_name() const { return m_name; };
    const std::string & getConfigName() const { return get_name(); };
    virtual const std::string& getConfigDescription() const { return m_description; };
    boost::property_tree::ptree get_test_header();
    std::string findXclbinPath( const std::shared_ptr<xrt_core::device>& _dev,
                      boost::property_tree::ptree& _ptTest);
    std::string findDPUPath( const std::shared_ptr<xrt_core::device>& _dev,
                              boost::property_tree::ptree& _ptTest, const std::string dpu_name);

  // Child class helper methods
  protected:
    TestRunner(const std::string & test_name, const std::string & description, 
            const std::string & xclbin = "", bool is_explicit = false);
    void runTestCase( const std::shared_ptr<xrt_core::device>& _dev, const std::string& py,
             boost::property_tree::ptree& _ptTest);
    void logger(boost::property_tree::ptree& ptree, const std::string& tag, const std::string& msg);
    bool search_and_program_xclbin(const std::shared_ptr<xrt_core::device>& dev, boost::property_tree::ptree& ptTest);
    std::string findPlatformPath(const std::shared_ptr<xrt_core::device>& _dev,
                  boost::property_tree::ptree& _ptTest);
    std::vector<std::string> findDependencies( const std::string& test_path,
                      const std::string& ps_kernel_name);
    int validate_binary_file(const std::string& binaryfile);

    const std::string test_token_skipped = "SKIPPED";
    const std::string test_token_failed = "FAILED";
    const std::string test_token_passed = "PASSED";
    std::string m_xclbin;
 
  private:
    std::string searchLegacyXclbin(const uint16_t vendor, const std::string& dev_name, 
                      boost::property_tree::ptree& _ptTest);
    std::string searchSSV2Xclbin(const std::string& logic_uuid,
                      boost::property_tree::ptree& _ptTest);
  
  //variables
  private:
    std::string m_name;
    std::string m_description;
    bool m_explicit;

};
  
#endif
