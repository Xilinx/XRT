/**
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
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

#include "FormattedOutput.h"
#include "ParameterSectionData.h"
#include "xrt/detail/xclbin.h"
#include "XclBinClass.h"
#include "XclBinSignature.h"
#include "XclBinUtilities.h"

// 3rd Party Library - Include Files
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <stdexcept>

// System - Include Files
#include <filesystem>
#include <iostream>
#include <set>
#include <string>

namespace XUtil = XclBinUtilities;
namespace fs = std::filesystem;

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

   for (const auto& file : _inputFiles) {
     if (!fs::exists(file)) {
       std::string errMsg = "ERROR: The following input file does not exist: " + file;
       throw std::runtime_error(errMsg);
     }
     fs::path filePath(file);
     normalizedInputFiles.insert(canonical(filePath).string());
   }

   std::vector<std::string> normalizedOutputFiles;
   for ( const auto& file : _outputFiles) {
     if ( !fs::exists(file))
       continue;

     if (_bForce == false) {
       std::string errMsg = "ERROR: The following output file already exists on disk (use the force option to overwrite): " + file;
       throw std::runtime_error(errMsg);
     } else {
       normalizedOutputFiles.push_back(fs::canonical(file).string());
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

void insertTargetMode(const std::string & _sTarget, std::vector<std::string> & _keyValuePairs)
{
  bool bDfxEnable = false;
  std::string sDomain, sKey, sValue;

  // Extract the DFX_ENABLE key (if present)
  for (unsigned int index = 0; index < _keyValuePairs.size(); ++index) {
    XclBin::getKeyValueComponents(_keyValuePairs[index], sDomain, sKey, sValue);
    if ((sDomain == "SYS") && (sKey == "dfx_enable")) {
      boost::to_upper(sValue);
      if ((sValue != "TRUE") && (sValue != "FALSE")) {
        std::string errMsg = "ERROR: Unsupported key value for SYS:dfx_enable : '" + sValue + "'";
        throw std::runtime_error(errMsg);
      }

      bDfxEnable = (sValue == "TRUE") ? true : false;

      // Remove the key
      _keyValuePairs.erase(_keyValuePairs.begin() + index);
      break;
    }
  }

  // Build the SYS:mode key value
  std::string modeValue;
  if (_sTarget == "hw")
    modeValue = bDfxEnable ? "hw_pr" : "flat";
  else if (_sTarget == "hw_emu")
    modeValue = bDfxEnable ? "hw_emu_pr" : "hw_emu";
  else if (_sTarget == "sw_emu") {
    if (bDfxEnable)
      throw std::runtime_error("ERROR: Target 'sw_emu' does not support the dfx_enable value of 'TRUE'");
    modeValue = "sw_emu";
  } else {
    std::string errMsg = "ERROR: Unknown target option: '" + _sTarget + "'";
    throw std::runtime_error(errMsg);
  }

  // Add a new key
  std::string keyValue = "SYS:mode:" + modeValue;
  _keyValuePairs.push_back(keyValue);
}

// Program entry point
int main_(int argc, const char** argv) {
  bool bForce = false;
  bool bGetSignature = false;
  bool bListNames = false;
  bool bListSections = false;
  bool bMigrateForward = false;
  bool bQuiet = false;
  bool bRemoveSignature = false;
  bool bValidateSignature = false;
  bool bVerbose = false;
  bool bVersion = false;
  bool fileCheck = false;
  std::string sCertificate;
  std::string sDigestAlgorithm = "sha512";
  std::string sInfoFile;
  std::string sInputFile;
  std::string sOutputFile;
  std::string sPrivateKey;
  std::string sSignature;
  std::string sTarget;
  std::vector<std::string> addPsKernels;
  std::vector<std::string> keysToRemove;
  std::vector<std::string> keyValuePairs;
  std::vector<std::string> sectionsToAdd;
  std::vector<std::string> sectionsToAddMerge;
  std::vector<std::string> sectionsToAddReplace;
  std::vector<std::string> sectionsToDump;
  std::vector<std::string> sectionsToRemove;
  std::vector<std::string> sectionsToReplace;


  namespace po = boost::program_options;

  po::options_description desc("Options");
  desc.add_options()
      ("add-merge-section", boost::program_options::value<decltype(sectionsToAddMerge)>(&sectionsToAddMerge)->multitoken(), "Section name to add or merge.  Format: <section>:<format>:<file>")
      ("add-pskernel", boost::program_options::value<decltype(addPsKernels)>(&addPsKernels)->multitoken(), "Helper option to add PS kernels.  Format: [<mem_banks>]:[<symbol_name>]:[<instances>]:<path_to_shared_library>")
      ("add-replace-section", boost::program_options::value<decltype(sectionsToAddReplace)>(&sectionsToAddReplace)->multitoken(), "Section name to add or replace.  Format: <section>:<format>:<file>")
      ("add-section", boost::program_options::value<decltype(sectionsToAdd)>(&sectionsToAdd)->multitoken(), "Section name to add.  Format: <section>:<format>:<file>")
      ("add-signature", boost::program_options::value<decltype(sSignature)>(&sSignature), "Adds a user defined signature to the given xclbin image.")
      ("certificate", boost::program_options::value<decltype(sCertificate)>(&sCertificate), "Certificate used in signing and validating the xclbin image.")
      ("digest-algorithm", boost::program_options::value<decltype(sDigestAlgorithm)>(&sDigestAlgorithm), "Digest algorithm. Default: sha512")
      ("dump-section", boost::program_options::value<decltype(sectionsToDump)>(&sectionsToDump)->multitoken(), "Section to dump. Format: <section>:<format>:<file>")
      ("force", boost::program_options::bool_switch(&bForce), "Forces a file overwrite.")
      ("get-signature", boost::program_options::bool_switch(&bGetSignature), "Returns the user defined signature (if set) of the xclbin image.")
      ("help,h", "Print help messages")
      ("info", boost::program_options::value<decltype(sInfoFile)>(&sInfoFile)->default_value("")->implicit_value("<console>"), "Report accelerator binary content.  Including: generation and packaging data, kernel signatures, connectivity, clocks, sections, etc.  Note: Optionally an output file can be specified.  If none is specified, then the output will go to the console.")
      ("input,i", boost::program_options::value<std::string>(&sInputFile), "Input file name. Reads xclbin into memory.")
      ("key-value", boost::program_options::value<decltype(keyValuePairs)>(&keyValuePairs)->multitoken(), "Key value pairs.  Format: [USER|SYS]:<key>:<value>")
      ("list-sections", boost::program_options::bool_switch(&bListSections), "List all possible section names (Stand Alone Option)")
      ("migrate-forward", boost::program_options::bool_switch(&bMigrateForward), "Migrate the xclbin archive forward to the new binary format.")
      ("output,o", boost::program_options::value<std::string>(&sOutputFile), "Output file name. Writes in memory xclbin image to a file.")
      ("private-key", boost::program_options::value<decltype(sPrivateKey)>(&sPrivateKey), "Private key used in signing the xclbin image.")
      ("quiet,q", boost::program_options::bool_switch(&bQuiet),     "Minimize reporting information.")
      ("remove-key", boost::program_options::value<decltype(keysToRemove)>(&keysToRemove)->multitoken(), "Removes the given user key from the xclbin archive." )
      ("remove-section", boost::program_options::value<decltype(sectionsToRemove)>(&sectionsToRemove)->multitoken(), "Section name to remove.")
      ("remove-signature", boost::program_options::bool_switch(&bRemoveSignature), "Removes the signature from the xclbin image.")
      ("replace-section", boost::program_options::value<decltype(sectionsToReplace)>(&sectionsToReplace)->multitoken(), "Section to replace. ")
      ("target", boost::program_options::value<decltype(sTarget)>(&sTarget), "Target flow for this image.  Valid values: hw, hw_emu, and sw_emu.")
      ("validate-signature", boost::program_options::bool_switch(&bValidateSignature), "Validates the signature for the given xclbin archive.")
      ("verbose,v", boost::program_options::bool_switch(&bVerbose), "Display verbose/debug information.")
      ("version", boost::program_options::bool_switch(&bVersion), "Version of this executable.")
      ("file-check", boost::program_options::bool_switch(&fileCheck), "Check for Linux file command utility compliance")
 ;

  // hidden options
  bool bResetBankGrouping = false;
  bool bSignatureDebug = false;
  bool bSkipBankGrouping = false;
  bool bSkipUUIDInsertion = false;
  bool bTrace = false;
  bool bTransformPdi = false;
  boost::program_options::options_description hidden("Hidden options");
  std::string sSignatureOutputFile;
  std::vector<std::string> addKernels;
  std::vector<std::string> badOptions;
  std::vector<std::string> sectionsToAppend;

  hidden.add_options()
    ("add-kernel", boost::program_options::value<decltype(addKernels)>(&addKernels)->multitoken(), "Helper option to add fixed kernels.  Format: <path_to_json>")
    ("append-section", boost::program_options::value<decltype(sectionsToAppend)>(&sectionsToAppend)->multitoken(), "Section to append to.")
    ("BAD-DATA", boost::program_options::value<decltype(badOptions)>(&badOptions)->multitoken(), "Dummy Data." )
    ("dump-signature", boost::program_options::value<decltype(sSignatureOutputFile)>(&sSignatureOutputFile), "Dumps a sign xclbin image's signature.")
    ("reset-bank-grouping", boost::program_options::bool_switch(&bResetBankGrouping), "Resets the memory bank grouping section(s).")
    ("signature-debug", boost::program_options::bool_switch(&bSignatureDebug), "Dump section debug data.")
    ("skip-bank-grouping", boost::program_options::bool_switch(&bSkipBankGrouping), "Disables creating the memory bank grouping section(s).")
    ("skip-uuid-insertion", boost::program_options::bool_switch(&bSkipUUIDInsertion), "Do not update the xclbin's UUID")
    ("trace,t", boost::program_options::bool_switch(&bTrace), "Trace")
    ("transform-pdi", boost::program_options::bool_switch(&bTransformPdi), "Transform the PDIs in AIE_PARTITION, this option only valid on Linux")
  ;

  boost::program_options::options_description all("Allowed options");
  all.add(desc).add(hidden);

  po::positional_options_description p;
  p.add("BAD-DATA", -1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).options(all).positional(p).run(), vm);

    if ((vm.count("help")) ||
        (argc == 1)) {
      std::cout << "This utility operates on a xclbin produced by v++.\n\n";
      std::cout << "For example:\n";
      std::cout << "  1) Reporting xclbin information  : xclbinutil --info --input binary_container_1.xclbin\n";
      std::cout << "  2) Extracting the bitstream image: xclbinutil --dump-section BITSTREAM:RAW:bitstream.bit --input binary_container_1.xclbin\n";
      std::cout << "  3) Extracting the build metadata : xclbinutil --dump-section BUILD_METADATA:HTML:buildMetadata.json --input binary_container_1.xclbin\n";
      std::cout << "  4) Removing a section            : xclbinutil --remove-section BITSTREAM --input binary_container_1.xclbin --output binary_container_modified.xclbin\n";
      std::cout << "  5) Signing xclbin                : xclbinutil --private-key key.priv --certificate cert.pem --input binary_container_1.xclbin --output signed.xclbin\n";

      std::cout << std::endl
                << "Command Line Options\n"
                << desc
                << std::endl;

      std::cout << "Addition Syntax Information\n";
      std::cout << "---------------------------\n";
      std::cout << "Syntax: <section>:<format>:<file>\n";
      std::cout << "    <section> - The section to add or dump (e.g., BUILD_METADATA, BITSTREAM, etc.)\n";
      std::cout << "                Note: If a JSON format is being used, this value can be empty.  If so, then\n";
      std::cout << "                      the JSON metadata will determine the section it is associated with.\n";
      std::cout << "                      In addition, only sections that are found in the JSON file will be reported.\n";
      std::cout << "\n";
      std::cout << "    <format>  - The format to be used.  Currently, there are three formats available:\n";
      std::cout << "                RAW: Binary Image; JSON: JSON file format; and HTML: Browser visible.\n";
      std::cout << "\n";
      std::cout << "                Note: Only selected operations and sections supports these file types.\n";
      std::cout << "\n";
      std::cout << "    <file>    - The name of the input/output file to use.\n";
      std::cout << "\n";
      std::cout << "  Used By: --add_section and --dump_section\n";
      std::cout << "  Example: xclbinutil --add-section BITSTREAM:RAW:mybitstream.bit\n";
      std::cout << std::endl;


      return RC_SUCCESS;
    }

    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cerr << desc << std::endl;
    return RC_ERROR_IN_COMMAND_LINE;
  }

  // Check for positional arguments
  if (badOptions.size()) {
    std::string errMsg = "ERROR: Positional arguments (e.g '" + badOptions[0] + "') are not supported.  Please use --input and/or --output if specifying a file.";
    throw std::runtime_error(errMsg);
  }

  // Examine the options
  XUtil::setVerbose(bTrace);
  XUtil::setQuiet(bQuiet);

  if (bVersion) {
    FormattedOutput::reportVersion();
    return RC_SUCCESS;
  }

  if (!bQuiet)
    FormattedOutput::reportVersion(true);

  // Actions not requiring --input
  if (bListSections || bListNames) {
    if (argc != 2)
      throw std::runtime_error("ERROR: The '--list-sections' argument is a stand alone option.  No other options can be specified with it.");

    XUtil::printKinds();
    return RC_SUCCESS;
  }

  // Pre-processing
  // -- Map the target to the mode
  if (!sTarget.empty()) {
    // Make sure that SYS:mode isn't being used
    if (!XclBin::findKeyAndGetValue("SYS","mode", keyValuePairs).empty())
      throw std::runtime_error("ERROR: The option '--target' and the key 'SYS:mode' are mutually exclusive.");

    insertTargetMode(sTarget, keyValuePairs);
  } else {
    // Validate that the SYS:dfx_enable is not set
    if (!XclBin::findKeyAndGetValue("SYS","dfx_enable", keyValuePairs).empty())
      throw std::runtime_error("ERROR: The option '--target' needs to be defined when using 'SYS:dfx_enable'.");
  }

  // If the user is specifying the xclbin's UUID, honor it
  if (!XclBin::findKeyAndGetValue("SYS","XclbinUUID", keyValuePairs).empty())
    bSkipUUIDInsertion = true;

  // Signing DRCs
  if (bValidateSignature == true) {
    if (sCertificate.empty())
      throw std::runtime_error("ERROR: Validate signature specified with no certificate defined.");

    if (sInputFile.empty())
      throw std::runtime_error("ERROR: Validate signature specified with no input file defined.");
  }

  if (!sPrivateKey.empty() && sOutputFile.empty())
    throw std::runtime_error("ERROR: Private key specified, but no output file defined.");

  if (sCertificate.empty() && !sOutputFile.empty() && !sPrivateKey.empty())
    throw std::runtime_error("ERROR: Private key specified, but no certificate defined.");

  // Report option conflicts
  if ((!sSignature.empty() && !sPrivateKey.empty()))
    throw std::runtime_error("ERROR: The options '-add-signature' (a private signature) and '-private-key' (a PKCS signature) are mutually exclusive.");

  // Actions requiring --input

  // Check to see if there any file conflicts
  std::vector< std::string> inputFiles;
  {
    if (!sInputFile.empty())
      inputFiles.push_back(sInputFile);

    if (!sCertificate.empty())
      inputFiles.push_back(sCertificate);

    if (!sPrivateKey.empty())
      inputFiles.push_back(sPrivateKey);

    for (const auto &section : sectionsToAdd) {
      ParameterSectionData psd(section);
      inputFiles.push_back(psd.getFile());
    }

    for (const auto &section : sectionsToAddReplace) {
      ParameterSectionData psd(section);
      inputFiles.push_back(psd.getFile());
    }

    for (const auto &section : sectionsToAddMerge) {
      ParameterSectionData psd(section);
      inputFiles.push_back(psd.getFile());
    }

    for (const auto &section : sectionsToReplace ) {
      ParameterSectionData psd(section);
      inputFiles.push_back(psd.getFile());
    }

    for (const auto & section : sectionsToAppend) {
      ParameterSectionData psd(section);
      inputFiles.push_back(psd.getFile());
    }
  }

  std::vector< std::string> outputFiles;
  {
    if (!sOutputFile.empty())
      outputFiles.push_back(sOutputFile);

    if (!sInfoFile.empty()) {
      if (sInfoFile != "<console>")
        outputFiles.push_back(sInfoFile);
    }

    for (const auto &section : sectionsToDump ) {
      ParameterSectionData psd(section);
      outputFiles.push_back(psd.getFile());
    }
  }

  drcCheckFiles(inputFiles, outputFiles, bForce);

  if (sOutputFile.empty()) {
    XUtil::QUIET("------------------------------------------------------------------------------");
    XUtil::QUIET("Warning: The option '--output' has not been specified. All operations will    ");
    XUtil::QUIET("         be done in memory with the exception of the '--dump-section' command.");
    XUtil::QUIET("------------------------------------------------------------------------------");
  }

  // Dump the signature
  if (!sSignatureOutputFile.empty()) {
    if (sInputFile.empty())
      throw std::runtime_error("ERROR: Missing input file.");

    dumpSignatureFile(sInputFile, sSignatureOutputFile);
    return RC_SUCCESS;
  }

  // Validate signature for the input file
  if (bValidateSignature == true)
    verifyXclBinImage(sInputFile, sCertificate, bSignatureDebug);

  if (!sSignature.empty()) {
    if (sInputFile.empty())
      throw std::runtime_error("ERROR: Cannot add signature.  Missing input file.");

    if(sOutputFile.empty())
      throw std::runtime_error("ERROR: Cannot add signature.  Missing output file.");

    XUtil::addSignature(sInputFile, sOutputFile, sSignature, "");
    XUtil::QUIET("Exiting");
    return RC_SUCCESS;
  }

  if (bGetSignature) {
    if(sInputFile.empty())
      throw std::runtime_error("ERROR: Cannot read signature.  Missing input file.");

    XUtil::reportSignature(sInputFile);
    XUtil::QUIET("Exiting");
    return RC_SUCCESS;
  }

  if (bRemoveSignature) {
    if(sInputFile.empty())
      throw std::runtime_error("ERROR: Cannot remove signature.  Missing input file.");

    if(sOutputFile.empty())
      throw std::runtime_error("ERROR: Cannot remove signature.  Missing output file.");

    XUtil::removeSignature(sInputFile, sOutputFile);
    XUtil::QUIET("Exiting");
    return RC_SUCCESS;
  }

  // -- Read in the xclbin image --
  XclBin xclBin;
  if (!sInputFile.empty()) {
    XUtil::QUIET("Reading xclbin file into memory.  File: " + sInputFile);
    xclBin.readXclBinBinary(sInputFile, bMigrateForward);
  } else {
    XUtil::QUIET("Creating a default 'in-memory' xclbin image.");
  }

  // -- Remove Sections --
  for (const auto &section : sectionsToRemove)
    xclBin.removeSection(section);

  // -- Add or Replace Sections --
  for (const auto &section : sectionsToAddReplace) {
    ParameterSectionData psd(section);
    xclBin.addReplaceSection( psd );
  }

  // -- Replace Sections --
  for (const auto &section : sectionsToReplace) {
    ParameterSectionData psd(section);
    xclBin.replaceSection( psd );
  }

  // -- Add Sections --
  for (const auto &section : sectionsToAdd) {
    ParameterSectionData psd(section);
    if (psd.getSectionName().empty() &&
        psd.getFormatType() == Section::FormatType::json) {
      xclBin.addSections(psd);
    } else {
      xclBin.addSection(psd);
    }
  }

  // -- Add or Merge Sections --
  for (const auto &section : sectionsToAddMerge) {
    ParameterSectionData psd(section);
    xclBin.addMergeSection( psd );
  }

  // -- Append to Sections --
  for (const auto &section : sectionsToAppend) {
    ParameterSectionData psd(section);
    if (psd.getSectionName().empty() &&
        psd.getFormatType() == Section::FormatType::json) {
      xclBin.appendSections(psd);
    } else {
      std::string errMsg = "ERROR: Appending of sections only supported via wildcards and the JSON format (e.g. :JSON:appendfile.rtd).";
      throw std::runtime_error(errMsg);
    }
  }

  // -- Add PS Kernels
  for (const auto &psKernel : addPsKernels)
    xclBin.addPsKernel(psKernel);

  // -- Add Fixed Kernels files
  for (const auto &kernel : addKernels)
    xclBin.addKernels(kernel);

  // -- Post Section Processing --
  if ( bResetBankGrouping ||
      (( !addKernels.empty() || !addPsKernels.empty()) && !bSkipBankGrouping)) {
    if (xclBin.findSection(ASK_GROUP_TOPOLOGY) != nullptr)
      xclBin.removeSection("GROUP_TOPOLOGY");

    if (xclBin.findSection(ASK_GROUP_CONNECTIVITY) != nullptr)
      xclBin.removeSection("GROUP_CONNECTIVITY");
  }

  if (xclBin.findSection(IP_LAYOUT) != nullptr && xclBin.findSection(AIE_PARTITION) != nullptr){
    if(!XUtil::checkAIEPartitionIPLayoutCompliance(xclBin)){
      throw std::runtime_error("ERROR: The AIE_PARTITION section in the xclbin is not compliant with IP_LAYOUT section");
    }
  }

  // Auto add GROUP_TOPOLOGY and/or GROUP_CONNECTIVITY
  if ((bSkipBankGrouping == false) &&
      (xclBin.findSection(ASK_GROUP_TOPOLOGY) == nullptr) &&
      (xclBin.findSection(ASK_GROUP_CONNECTIVITY) == nullptr) &&
      (xclBin.findSection(MEM_TOPOLOGY) != nullptr))
    XUtil::createMemoryBankGrouping(xclBin);

  // add support for transform-pdi
  // transform the PDIs in AIE_PARTITION sections before writing out the output xclbin
  if (bTransformPdi) {
#ifndef _WIN32
    XUtil::transformAiePartitionPDIs(xclBin);
#else
    std::string errMsg = "ERROR: --transform-pdi is only valid on Linux.";
    throw std::runtime_error(errMsg);
#endif
  }

  // -- Remove Keys --
  for (const auto &key : keysToRemove)
    xclBin.removeKey(key);

  // -- Add / Set Keys --
  for (const auto &keyValue : keyValuePairs)
    xclBin.setKeyValue(keyValue);

  // -- Update Interface uuid in xclbin --
  xclBin.updateInterfaceuuid();

  // -- Dump Sections --
  for (const auto &section : sectionsToDump) {
    ParameterSectionData psd(section);
    if (psd.getSectionName().empty() &&
        psd.getFormatType() == Section::FormatType::json) {
      xclBin.dumpSections(psd);
    } else {
      xclBin.dumpSection(psd);
    }
  }

  // -- Write out new xclbin image --
  if (!sOutputFile.empty()) {
    xclBin.writeXclBinBinary(sOutputFile, bSkipUUIDInsertion);

    if (!sPrivateKey.empty() && !sCertificate.empty())
      signXclBinImage(sOutputFile, sPrivateKey, sCertificate, sDigestAlgorithm, bSignatureDebug);
  }

  // -- Redirect INFO output --
  if (!sInfoFile.empty()) {
    if (sInfoFile == "<console>") {
      xclBin.reportInfo(std::cout, sInputFile, bVerbose);
    } else {
      std::fstream oInfoFile;
      oInfoFile.open(sInfoFile, std::ifstream::out | std::ifstream::binary);
      if (!oInfoFile.is_open()) {
        std::string errMsg = "ERROR: Unable to open the info file for writing: " + sInfoFile;
        throw std::runtime_error(errMsg);
      }
      xclBin.reportInfo(oInfoFile, sInputFile, bVerbose);
      oInfoFile.close();
    }
  }

  if (fileCheck) {
    if (!xclBin.checkForValidSection() && !xclBin.checkForPlatformVbnv())
      throw std::runtime_error("ERROR: The xclbin is missing platformVBNV information and at least one section required by the 'file' command to identify its file type and display file characteristics.");

    else if (!xclBin.checkForPlatformVbnv())
      throw std::runtime_error("ERROR: The xclbin is missing platformVBNV information required by the 'file' command to identify its file type and display file characteristics.");

    else if (!xclBin.checkForValidSection())
      throw std::runtime_error("ERROR: The xclbin is missing at least one section required by the 'file' command to identify its file type and display file characteristics.");
  }
  XUtil::QUIET("Leaving xclbinutil.");

  return RC_SUCCESS;
}
