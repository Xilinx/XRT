// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2023 Xilinx, Inc
// Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdValidate.h"

#include "core/common/utils.h"
#include "core/common/query_requests.h"
#include "core/tools/common/EscapeCodes.h"
#include "core/tools/common/Process.h"
#include "tools/common/Report.h"
#include "tools/common/reports/platform/ReportPlatforms.h"
#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/TestRunner.h"
#include "tools/common/tests/TestAuxConnection.h"
#include "tools/common/tests/TestPcieLink.h"
#include "tools/common/tests/TestSCVersion.h"
#include "tools/common/tests/TestVerify.h"
#include "tools/common/tests/TestDMA.h"
#include "tools/common/tests/TestIOPS.h"
#include "tools/common/tests/TestBandwidthKernel.h"
#include "tools/common/tests/Testp2p.h"
#include "tools/common/tests/Testm2m.h"
#include "tools/common/tests/TestHostMemBandwidthKernel.h"
#include "tools/common/tests/TestBist.h"
#include "tools/common/tests/TestVcuKernel.h"
#include "tools/common/tests/TestAiePl.h"
#include "tools/common/tests/TestAiePs.h"
#include "tools/common/tests/TestPsPlVerify.h"
#include "tools/common/tests/TestPsVerify.h"
#include "tools/common/tests/TestPsIops.h"
#include "tools/common/tests/TestDF_bandwidth.h"
#include "tools/common/tests/TestTCTOneColumn.h"
#include "tools/common/tests/TestTCTAllColumn.h"
#include "tools/common/tests/TestGemm.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <algorithm>
#include <filesystem>
#ifdef __linux__
#include <sys/mman.h> //munmap
#endif

#ifdef _WIN32
#pragma warning ( disable : 4702 )
#endif

// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {

enum class test_status
{
  passed = 0,
  warning = 1,
  failed = 2
};

static const std::string test_token_skipped = "SKIPPED";
static const std::string test_token_failed = "FAILED";
static const std::string test_token_passed = "PASSED";

void
doesTestExist(const std::string& userTestName, const XBU::VectorPairStrings& testNames)
{
  const auto iter = std::find_if( testNames.begin(), testNames.end(),
    [&userTestName](const std::pair<std::string, std::string>& pair){ return pair.first == userTestName;} );

  if (iter == testNames.end())
    throw xrt_core::error((boost::format("Invalid test name: '%s'") % userTestName).str());
}

std::vector<std::shared_ptr<TestRunner>> testSuite = {
  std::make_shared<TestAuxConnection>(),
  std::make_shared<TestPcieLink>(),
  std::make_shared<TestSCVersion>(),
  std::make_shared<TestVerify>(),
  std::make_shared<TestDMA>(),
  std::make_shared<TestIOPS>(),
  std::make_shared<TestBandwidthKernel>(),
  std::make_shared<Testp2p>(),
  std::make_shared<Testm2m>(),
  std::make_shared<TestHostMemBandwidthKernel>(),
  std::make_shared<TestBist>(),
  std::make_shared<TestVcuKernel>(),
  std::make_shared<TestAiePl>(),
  std::make_shared<TestAiePs>(),
  std::make_shared<TestPsPlVerify>(),
  std::make_shared<TestPsVerify>(),
  std::make_shared<TestPsIops>(),
  std::make_shared<TestDF_bandwidth>(),
  std::make_shared<TestTCTOneColumn>(),
  std::make_shared<TestTCTAllColumn>(),
  std::make_shared<TestGemm>()
};

/*
 * print basic information about a test
 */
static void
pretty_print_test_desc(const boost::property_tree::ptree& test, int& test_idx,
                       std::ostream & _ostream, const std::string& bdf)
{
  // If the status is anything other than skipped print the test name
  auto _status = test.get<std::string>("status", "");
  if (!boost::equals(_status, test_token_skipped)) {
    std::string test_desc = boost::str(boost::format("Test %d [%s]") % ++test_idx % bdf);
    // Only use the long name option when displaying the test
    _ostream << boost::format("%-26s: %s \n") % test_desc % test.get<std::string>("name", "<unknown>");

    if (XBU::getVerbose())
      XBU::message(boost::str(boost::format("    %-22s: %s\n") % "Description" % test.get<std::string>("description")), false, _ostream);
  }
  else if (XBU::getVerbose()) {
    std::string test_desc = boost::str(boost::format("Test %d [%s]") % ++test_idx % bdf);
    XBU::message(boost::str(boost::format("%-26s: %s \n") % test_desc % test.get<std::string>("name")));
    XBU::message(boost::str(boost::format("    %-22s: %s\n") % "Description" % test.get<std::string>("description")), false, _ostream);
  }

}

