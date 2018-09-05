/**
 * Copyright (C) 2018 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "XclBinUtilMain.h"
#include "XclBinUtilities.h"
#include "XclBin.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

// System - Include Files
#include <iostream>
#include <string>

namespace XUtil = XclBinUtilities;

namespace {
enum ReturnCodes {
  RC_SUCCESS = 0,
  RC_ERROR_IN_COMMAND_LINE = 1,
  RC_ERROR_UNHANDLED_EXCEPTION = 2,
};
}


// Program entry point
int main(int argc, char** argv) {

  bool bVerbose = false;
  bool bValidateImage = false;
  bool bAddValidateImage = false;
  bool bMigrateForward = false;

  std::string sInputFile;
  std::string sOutputFile;

  std::string sSectionToRemove;
  std::string sSectionToAdd;
  std::string sSectionToDump;


  try {
    namespace po = boost::program_options;

    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Print help messages")
        ("input,i", boost::program_options::value<std::string>(&sInputFile), "Input file name")
        ("output,o", boost::program_options::value<std::string>(&sOutputFile), "Output file name")
        ("trace,t", boost::program_options::bool_switch(&bVerbose), "Trace")
        ("validate,v", boost::program_options::bool_switch(&bValidateImage), "Validate xclbin image")
        ("add-validation,c", boost::program_options::bool_switch(&bAddValidateImage), "Add image validation")
        ("migrate-forward", boost::program_options::bool_switch(&bMigrateForward), "Migrate the xclbin archive forward to the new binary format.")
        ("remove-section", boost::program_options::value<std::string>(&sSectionToRemove), "Section name to remove")
        ("add-section", boost::program_options::value<std::string>(&sSectionToAdd), "Section name to add")
        ("dump-section", boost::program_options::value<std::string>(&sSectionToDump), "Section to dump")


// --remove-section=section
//    Remove the section matching the section name. Note that using this option inappropriately may make the output file unusable.
//
// --dump-section sectionname=filename
//    Place the contents of section named sectionname into the file filename, overwriting any contents that may have been there previously.
//    This option is the inverse of --add-section. This option is similar to the --only-section option except that it does not create a formatted file,
//    it just dumps the contents as raw binary data, without applying any relocations. The option can be specified more than once.
//
// --add-section sectionname=filename
//    Add a new section named sectionname while copying the file. The contents of the new section are taken from the file filename.
//    The size of the section will be the size of the file. This option only works on file formats which can support sections with arbitrary names.
//
// --migrate-forward
//    Migrates the xclbin forward to the new binary structure using the backup mirror metadata.


    ;

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm); // Can throw

      if (vm.count("help")) {
        std::cout << "Command Line Options" << std::endl
            << desc
            << std::endl;
        return RC_SUCCESS;
      }

      po::notify(vm); // Can throw
    } catch (po::error& e) {
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      std::cerr << desc << std::endl;
      return RC_ERROR_IN_COMMAND_LINE;
    }

    // Examine the options
    // TODO: Clean up this flow.  Currently, its flow is that of testing features
    //       and not how the customer would use it.
    XclBinUtilities::setVerbose(bVerbose);

    if (bValidateImage && !sInputFile.empty()) {
      XUtil::validateImage(sInputFile);
      return RC_SUCCESS;
    }

    XclBin xclBin;
    if (!sInputFile.empty()) {
      xclBin.readXclBinBinary(sInputFile, bMigrateForward);
    }

    if (!sSectionToRemove.empty()) {
      xclBin.removeSection(sSectionToRemove);
    }

    if (!sSectionToAdd.empty()) {
      xclBin.addSection(sSectionToAdd);
    }

    if (!sSectionToDump.empty()) {
      xclBin.dumpSection(sSectionToDump);
    }

    if (!sOutputFile.empty()) {
      xclBin.writeXclBinBinary(sOutputFile);
    }

    if (bAddValidateImage && !sOutputFile.empty()) {
      XUtil::addCheckSumImage(sOutputFile, CST_SDBM);
    }
  } catch(std::exception& e){
    std::cerr << "Unhandled Exception caught in main(): " << std::endl
        << e.what() << std::endl
        << "exiting" << std::endl;
    return RC_ERROR_UNHANDLED_EXCEPTION;
  }

  return RC_SUCCESS;
}

