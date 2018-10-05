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
#include "ParameterSectionData.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <stdexcept>

// System - Include Files
#include <iostream>
#include <string>
#include <set>

// Generated include files
#include <version.h>
 
namespace XUtil = XclBinUtilities;

namespace {
enum ReturnCodes {
  RC_SUCCESS = 0,
  RC_ERROR_IN_COMMAND_LINE = 1,
  RC_ERROR_UNHANDLED_EXCEPTION = 2,
};
}


void drcCheckFiles(const std::vector<std::string> & _inputFiles, 
                   const std::vector<std::string> & _outputFiles,
                   bool _bForce)
{
   std::set<std::string> normalizedInputFiles;

   for( auto file : _inputFiles) {
     if ( !boost::filesystem::exists(file)) {
       std::string errMsg = "ERROR: The following input file does not exist: " + file;
       throw std::runtime_error(errMsg);
     }
     boost::filesystem::path filePath(file);
     normalizedInputFiles.insert(canonical(filePath).string());
   }

   std::vector<std::string> normalizedOutputFiles;
   for ( auto file : _outputFiles) {
     if ( boost::filesystem::exists(file)) {
       if (_bForce == false) {
         std::string errMsg = "ERROR: The following output file already exists on disk (use the force option to overwrite): " + file;
         throw std::runtime_error(errMsg);
       } else {
         boost::filesystem::path filePath(file);
         normalizedOutputFiles.push_back(canonical(filePath).string());
       }
     }
   }

   // See if the output file will stomp on an input file
   for (auto file : normalizedOutputFiles) {
     if (normalizedInputFiles.find(file) != normalizedInputFiles.end()) {
       std::string errMsg = "ERROR: The following output file is also used for input : " + file;
       throw std::runtime_error(errMsg);
     }
   }
}