/*
 * print test run
 */
static void
pretty_print_test_run(const boost::property_tree::ptree& test,
                      test_status& status, std::ostream & _ostream)
{
  auto _status = test.get<std::string>("status", "");
  std::string prev_tag = "";
  bool warn = false;
  bool error = false;

  // if supported and details/error/warning: ostr
  // if supported and xclbin/testcase: verbose
  // if not supported: verbose
  auto redirect_log = [&](const std::string& tag, const std::string& log_str) {
    std::vector<std::string> verbose_tags = {"Xclbin", "Testcase", "DPU-Sequence"};
    if (boost::equals(_status, test_token_skipped) || (std::find(verbose_tags.begin(), verbose_tags.end(), tag) != verbose_tags.end())) {
      if (XBU::getVerbose())
        XBU::message(log_str, false, _ostream);
      else
        return;
    }
    else
      _ostream << log_str;
  };

  try {
    for (const auto& dict : test.get_child("log")) {
      for (const auto& kv : dict.second) {
        std::string formattedString = XBU::wrap_paragraphs(kv.second.get_value<std::string>(), 28 /*Indention*/, 60 /*Max length*/, false /*Indent*/);
        std::string logType = kv.first;

        if (boost::iequals(logType, "warning")) {
          warn = true;
          logType = "Warning(s)";
        }

        if (boost::iequals(kv.first, "error")) {
          error = true;
          logType = "Error(s)";
        }

        if (kv.first.compare(prev_tag)) {
          prev_tag = kv.first;
          redirect_log(kv.first, boost::str(boost::format("    %-22s: %s\n") % logType % formattedString));
        }
        else
          redirect_log(kv.first, boost::str(boost::format("    %-22s  %s\n") % "" % formattedString));
      }
    }
  }
  catch (...) {}

  if (error) {
    status = test_status::failed;
  }
  else if (warn) {
    _status.append(" with warnings");
    status = test_status::warning;
  }

  boost::to_upper(_status);
  redirect_log("", boost::str(boost::format("    %-22s: [%s]\n") % "Test Status" % _status));
  redirect_log("", "-------------------------------------------------------------------------------\n");
}

/*
 * print final status of the card
 */
static void
print_status(test_status status, std::ostream & _ostream)
{
  if (status == test_status::failed)
    _ostream << "Validation failed";
  else
    _ostream << "Validation completed";
  if (status == test_status::warning)
    _ostream << ", but with warnings";
  if (!XBU::getVerbose())
    _ostream << ". Please run the command '--verbose' option for more details";
  _ostream << std::endl;
}

static void
get_alveo_platform_info(const std::shared_ptr<xrt_core::device>& device,
                        boost::property_tree::ptree& ptTree)
{
  boost::property_tree::ptree platform_report;
  const boost::property_tree::ptree empty_ptree;
  auto report = std::make_shared<ReportPlatforms>();
  report->getPropertyTreeInternal(device.get(), platform_report);

  const boost::property_tree::ptree& platforms = platform_report.get_child("platforms", empty_ptree);
  if (platforms.size() > 1)
    throw xrt_core::error(std::errc::operation_canceled);

  for (auto& kp : platforms) {
    const boost::property_tree::ptree& pt_platform = kp.second;
    const boost::property_tree::ptree& pt_static_region = pt_platform.get_child("static_region", empty_ptree);
    ptTree.put("platform", pt_static_region.get<std::string>("vbnv", "N/A"));
    ptTree.put("platform_id", pt_static_region.get<std::string>("logic_uuid", "N/A"));
    ptTree.put("sc_version", pt_platform.get<std::string>("controller.satellite_controller.version", "N/A"));
  }
}

static void
get_ryzen_platform_info(const std::shared_ptr<xrt_core::device>& device,
                        boost::property_tree::ptree& ptTree)
{
  ptTree.put("platform", xrt_core::device_query<xrt_core::query::rom_vbnv>(device));
  const auto mode = xrt_core::device_query_default<xrt_core::query::performance_mode>(device, 0);
  ptTree.put("performance_mode", xrt_core::query::performance_mode::parse_status(mode));
  ptTree.put("power", xrt_core::utils::format_base10_shiftdown6(xrt_core::device_query_default<xrt_core::query::power_microwatts>(device, 0)));
}

