#include <gtest/gtest.h>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

namespace po = boost::program_options;

#include <iostream>
#include "globals.h"

int main(int argc, char **argv) {
  // -- Initialize the google test environment
  // Note: The InitGoogleTest() method function will remove arguments
  //       and updated argc accordingly.
  ::testing::InitGoogleTest(&argc, argv); 

  // -- Process the remaining arguments
  std::string resourceDirectory = boost::filesystem::path(boost::filesystem::current_path()).string();
  bool bQuiet = false;

  po::options_description options("Common Options");
  options.add_options()
    ("resource-dir", boost::program_options::value<decltype(resourceDirectory)>(&resourceDirectory), "The path to the unit test's resource directory")
    ("quiet", boost::program_options::bool_switch(&bQuiet), "All helping flow messages are suppressed")
    ;

  // -- Parse the command line
  po::parsed_options parsed = po::command_line_parser(argc, argv).
    options(options).            // Global options
    run();                          // Parse the options
  
  po::variables_map vm;
  
  try {
    po::store(parsed, vm);          // Can throw
    po::notify(vm);                 // Can throw
  } catch (po::error& e) {
    // Something bad happen with parsing our options
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    return 1;
  }
  
  // -- Set our global values
  TestUtilities::setResourceDir(resourceDirectory);
  TestUtilities::setIsQuiet(bQuiet);

  return RUN_ALL_TESTS();
}