// Program entry point
int main_(int argc, char** argv) {

  bool bVerbose = false;
  bool bTrace = false;
  bool bValidateImage = false;
  bool bMigrateForward = false;
  bool bListNames = false;
  bool bListSections = false;
  bool bInfo = false;
  bool bSkipUUIDInsertion = false;
  bool bValidateInsertion = true;
  bool bSkipValidateInsertion = false;
  bool bVersion = false;
  bool bForce = true;   // Assume true until xocc is updated to use the --force option

  std::string sInputFile;
  std::string sOutputFile;

  std::vector<std::string> sectionToReplace;
  std::vector<std::string> sectionsToAdd;
  std::vector<std::string> sectionsToRemove;
  std::vector<std::string> sectionsToDump;

  std::vector<std::string> keyValuePairs;


  namespace po = boost::program_options;

  po::options_description desc("Options");
  desc.add_options()
      ("help,h", "Print help messages")
      ("input,i", boost::program_options::value<std::string>(&sInputFile), "Input file name")
      ("output,o", boost::program_options::value<std::string>(&sOutputFile), "Output file name")
      ("verbose,v", boost::program_options::bool_switch(&bVerbose), "Display verbose/debug information")
      ("validate", boost::program_options::bool_switch(&bValidateImage), "Validate xclbin image")
      ("migrate-forward", boost::program_options::bool_switch(&bMigrateForward), "Migrate the xclbin archive forward to the new binary format.")
      ("remove-section", boost::program_options::value<std::vector<std::string> >(&sectionsToRemove)->multitoken(), "Section name to remove")
      ("add-section", boost::program_options::value<std::vector<std::string> >(&sectionsToAdd)->multitoken(), "Section name to add.  Format: <section>:<format>:<file>")
      ("dump-section", boost::program_options::value<std::vector<std::string> >(&sectionsToDump)->multitoken(), "Section to dump")
      ("replace-section", boost::program_options::value<std::vector<std::string> >(&sectionToReplace)->multitoken(), "Section to replace")
      ("key-value", boost::program_options::value<std::vector<std::string> >(&keyValuePairs)->multitoken(), "Key value pairs.  Format: [USER | SYS}:<key>:<value>")

      ("info", boost::program_options::bool_switch(&bInfo), "Print Section Info")
      ("list-names,n", boost::program_options::bool_switch(&bListNames), "List the available names")
      ("list-sections,l", boost::program_options::bool_switch(&bListSections), "List the sections")
      ("version", boost::program_options::bool_switch(&bVersion), "Version information regarding this executable")
      ("force", boost::program_options::bool_switch(&bForce), "Forces an file overwrite")
 ;

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

  // hidden options
  boost::program_options::options_description hidden("Hidden options");
  hidden.add_options()
    ("trace,t", boost::program_options::bool_switch(&bTrace), "Trace")
    ("skip-uuid-insertion", boost::program_options::bool_switch(&bSkipUUIDInsertion), "Do not update the xclbin's UUID")
    ("skip-validate-insertion", boost::program_options::bool_switch(&bSkipValidateInsertion), "Do not insert the checksum validation block.")
  ;

  boost::program_options::options_description all("Allowed options");
  all.add(desc).add(hidden);


  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, all), vm); // Can throw

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
  XUtil::setVerbose(bTrace);

  if (bVersion) {
    xrt::version::print(std::cout);
    return RC_SUCCESS;
  }

  if (argc == 1) {
    std::cout << "This utility operations on a xclbin produced by xocc." << std::endl << std::endl;
    std::cout << "For example:" << std::endl;
    std::cout << "  1) Reporting xclbin information  : xclbinutil --info --input binary_container_1.xclbin" << std::endl;
    std::cout << "  2) Extracting the bitstream image: xclbinutil --dump-section BITSTREAM:RAW:bitstream.bit --input binary_container_1.xclbin" << std::endl;
    std::cout << "  3) Extracting the build metadata : xclbinutil --dump-section BUILD_METADATA:HTML:buildMetadata.json --input binary_container_1.xclbin" << std::endl;
    std::cout << "  4) Removing a section            : xclbinutil --remove-section BITSTREAM --input binary_container_1.xclbin --output binary_container_modified.xclbin" << std::endl;
    std::cout << "  5) Checking xclbin integrity     : xclbinutil --validate --input binary_containter_1.xclbin" <<std::endl;

    std::cout << std::endl 
              << "Command Line Options" << std::endl
              << desc
              << std::endl;

    return RC_SUCCESS;
  }

  // Actions not requiring --input

  if (bListNames) {
    XUtil::printKinds();
    return RC_SUCCESS;
  }

  // Actions requiring --input
  
  // Check to see if there any file conflicts
  std::vector< std::string> inputFiles;
  {
    if (!sInputFile.empty()) {
       inputFiles.push_back(sInputFile);
    }

    for (auto section : sectionsToAdd) {
      ParameterSectionData psd(section);
      inputFiles.push_back(psd.getFile());
    }

    for (auto section : sectionToReplace ) {
      ParameterSectionData psd(section);
      inputFiles.push_back(psd.getFile());
    }
  }

  std::vector< std::string> outputFiles;
  {
    if (!sOutputFile.empty()) {
      outputFiles.push_back(sOutputFile);
    }

    for (auto section : sectionsToDump ) {
      ParameterSectionData psd(section);
      outputFiles.push_back(psd.getFile());
    }
  }

  drcCheckFiles(inputFiles, outputFiles, bForce);


  if (bValidateImage && !sInputFile.empty()) {
    XUtil::validateImage(sInputFile);
    return RC_SUCCESS;
  }

  if (bSkipValidateInsertion == true) {
      bValidateInsertion = false;
  }

  XclBin xclBin;
  if (!sInputFile.empty()) {
    xclBin.readXclBinBinary(sInputFile, bMigrateForward);
  }

  for (auto keyValue : keyValuePairs) {
    xclBin.setKeyValue(keyValue);
  }

  for (auto section : sectionsToRemove) {
    xclBin.removeSection(section);
  }

  for (auto section : sectionToReplace) {
    ParameterSectionData psd(section);
    xclBin.replaceSection( psd );
  }

  for (auto section : sectionsToAdd) {
    ParameterSectionData psd(section);
    if (psd.getSectionName().empty() &&
        psd.getFormatType() == Section::FT_JSON) {
      xclBin.addSections(psd);
    } else {
      xclBin.addSection(psd);
    }
  }

  for (auto section : sectionsToDump) {
    ParameterSectionData psd(section);
    xclBin.dumpSection(psd);
  }

  if (!sOutputFile.empty()) {
    xclBin.writeXclBinBinary(sOutputFile, bSkipUUIDInsertion, bValidateInsertion);
  }

  if (bListSections) {
    xclBin.printSections(std::cout);
  }

  if (bInfo) {
    xclBin.printHeader(std::cout);
  }

  return RC_SUCCESS;
}