static void
get_platform_info(const std::shared_ptr<xrt_core::device>& device,
                  boost::property_tree::ptree& ptTree,
                  Report::SchemaVersion /*schemaVersion*/,
                  std::ostream& oStream)
{
  auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(device);
  ptTree.put("device_id", xrt_core::query::pcie_bdf::to_string(bdf));

  switch (xrt_core::device_query<xrt_core::query::device_class>(device)) {
  case xrt_core::query::device_class::type::alveo:
    get_alveo_platform_info(device, ptTree);
    break;
  case xrt_core::query::device_class::type::ryzen:
    get_ryzen_platform_info(device, ptTree);
    break;
  }

  // Text output
  oStream << boost::format("%-26s: [%s]\n") % "Validate Device" % ptTree.get<std::string>("device_id");
  oStream << boost::format("    %-22s: %s\n") % "Platform" % ptTree.get<std::string>("platform");

  const std::string& sc_ver = ptTree.get("sc_version", "");
  if (!sc_ver.empty())
    oStream << boost::format("    %-22s: %s\n") % "SC Version" % sc_ver;
  const std::string& plat_id = ptTree.get("platform_id", "");
  if (!plat_id.empty())
    oStream << boost::format("    %-22s: %s\n") % "Platform ID" % plat_id;
  const std::string& perf_mode = ptTree.get("performance_mode", "");
  if (!perf_mode.empty())
    oStream << boost::format("    %-22s: %s\n") % "Performance Mode" % perf_mode;
  const std::string& power = ptTree.get("power", "");
  if (!boost::starts_with(power, ""))
    oStream << boost::format("    %-22s: %s Watts\n") % "Power" % power;
}

static test_status
run_test_suite_device( const std::shared_ptr<xrt_core::device>& device,
                       Report::SchemaVersion schemaVersion,
                       std::vector<std::shared_ptr<TestRunner>>& testObjectsToRun,
                       boost::property_tree::ptree& ptDevCollectionTestSuite)
{
  boost::property_tree::ptree ptDeviceTestSuite;
  boost::property_tree::ptree ptDeviceInfo;
  test_status status = test_status::passed;

  if (testObjectsToRun.empty())
    throw std::runtime_error("No test given to validate against.");

  get_platform_info(device, ptDeviceInfo, schemaVersion, std::cout);
  std::cout << "-------------------------------------------------------------------------------" << std::endl;

  int test_idx = 0;

  if (testObjectsToRun.size() == 1)
    XBU::setVerbose(true);// setting verbose true for single_case.

  for (std::shared_ptr<TestRunner> testPtr : testObjectsToRun) {
    auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(device);

    boost::property_tree::ptree ptTest;
    try {
      ptTest = testPtr->startTest(device);
    } catch (const std::exception&) {
      ptTest = testPtr->get_test_header();
      ptTest.put("status", test_token_failed);
    }
    ptDeviceTestSuite.push_back( std::make_pair("", ptTest) );

    pretty_print_test_desc(ptTest, test_idx, std::cout, xrt_core::query::pcie_bdf::to_string(bdf));
    pretty_print_test_run(ptTest, status, std::cout);

    // If a test fails, don't test the remaining ones
    if (status == test_status::failed) {
      break;
    }
  }

  print_status(status, std::cout);

  ptDeviceInfo.put_child("tests", ptDeviceTestSuite);
  ptDevCollectionTestSuite.push_back( std::make_pair("", ptDeviceInfo) );

  return status;
}

static bool
run_tests_on_devices( std::shared_ptr<xrt_core::device> &device,
                      Report::SchemaVersion schemaVersion,
                      std::vector<std::shared_ptr<TestRunner>>& testObjectsToRun,
                      std::ostream & output)
{
  // -- Root property tree
  boost::property_tree::ptree ptDevCollectionTestSuite;

  // -- Run the various tests and collect the test data
  boost::property_tree::ptree ptDeviceTested;
  auto has_failures = (run_test_suite_device(device, schemaVersion, testObjectsToRun, ptDeviceTested) == test_status::failed);

  ptDevCollectionTestSuite.put_child("logical_devices", ptDeviceTested);

  // -- Write the formatted output
  switch (schemaVersion) {
    case Report::SchemaVersion::json_20202:
      boost::property_tree::json_parser::write_json(output, ptDevCollectionTestSuite, true /*Pretty Print*/);
      output << std::endl;
      break;
    default:
      // Do nothing
      break;
  }

  return has_failures;
}

}
//end anonymous namespace

