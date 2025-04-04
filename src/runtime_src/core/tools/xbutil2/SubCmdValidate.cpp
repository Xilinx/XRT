// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.

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
#include "tools/common/tests/TestAuxConnection.h"
#include "tools/common/tests/TestPcieLink.h"
#include "tools/common/tests/TestSCVersion.h"
#include "tools/common/tests/TestVerify.h"
#include "tools/common/tests/TestDMA.h"
#include "tools/common/tests/TestBandwidthKernel.h"
#include "tools/common/tests/Testp2p.h"
#include "tools/common/tests/Testm2m.h"
#include "tools/common/tests/TestHostMemBandwidthKernel.h"
#include "tools/common/tests/TestAiePl.h"
#include "tools/common/tests/TestAiePs.h"
#include "tools/common/tests/TestPsPlVerify.h"
#include "tools/common/tests/TestPsVerify.h"
#include "tools/common/tests/TestPsIops.h"
#include "tools/common/tests/TestDF_bandwidth.h"
#include "tools/common/tests/TestTCTOneColumn.h"
#include "tools/common/tests/TestTCTAllColumn.h"
#include "tools/common/tests/TestGemm.h"
#include "tools/common/tests/TestNPUThroughput.h"
#include "tools/common/tests/TestNPULatency.h"
#include "tools/common/tests/TestCmdChainLatency.h"
#include "tools/common/tests/TestCmdChainThroughput.h"
#include "tools/common/tests/TestAIEReconfigOverhead.h"
#include "tools/common/tests/TestTemporalSharingOvd.h"
#include "tools/common/tests/TestPreemptionOverhead.h"
#include "tools/common/tests/TestValidateUtilities.h"

namespace XBU = XBUtilities;
namespace xq = xrt_core::query;

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
doesTestExist(const std::string& userTestName, std::vector<std::shared_ptr<TestRunner>>& testNames)
{
  const auto iter = std::find_if( testNames.begin(), testNames.end(),
    [&userTestName](const std::shared_ptr<TestRunner>& testRunner){ 
      return userTestName == "all" || userTestName == "quick" || userTestName == testRunner->get_name();
    });

  if (iter == testNames.end())
    throw xrt_core::error((boost::format("Invalid test name: '%s'") % userTestName).str());
}

std::vector<std::shared_ptr<TestRunner>> testSuite = {
  std::make_shared<TestAuxConnection>(),
  std::make_shared<TestPcieLink>(),
  std::make_shared<TestSCVersion>(),
  std::make_shared<TestVerify>(),
  std::make_shared<TestDMA>(),
  std::make_shared<TestBandwidthKernel>(),
  std::make_shared<Testp2p>(),
  std::make_shared<Testm2m>(),
  std::make_shared<TestHostMemBandwidthKernel>(),
  std::make_shared<TestAiePl>(),
  std::make_shared<TestAiePs>(),
  std::make_shared<TestPsPlVerify>(),
  std::make_shared<TestPsVerify>(),
  std::make_shared<TestPsIops>(),
  std::make_shared<TestDF_bandwidth>(),
  std::make_shared<TestTCTOneColumn>(),
  std::make_shared<TestTCTAllColumn>(),
  std::make_shared<TestGemm>(),
  std::make_shared<TestNPUThroughput>(),
  std::make_shared<TestNPULatency>(),
  std::make_shared<TestCmdChainLatency>(),
  std::make_shared<TestCmdChainThroughput>(),
  std::make_shared<TestAIEReconfigOverhead>(),
  std::make_shared<TestTemporalSharingOvd>(),
  std::make_shared<TestPreemptionOverhead>(),
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
    std::vector<std::string> verbose_tags = {"Xclbin", "Testcase", "DPU-Sequence", "Benchmarks"};
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
  ptTree.put("platform", xrt_core::device_query<xq::rom_vbnv>(device));
  const auto mode = xrt_core::device_query_default<xq::performance_mode>(device, 0);
  ptTree.put("power_mode", xq::performance_mode::parse_status(mode));
}