// ----- C L A S S   M E T H O D S -------------------------------------------

XBU::VectorPairStrings
SubCmdValidate::getTestNameDescriptions(const bool addAdditionOptions) const
{
  XBU::VectorPairStrings reportDescriptionCollection;

  // 'verbose' option
  if (addAdditionOptions) {
    reportDescriptionCollection.emplace_back("all", "All applicable validate tests will be executed (default)");
    reportDescriptionCollection.emplace_back("quick", "Only the first 4 tests will be executed");
  }

  const auto& configs = JSONConfigurable::parse_configuration_tree(m_commandConfig);
  const auto& testOptionsMap = JSONConfigurable::extract_subcmd_config<TestRunner, TestRunner>(testSuite, configs, getConfigName(), std::string("test"));
  const std::string& deviceClass = XBU::get_device_class(m_device, true);
  const auto it = testOptionsMap.find(deviceClass);
  const std::vector<std::shared_ptr<TestRunner>>& testOptions = (it == testOptionsMap.end()) ? testSuite : it->second;

  // report names and description
  for (const auto& test : testOptions) {
    std::string testName = boost::algorithm::to_lower_copy(test.get()->getConfigName());
    reportDescriptionCollection.emplace_back(testName, test.get()->get_test_header().get("description", "<no description>"));
  }

  return reportDescriptionCollection;
}

  // -- Build up the format options
static boost::program_options::options_description common_options;
static std::map<std::string,std::vector<std::shared_ptr<JSONConfigurable>>> jsonOptions;
static const std::pair<std::string, std::string> all_test = {"all", "All applicable validate tests will be executed (default)"};
static const std::pair<std::string, std::string> quick_test = {"quick", "Only the first 4 tests will be executed"};

SubCmdValidate::SubCmdValidate(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree& configurations)
    : SubCmd("validate",
             "Validates the basic shell acceleration functionality")
    , m_device("")
    , m_tests_to_run({"all"})
    , m_format("JSON")
    , m_output("")
    , m_param("")
    , m_xclbin_location("")
    , m_help(false)
{
  const std::string longDescription = "Validates the given device by executing the platform's validate executable.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  m_commandConfig = configurations;

  const auto& configs = JSONConfigurable::parse_configuration_tree(configurations);
  jsonOptions = JSONConfigurable::extract_subcmd_config<JSONConfigurable, TestRunner>(testSuite, configs, getConfigName(), std::string("test"));

  // -- Build up the format options
  static const auto formatOptionValues = XBU::create_suboption_list_string(Report::getSchemaDescriptionVector());
  XBUtilities::VectorPairStrings common_tests;
  common_tests.emplace_back(all_test);
  common_tests.emplace_back(quick_test);
  static const auto formatRunValues = XBU::create_suboption_list_map("", jsonOptions, common_tests);

  common_options.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("format,f", boost::program_options::value<decltype(m_format)>(&m_format), (std::string("Report output format. Valid values are:\n") + formatOptionValues).c_str() )
    ("output,o", boost::program_options::value<decltype(m_output)>(&m_output), "Direct the output to the given file")
    ("param", boost::program_options::value<decltype(m_param)>(&m_param), "Extended parameter for a given test. Format: <test-name>:<key>:<value>")
    ("path,p", boost::program_options::value<decltype(m_xclbin_location)>(&m_xclbin_location), "Path to the directory containing validate xclbins")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_commonOptions.add(common_options);
  m_commonOptions.add_options()
    ("run,r", boost::program_options::value<decltype(m_tests_to_run)>(&m_tests_to_run)->multitoken(), (std::string("Run a subset of the test suite.  Valid options are:\n") + formatRunValues).c_str() )
  ;
}

/*
 * Extended keys helper struct
 */
struct ExtendedKeysStruct {
  std::string test_name;
  std::string param_name;
  std::string description;
};

static std::vector<ExtendedKeysStruct>  extendedKeysCollection = {
  {"dma", "block-size", "Memory transfer size (bytes)"}
};

std::string
extendedKeysOptions()
{
  static unsigned int m_maxColumnWidth = 100;
  std::stringstream fmt_output;
  // Formatting color parameters
  const std::string fgc_header     = XBU::is_escape_codes_disabled() ? "" : EscapeCodes::fgcolor(EscapeCodes::FGC_HEADER).string();
  const std::string fgc_optionName = XBU::is_escape_codes_disabled() ? "" : EscapeCodes::fgcolor(EscapeCodes::FGC_OPTION).string();
  const std::string fgc_optionBody = XBU::is_escape_codes_disabled() ? "" : EscapeCodes::fgcolor(EscapeCodes::FGC_OPTION_BODY).string();
  const std::string fgc_reset      = XBU::is_escape_codes_disabled() ? "" : EscapeCodes::fgcolor::reset();

  // Report option group name (if defined)
  boost::format fmtHeader(fgc_header + "\n%s:\n" + fgc_reset);
  fmt_output << fmtHeader % "EXTENDED KEYS";

  // Report the options
  boost::format fmtOption(fgc_optionName + "  %-18s " + fgc_optionBody + "- %s\n" + fgc_reset);
  unsigned int optionDescTab = 23;

  for (auto& param : extendedKeysCollection) {
    const auto key_desc = (boost::format("%s:<value> - %s") % param.param_name % param.description).str();
    const auto& formattedString = XBU::wrap_paragraphs(key_desc, optionDescTab, m_maxColumnWidth - optionDescTab, false);
    fmt_output << fmtOption % param.test_name % formattedString;
  }

  return fmt_output.str();
}

void
SubCmdValidate::print_help_internal() const
{
  if (m_device.empty()) {
    printHelp(false, extendedKeysOptions());
    return;
  }

  const std::string deviceClass = XBU::get_device_class(m_device, true);
  auto it = jsonOptions.find(deviceClass);

  XBUtilities::VectorPairStrings help_tests = { all_test };
  if (it != jsonOptions.end() && it->second.size() > 3)
    help_tests.emplace_back(quick_test);

  static const std::string testOptionValues = XBU::create_suboption_list_map(deviceClass, jsonOptions, help_tests);
  std::vector<std::string> tempVec;
  common_options.add_options()
    ("report,r", boost::program_options::value<decltype(tempVec)>(&tempVec)->multitoken(), (std::string("The type of report to be produced. Reports currently available are:\n") + testOptionValues).c_str() )
  ;
  printHelp(common_options, m_hiddenOptions, deviceClass, false, extendedKeysOptions());
}