static void
get_platform_info(const std::shared_ptr<xrt_core::device>& device,
                  boost::property_tree::ptree& ptTree,
                  Report::SchemaVersion /*schemaVersion*/,
                  std::ostream& oStream)
{
  auto bdf = xrt_core::device_query<xq::pcie_bdf>(device);
  ptTree.put("device_id", xq::pcie_bdf::to_string(bdf));

  switch (xrt_core::device_query<xq::device_class>(device)) {
  case xq::device_class::type::alveo:
    get_alveo_platform_info(device, ptTree);
    break;
  case xq::device_class::type::ryzen:
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
  const std::string& power_mode = ptTree.get("power_mode", "");
  if (!power_mode.empty())
    oStream << boost::format("    %-22s: %s\n") % "Power Mode" % power_mode;
  const std::string& power = ptTree.get("power", "");
  if (!boost::starts_with(power, ""))
    oStream << boost::format("    %-22s: %s Watts\n") % "Estimated Power" % power;
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

  for (std::shared_ptr<TestRunner> testPtr : testObjectsToRun) {
    auto bdf = xrt_core::device_query<xq::pcie_bdf>(device);

    boost::property_tree::ptree ptTest;
    try {
      ptTest = testPtr->startTest(device);
    } catch (const std::runtime_error& e) {
      std::cout << e.what() << std::endl;
      return test_status::failed;
    } catch (const std::exception&) {
      ptTest = testPtr->get_test_header();
      XBValidateUtils::logger(ptTest, "Error", "The test timed out");
      ptTest.put("status", test_token_failed);
      status = test_status::failed;
    }
    ptDeviceTestSuite.push_back( std::make_pair("", ptTest) );

    pretty_print_test_desc(ptTest, test_idx, std::cout, xq::pcie_bdf::to_string(bdf));
    pretty_print_test_run(ptTest, status, std::cout);
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

}
//end anonymous namespace

// ----- C L A S S   M E T H O D S -------------------------------------------
SubCmdValidate::SubCmdValidate(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("validate",
             "Validates the basic device acceleration functionality")
{
  const std::string longDescription = "Validates the given device by executing the platform's validate executable.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void 
                                                 
SubCmdValidate::handle_errors_and_validate_tests(const boost::program_options::variables_map& vm,
                                                 const SubCmdValidateOptions& options,
                                                 std::vector<std::shared_ptr<TestRunner>>& testOptions,
                                                 std::vector<std::string>& validatedTests,
                                                 std::vector<std::string>& param) const
{
  if (vm.count("output") && options.m_output.empty())
    throw xrt_core::error("Output file not specified ");

  if (vm.count("path") && options.m_xclbin_path.empty())
    throw xrt_core::error("xclbin path not specified");

  if (vm.count("param") && options.m_param.empty())
    throw xrt_core::error("Parameter not specified");

  if (vm.count("pmode") && options.m_pmode.empty())
    throw xrt_core::error("Power mode not specified");

  // When json is specified, make sure an accompanying output file is also specified
   if (vm.count("format") && options.m_output.empty())
    throw xrt_core::error("Please specify an output file to redirect the json to");

  if (!options.m_output.empty() && !XBU::getForce() && std::filesystem::exists(options.m_output))
    throw xrt_core::error((boost::format("The output file '%s' already exists. Please either remove it or execute this command again with the '--force' option to overwrite it") % options.m_output).str());


  auto testsToRun = options.m_tests_to_run;
  if (testsToRun.empty()) {
    if (!XBU::getAdvance()) {
      testsToRun = std::vector<std::string>({"all"});
    }
    else {
      throw xrt_core::error("No test given to validate against.");
    }
  }
  // Validate the user test requests
  for (auto &userTestName : testsToRun) {
    const auto validateTestName = boost::algorithm::to_lower_copy(userTestName);

    if ((validateTestName == "all") && (testsToRun.size() > 1))
      throw xrt_core::error("The 'all' value for the tests to run cannot be used with any other named tests.");

    if ((validateTestName == "quick") && (testsToRun.size() > 1))
      throw xrt_core::error("The 'quick' value for the tests to run cannot be used with any other name tests.");

    // Verify the current user test request exists in the test suite
    doesTestExist(validateTestName, testOptions);
    validatedTests.push_back(validateTestName);
  }
  //check if param option is provided
  if (!options.m_param.empty()) {
    XBU::verbose("Sub command: --param");
    boost::split(param, options.m_param, boost::is_any_of(":")); // eg: dma:block-size:1024

    //check parameter format
    if (param.size() != 3)
      throw xrt_core::error((boost::format("Invalid parameter format (expected 3 positional arguments): '%s'") % options.m_param).str());

    //check test case name
    doesTestExist(param[0], testOptions);

    //check parameter name
    auto iter = std::find_if( extendedKeysCollection.begin(), extendedKeysCollection.end(),
        [&param](const ExtendedKeysStruct& collection){ return collection.param_name == param[1];} );
    if (iter == extendedKeysCollection.end())
      throw xrt_core::error((boost::format("Unsupported parameter name '%s' for validation test '%s'") % param[1] % param[2]).str());
  }
}

void
SubCmdValidate::execute(const SubCmdOptions& _options) const
{
  // Parse sub-command ...
  po::variables_map vm;
  SubCmdValidateOptions options;
  try{
    const auto unrecognized_options = process_arguments(vm, _options, false);
    fill_option_values(vm, options);

    if (!unrecognized_options.empty())
    {
      std::string error_str;
      error_str.append("Unrecognized arguments:\n");
      for (const auto& option : unrecognized_options)
        error_str.append(boost::str(boost::format("  %s\n") % option));
      throw boost::program_options::error(error_str);
    }
  }
  catch (const boost::program_options::error& e)
  {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }


  // Check to see if help was requested or no command was found
  if (options.m_help) {
    printHelp();
    return;
  }

  XBU::setElf(options.m_elf);

  // -- Process the options --------------------------------------------
  Report::SchemaVersion schemaVersion = Report::SchemaVersion::unknown;    // Output schema version
  std::vector<std::string> param;
  std::vector<std::string> validatedTests;
  std::string validateXclbinPath = options.m_xclbin_path;

  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(options.m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  //get list of tests supported by a device
  auto tests = xrt_core::device_query<xq::xrt_smi_lists>(device, xq::xrt_smi_lists::type::validate_tests);
  auto testOptions = getTestList(tests);

  try {
    // Output Format
    schemaVersion = Report::getSchemaDescription(options.m_format).schemaVersion;
    if (schemaVersion == Report::SchemaVersion::unknown)
      throw xrt_core::error((boost::format("Unknown output format: '%s'") % options.m_format).str());
    // All Error Handling for xrt-smi validate should go here
    handle_errors_and_validate_tests(vm, options, testOptions, validatedTests, param); 

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
  } catch (const xrt_core::error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

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

  //get current performance mode
  const auto og_pmode = xrt_core::device_query_default<xq::performance_mode>(device, 0);
  const auto parsed_og_pmode = xq::performance_mode::parse_status(og_pmode);
  //--pmode
  try {
    if (!options.m_pmode.empty()) {
      XBU::verbose("Sub command: --param");

      if (boost::iequals(options.m_pmode, "DEFAULT")) {
        xrt_core::device_update<xq::performance_mode>(device.get(), xq::performance_mode::power_type::basic); // default
      }
      else if (boost::iequals(options.m_pmode, "PERFORMANCE")) {
        xrt_core::device_update<xq::performance_mode>(device.get(), xq::performance_mode::power_type::performance);
      }
      else if (boost::iequals(options.m_pmode, "TURBO")) {
        xrt_core::device_update<xq::performance_mode>(device.get(), xq::performance_mode::power_type::turbo);
      }
      else if (boost::iequals(options.m_pmode, "POWERSAVER") || boost::iequals(options.m_pmode, "BALANCED")) {
        throw xrt_core::error(boost::str(boost::format("No tests are supported in %s mode\n") % options.m_pmode));
      }
      else {
        throw xrt_core::error(boost::str(boost::format("Invalid pmode value: '%s'\n") % options.m_pmode));
      }
      XBU::verbose(boost::str(boost::format("Setting power mode to `%s` \n") % options.m_pmode));
    }
    else if(!boost::iequals(parsed_og_pmode, "PERFORMANCE")) {
      xrt_core::device_update<xq::performance_mode>(device.get(), xq::performance_mode::power_type::performance);
      XBU::verbose("Setting power mode to `performance`\n");
    } 
  } catch (const xq::no_such_key&) {
    // Do nothing, as performance mode setting is not supported
  } catch(const xrt_core::error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  } catch (const std::exception & ex) { //check if permission was denied, i.e., no sudo access
    if(boost::icontains(ex.what(), "Permission denied"))
      std::cout << boost::format("WARNING: User doesn't have admin permissions to set performance mode. Running validate in %s mode") 
                                    % parsed_og_pmode << std::endl;
  }
  // -- Run the tests --------------------------------------------------
  std::ostringstream oSchemaOutput;
  bool has_failures = run_tests_on_devices(device, schemaVersion, testObjectsToRun, oSchemaOutput);

  try {
    //reset pmode
    xrt_core::device_update<xq::performance_mode>(device.get(), static_cast<xq::performance_mode::power_type>(og_pmode));
  } catch (const xq::no_such_key&) {
    // Do nothing, as performance mode setting is not supported
  } catch (const std::exception & ex) { //check if permission was denied, i.e., no sudo access
    if(!boost::icontains(ex.what(), "Permission denied")) {
      std::cerr << boost::format("\nERROR: %s\n") % ex.what();
      throw xrt_core::error(std::errc::operation_canceled);
    }
  }

  // -- Write output file ----------------------------------------------
  if (!options.m_output.empty()) {
    std::ofstream fOutput;
    fOutput.open(options.m_output, std::ios::out | std::ios::binary);
    if (!fOutput.is_open())
      throw xrt_core::error((boost::format("Unable to open the file '%s' for writing.") % options.m_output).str());

    fOutput << oSchemaOutput.str();

    std::cout << boost::format("Successfully wrote the %s file: %s") % options.m_format % options.m_output << std::endl;
  }

  if (has_failures == true)
    throw xrt_core::error(std::errc::operation_canceled);
}

void SubCmdValidate::fill_option_values(const po::variables_map& vm, SubCmdValidateOptions& options) const
{
  options.m_device = vm.count("device") ? vm["device"].as<std::string>() : "";
  options.m_format = vm.count("format") ? vm["format"].as<std::string>() : "JSON";
  options.m_output = vm.count("output") ? vm["output"].as<std::string>() : "";
  options.m_param = vm.count("param") ? vm["param"].as<std::string>() : "";
  options.m_xclbin_path = vm.count("path") ? vm["path"].as<std::string>() : "";
  options.m_pmode = vm.count("pmode") ? vm["pmode"].as<std::string>() : "";
  options.m_tests_to_run = vm.count("run") ? vm["run"].as<std::vector<std::string>>() : std::vector<std::string>();
  options.m_help = vm.count("help") ? vm["help"].as<bool>() : false;
  options.m_elf = vm.count("elf") ? vm["elf"].as<bool>() : false;
}

void
SubCmdValidate::setOptionConfig(const boost::property_tree::ptree &config)
{
  m_jsonConfig = SubCmdJsonObjects::JsonConfig(config.get_child("subcommands"), getName());
  try{
    m_jsonConfig.addProgramOptions(m_commonOptions, "common", getName());
    m_jsonConfig.addProgramOptions(m_hiddenOptions, "hidden", getName());
  } 
  catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}

std::vector<std::shared_ptr<TestRunner>>
SubCmdValidate::getTestList(const xrt_core::smi::tuple_vector& tests) const
{
  // Vector to store the matched tests
  std::vector<std::shared_ptr<TestRunner>> matchedTests;

  for (const auto& test : tests) {
    auto it = std::find_if(testSuite.begin(), testSuite.end(),
              [&test](const std::shared_ptr<TestRunner>& runner) {
                return std::get<0>(test) == runner->getConfigName() &&
                       (std::get<2>(test) != "hidden" || XBU::getAdvance());
              });

    if (it != testSuite.end()) {
      matchedTests.push_back(*it);
    }
  }
  return matchedTests;
}