void
SubCmdValidate::execute(const SubCmdOptions& _options) const
{
  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  // Check to see if help was requested or no command was found
  if (m_help) {
    print_help_internal();
    return;
  }

  // -- Process the options --------------------------------------------
  Report::SchemaVersion schemaVersion = Report::SchemaVersion::unknown;    // Output schema version
  std::vector<std::string> param;
  std::vector<std::string> validatedTests;
  std::string validateXclbinPath = m_xclbin_location;
  const auto testNameDescription = getTestNameDescriptions(true /* Add "all" and "quick" options*/);
  try {
    // Output Format
    schemaVersion = Report::getSchemaDescription(m_format).schemaVersion;
    if (schemaVersion == Report::SchemaVersion::unknown)
      throw xrt_core::error((boost::format("Unknown output format: '%s'") % m_format).str());

    // Output file
    if (!m_output.empty() && !XBU::getForce() && std::filesystem::exists(m_output))
        throw xrt_core::error((boost::format("Output file already exists: '%s'") % m_output).str());

    if (m_tests_to_run.empty())
      throw std::runtime_error("No test given to validate against.");

    // Validate the user test requests
    for (auto &userTestName : m_tests_to_run) {
      const auto validateTestName = boost::algorithm::to_lower_copy(userTestName);

      if ((validateTestName == "all") && (m_tests_to_run.size() > 1))
        throw xrt_core::error("The 'all' value for the tests to run cannot be used with any other named tests.");

      if ((validateTestName == "quick") && (m_tests_to_run.size() > 1))
        throw xrt_core::error("The 'quick' value for the tests to run cannot be used with any other name tests.");

      // Verify the current user test request exists in the test suite
      doesTestExist(validateTestName, testNameDescription);
      validatedTests.push_back(validateTestName);
    }

    // check if xclbin folder path is provided
    if (!validateXclbinPath.empty()) {
      XBU::verbose("Sub command: --path");
      if (!std::filesystem::exists(validateXclbinPath) || !std::filesystem::is_directory(validateXclbinPath))
        throw xrt_core::error((boost::format("Invalid directory path : '%s'") % validateXclbinPath).str());
      if (validateXclbinPath.compare(".") == 0 || validateXclbinPath.compare("./") == 0)
        validateXclbinPath = std::filesystem::current_path().string();
      if (validateXclbinPath.back() != '/')
        validateXclbinPath.append("/");
    }

    //check if param option is provided
    if (!m_param.empty()) {
      XBU::verbose("Sub command: --param");
      boost::split(param, m_param, boost::is_any_of(":")); // eg: dma:block-size:1024

      //check parameter format
      if (param.size() != 3)
        throw xrt_core::error((boost::format("Invalid parameter format (expected 3 positional arguments): '%s'") % m_param).str());

      //check test case name
      doesTestExist(param[0], testNameDescription);

      //check parameter name
      auto iter = std::find_if( extendedKeysCollection.begin(), extendedKeysCollection.end(),
          [&param](const ExtendedKeysStruct& collection){ return collection.param_name == param[1];} );
      if (iter == extendedKeysCollection.end())
        throw xrt_core::error((boost::format("Unsupported parameter name '%s' for validation test '%s'") % param[1] % param[2]).str());
    }

  } catch (const xrt_core::error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    print_help_internal();
    throw xrt_core::error(std::errc::operation_canceled);
  }


  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  const auto& configs = JSONConfigurable::parse_configuration_tree(m_commandConfig);
  auto testOptionsMap = JSONConfigurable::extract_subcmd_config<TestRunner, TestRunner>(testSuite, configs, getConfigName(), std::string("test"));
  const std::string& deviceClass = XBU::get_device_class(m_device, true);
  auto it = testOptionsMap.find(deviceClass);
  if (it == testOptionsMap.end())
    XBU::throw_cancel(boost::format("Invalid device class %s. Device: %s") % deviceClass % m_device);
  std::vector<std::shared_ptr<TestRunner>>& testOptions = it->second;

  // Collect all of the tests of interests
  std::vector<std::shared_ptr<TestRunner>> testObjectsToRun;
  for (size_t index = 0; index < testOptions.size(); ++index) {
    std::string testSuiteName = testOptions[index]->get_name();
    // The all option enqueues all test suites not marked explicit
    if (validatedTests[0] == "all") {
      // Do not queue test suites that must be explicitly passed in
      if (testOptions[index]->is_explicit())
        continue;
      testObjectsToRun.push_back(testOptions[index]);
      // add custom param to the ptree if available
      if (!param.empty() && boost::equals(param[0], testSuiteName)) {
        testOptions[index]->set_param(param[1], param[2]);
      }
      if (!validateXclbinPath.empty())
        testOptions[index]->set_xclbin_path(validateXclbinPath);
      continue;
    }

    // The quick test option enqueues only the first three test suites
    if (validatedTests[0] == "quick") {
      testObjectsToRun.push_back(testOptions[index]);
      if (!validateXclbinPath.empty())
        testOptions[index]->set_xclbin_path(validateXclbinPath);
      if (index == 3)
        break;
    }

    // Logic for individually defined tests
    // Enqueue the matching test suites to be executed
    for (const auto & testName : validatedTests) {
      if (boost::equals(testName, testSuiteName)) {
        testObjectsToRun.push_back(testOptions[index]);
        // add custom param to the ptree if available
        if (!param.empty() && boost::equals(param[0], testSuiteName)) {
          testOptions[index]->set_param(param[1], param[2]);
        }
        if (!validateXclbinPath.empty())
          testOptions[index]->set_xclbin_path(validateXclbinPath);
        break;
      }
    }
  }

  // -- Run the tests --------------------------------------------------
  std::ostringstream oSchemaOutput;
  bool has_failures = run_tests_on_devices(device, schemaVersion, testObjectsToRun, oSchemaOutput);

  // -- Write output file ----------------------------------------------
  if (!m_output.empty()) {
    std::ofstream fOutput;
    fOutput.open(m_output, std::ios::out | std::ios::binary);
    if (!fOutput.is_open())
      throw xrt_core::error((boost::format("Unable to open the file '%s' for writing.") % m_output).str());

    fOutput << oSchemaOutput.str();

    std::cout << boost::format("Successfully wrote the %s file: %s") % m_format % m_output << std::endl;
  }

  if (has_failures == true)
    throw xrt_core::error(std::errc::operation_canceled);
}
