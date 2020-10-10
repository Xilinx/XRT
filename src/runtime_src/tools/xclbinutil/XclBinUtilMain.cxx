/**
 * Copyright (C) 2018-2020 Xilinx, Inc
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
#include "XclBinClass.h"
#include "ParameterSectionData.h"
#include "FormattedOutput.h"
#include "XclBinSignature.h"
#include "xclbin.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <stdexcept>

// System - Include Files
#include <iostream>
#include <string>
#include <set>

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


static bool bQuiet = false;
void QUIET(const std::string _msg){
  if (bQuiet == false) {
    std::cout << _msg.c_str() << std::endl;
  }
}

// Program entry point
int main_(int argc, const char** argv) {
  bool bVerbose = false;
  bool bTrace = false;
  bool bMigrateForward = false;
  bool bListNames = false;
  bool bListSections = false;
  std::string sInfoFile;
  bool bSkipUUIDInsertion = false;
  bool bSkipBankGrouping = false;
  bool bVersion = false;
  bool bForce = false;

  bool bRemoveSignature = false;
  std::string sSignature;
  bool bGetSignature = false;
  bool bSignatureDebug = false;
  std::string sSignatureOutputFile;
  std::string sDigestAlgorithm = "sha512";

  std::string sPrivateKey;
  std::string sCertificate;
  bool bValidateSignature = false;

  std::string sTarget;

  std::string sInputFile;
  std::string sOutputFile;

  std::vector<std::string> sectionsToReplace;
  std::vector<std::string> sectionsToAdd;
  std::vector<std::string> sectionsToRemove;
  std::vector<std::string> sectionsToDump;
  std::vector<std::string> sectionsToAppend;

  std::vector<std::string> keyValuePairs;
  std::vector<std::string> keysToRemove;


  namespace po = boost::program_options;

  po::options_description desc("Options");
  desc.add_options()
      ("help,h", "Print help messages")
      ("input,i", boost::program_options::value<std::string>(&sInputFile), "Input file name. Reads xclbin into memory.")
      ("output,o", boost::program_options::value<std::string>(&sOutputFile), "Output file name. Writes in memory xclbin image to a file.")

      ("target", boost::program_options::value<decltype(sTarget)>(&sTarget), "Target flow for this image.  Valid values: HW, HW_EMU, and SW_EMU.")

      ("private-key", boost::program_options::value<std::string>(&sPrivateKey), "Private key used in signing the xclbin image.")
      ("certificate", boost::program_options::value<std::string>(&sCertificate), "Certificate used in signing and validating the xclbin image.")
      ("digest-algorithm", boost::program_options::value<std::string>(&sDigestAlgorithm), "Digest algorithm. Default: sha512")
      ("validate-signature", boost::program_options::bool_switch(&bValidateSignature), "Validates the signature for the given xclbin archive.")

      ("verbose,v", boost::program_options::bool_switch(&bVerbose), "Display verbose/debug information.")
      ("quiet,q", boost::program_options::bool_switch(&bQuiet),     "Minimize reporting information.")

      ("migrate-forward", boost::program_options::bool_switch(&bMigrateForward), "Migrate the xclbin archive forward to the new binary format.")

      ("remove-section", boost::program_options::value<std::vector<std::string> >(&sectionsToRemove)->multitoken(), "Section name to remove.")
      ("add-section", boost::program_options::value<std::vector<std::string> >(&sectionsToAdd)->multitoken(), "Section name to add.  Format: <section>:<format>:<file>")
      ("dump-section", boost::program_options::value<std::vector<std::string> >(&sectionsToDump)->multitoken(), "Section to dump. Format: <section>:<format>:<file>")
      ("replace-section", boost::program_options::value<std::vector<std::string> >(&sectionsToReplace)->multitoken(), "Section to replace. ")

      ("key-value", boost::program_options::value<std::vector<std::string> >(&keyValuePairs)->multitoken(), "Key value pairs.  Format: [USER|SYS]:<key>:<value>")
      ("remove-key", boost::program_options::value<std::vector<std::string> >(&keysToRemove)->multitoken(), "Removes the given user key from the xclbin archive." )

      ("add-signature", boost::program_options::value<std::string>(&sSignature), "Adds a user defined signature to the given xclbin image.")
      ("remove-signature", boost::program_options::bool_switch(&bRemoveSignature), "Removes the signature from the xclbin image.")
      ("get-signature", boost::program_options::bool_switch(&bGetSignature), "Returns the user defined signature (if set) of the xclbin image.")

      ("info", boost::program_options::value<std::string>(&sInfoFile)->default_value("")->implicit_value("<console>"), "Report accelerator binary content.  Including: generation and packaging data, kernel signatures, connectivity, clocks, sections, etc.  Note: Optionally an output file can be specified.  If none is specified, then the output will go to the console.")
      ("list-sections", boost::program_options::bool_switch(&bListSections), "List all possible section names (Stand Alone Option)")
      ("version", boost::program_options::bool_switch(&bVersion), "Version of this executable.")
      ("force", boost::program_options::bool_switch(&bForce), "Forces a file overwrite.")
 ;

  // hidden options
  std::vector<std::string> badOptions;
  boost::program_options::options_description hidden("Hidden options");

  hidden.add_options()
    ("trace,t", boost::program_options::bool_switch(&bTrace), "Trace")
    ("skip-uuid-insertion", boost::program_options::bool_switch(&bSkipUUIDInsertion), "Do not update the xclbin's UUID")
    ("append-section", boost::program_options::value<std::vector<std::string> >(&sectionsToAppend)->multitoken(), "Section to append to.")
    ("signature-debug", boost::program_options::bool_switch(&bSignatureDebug), "Dump section debug data.")
    ("dump-signature", boost::program_options::value<std::string>(&sSignatureOutputFile), "Dumps a sign xclbin image's signature.")
    ("skip-bank-grouping", boost::program_options::bool_switch(&bSkipBankGrouping), "Disables creating the memory bank grouping section(s).")
    ("BAD-DATA", boost::program_options::value<std::vector<std::string> >(&badOptions)->multitoken(), "Dummy Data." )
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
      std::cout << "This utility operates on a xclbin produced by v++." << std::endl << std::endl;
      std::cout << "For example:" << std::endl;
      std::cout << "  1) Reporting xclbin information  : xclbinutil --info --input binary_container_1.xclbin" << std::endl;
      std::cout << "  2) Extracting the bitstream image: xclbinutil --dump-section BITSTREAM:RAW:bitstream.bit --input binary_container_1.xclbin" << std::endl;
      std::cout << "  3) Extracting the build metadata : xclbinutil --dump-section BUILD_METADATA:HTML:buildMetadata.json --input binary_container_1.xclbin" << std::endl;
      std::cout << "  4) Removing a section            : xclbinutil --remove-section BITSTREAM --input binary_container_1.xclbin --output binary_container_modified.xclbin" << std::endl;
      std::cout << "  5) Signing xclbin                : xclbinutil --private-key key.priv --certificate cert.pem --input binary_container_1.xclbin --output signed.xclbin" << std::endl;

      std::cout << std::endl
                << "Command Line Options" << std::endl
                << desc
                << std::endl;

      std::cout << "Addition Syntax Information" << std::endl;
      std::cout << "---------------------------" << std::endl;
      std::cout << "Syntax: <section>:<format>:<file>" << std::endl;
      std::cout << "    <section> - The section to add or dump (e.g., BUILD_METADATA, BITSTREAM, etc.)"  << std::endl;
      std::cout << "                Note: If a JSON format is being used, this value can be empty.  If so, then" << std::endl;
      std::cout << "                      the JSON metadata will determine the section it is associated with." << std::endl;
      std::cout << "                      In addition, only sections that are found in the JSON file will be reported." << std::endl;
      std::cout << std::endl;
      std::cout << "    <format>  - The format to be used.  Currently, there are three formats available: " << std::endl;
      std::cout << "                RAW: Binary Image; JSON: JSON file format; and HTML: Browser visible." << std::endl;
      std::cout << std::endl;
      std::cout << "                Note: Only selected operations and sections supports these file types."  << std::endl;
      std::cout << std::endl;
      std::cout << "    <file>    - The name of the input/output file to use." << std::endl;
      std::cout << std::endl;
      std::cout << "  Used By: --add_section and --dump_section" << std::endl;
      std::cout << "  Example: xclbinutil --add-section BITSTREAM:RAW:mybitstream.bit"  << std::endl;
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

  if (bVersion) {
    FormattedOutput::reportVersion();
    return RC_SUCCESS;
  }

  if (!bQuiet) {
    FormattedOutput::reportVersion(true);
  }

  // Actions not requiring --input
  if (bListSections || bListNames) {
    if (argc != 2) {
      std::string errMsg = "ERROR: The '--list-sections' argument is a stand alone option.  No other options can be specified with it.";
      throw std::runtime_error(errMsg);
    }
    XUtil::printKinds();
    return RC_SUCCESS;
  }

  // Pre-processing
  // -- Map the target to the mode
  if (!sTarget.empty()) {
    // Make sure that SYS:mode isn't being used
    if (!XclBin::findKeyAndGetValue("SYS","mode", keyValuePairs).empty()) {
      std::string errMsg = "ERROR: The option '--target' and the key 'SYS:mode' are mutually exclusive.";
      throw std::runtime_error(errMsg);
    }

    insertTargetMode(sTarget, keyValuePairs);
  } else {
    // Validate that the SYS:dfx_enable is not set
    if (!XclBin::findKeyAndGetValue("SYS","dfx_enable", keyValuePairs).empty()) {
      std::string errMsg = "ERROR: The option '--target' needs to be defined when using 'SYS:dfx_enable'.";
      throw std::runtime_error(errMsg);
    }
  }

  // Signing DRCs
  if (bValidateSignature == true) {
    if (sCertificate.empty()) {
      throw std::runtime_error("ERROR: Validate signature specified with no certificate defined.");
    }

    if (sInputFile.empty()) {
      throw std::runtime_error("ERROR: Validate signature specified with no input file defined.");
    }
  }

  if (!sPrivateKey.empty() && sOutputFile.empty()) {
    throw std::runtime_error("ERROR: Private key specified, but no output file defined.");
  }

  if (sCertificate.empty() && !sOutputFile.empty() && !sPrivateKey.empty()) {
    throw std::runtime_error("ERROR: Private key specified, but no certificate defined.");
  }

  // Report option conflicts
  if ((!sSignature.empty() && !sPrivateKey.empty())) {
    throw std::runtime_error("ERROR: The options '-add-signature' (a private signature) and '-private-key' (a PKCS signature) are mutually exclusive.");
  }

  // Actions requiring --input

  // Check to see if there any file conflicts
  std::vector< std::string> inputFiles;
  {
    if (!sInputFile.empty()) {
       inputFiles.push_back(sInputFile);
    }

    if (!sCertificate.empty()) {
       inputFiles.push_back(sCertificate);
    }

    if (!sPrivateKey.empty()) {
       inputFiles.push_back(sPrivateKey);
    }

    for (auto section : sectionsToAdd) {
      ParameterSectionData psd(section);
      inputFiles.push_back(psd.getFile());
    }

    for (auto section : sectionsToReplace ) {
      ParameterSectionData psd(section);
      inputFiles.push_back(psd.getFile());
    }

    for (auto section : sectionsToAppend) {
      ParameterSectionData psd(section);
      inputFiles.push_back(psd.getFile());
    }
  }

  std::vector< std::string> outputFiles;
  {
    if (!sOutputFile.empty()) {
      outputFiles.push_back(sOutputFile);
    }

    if (!sInfoFile.empty()) {
      if (sInfoFile != "<console>") {
        outputFiles.push_back(sInfoFile);
      }
    }

    for (auto section : sectionsToDump ) {
      ParameterSectionData psd(section);
      outputFiles.push_back(psd.getFile());
    }
  }

  drcCheckFiles(inputFiles, outputFiles, bForce);

  if (sOutputFile.empty()) {
    QUIET("------------------------------------------------------------------------------");
    QUIET("Warning: The option '--output' has not been specified. All operations will    ");
    QUIET("         be done in memory with the exception of the '--dump-section' command.");
    QUIET("------------------------------------------------------------------------------");
  }

  // Dump the signature
  if (!sSignatureOutputFile.empty()) {
    if (sInputFile.empty()) {
      throw std::runtime_error("ERROR: Missing input file.");
    }
    dumpSignatureFile(sInputFile, sSignatureOutputFile);
    return RC_SUCCESS;
  }

  // Validate signature for the input file
  if (bValidateSignature == true) {
    verifyXclBinImage(sInputFile, sCertificate, bSignatureDebug);
  }

  if (!sSignature.empty()) {
    if (sInputFile.empty()) {
      std::string errMsg = "ERROR: Cannot add signature.  Missing input file.";
      throw std::runtime_error(errMsg);
    }
    if(sOutputFile.empty()) {
      std::string errMsg = "ERROR: Cannot add signature.  Missing output file.";
      throw std::runtime_error(errMsg);
    }
    XUtil::addSignature(sInputFile, sOutputFile, sSignature, "");
    QUIET("Exiting");
    return RC_SUCCESS;
  }

  if (bGetSignature) {
    if(sInputFile.empty()) {
      std::string errMsg = "ERROR: Cannot read signature.  Missing input file.";
      throw std::runtime_error(errMsg);
    }
    XUtil::reportSignature(sInputFile);
    QUIET("Exiting");
    return RC_SUCCESS;
  }

  if (bRemoveSignature) {
    if(sInputFile.empty()) {
      std::string errMsg = "ERROR: Cannot remove signature.  Missing input file.";
      throw std::runtime_error(errMsg);
    }
    if(sOutputFile.empty()) {
      std::string errMsg = "ERROR: Cannot remove signature.  Missing output file.";
      throw std::runtime_error(errMsg);
    }
    XUtil::removeSignature(sInputFile, sOutputFile);
    QUIET("Exiting");
    return RC_SUCCESS;
  }

  // -- Read in the xclbin image --
  XclBin xclBin;
  if (!sInputFile.empty()) {
    QUIET("Reading xclbin file into memory.  File: " + sInputFile);
    xclBin.readXclBinBinary(sInputFile, bMigrateForward);
  } else {
    QUIET("Creating a default 'in-memory' xclbin image.");
  }

  // -- DRC checks --
  // Determine if we should auto create the GROUP_TOPOLOGY or GROUP_CONNECTIVITY sections
  if ((xclBin.findSection(ASK_GROUP_TOPOLOGY) != nullptr) ||
      (xclBin.findSection(ASK_GROUP_CONNECTIVITY) != nullptr)) {
    // GROUP Sections already exist don't modify them.
    bSkipBankGrouping = true;
  }

  // -- Remove Sections --
  for (auto section : sectionsToRemove) 
    xclBin.removeSection(section);

  // -- Replace Sections --
  for (auto section : sectionsToReplace) {
    ParameterSectionData psd(section);
    xclBin.replaceSection( psd );
  }

  // -- Add Sections --
  for (auto section : sectionsToAdd) {
    ParameterSectionData psd(section);
    if (psd.getSectionName().empty() &&
        psd.getFormatType() == Section::FT_JSON) {
      xclBin.addSections(psd);
    } else {
      xclBin.addSection(psd);
    }
  }

  // -- Append to Sections --
  for (auto section : sectionsToAppend) {
    ParameterSectionData psd(section);
    if (psd.getSectionName().empty() &&
        psd.getFormatType() == Section::FT_JSON) {
      xclBin.appendSections(psd);
    } else {
      std::string errMsg = "ERROR: Appending of sections only supported via wildcards and the JSON format (e.g. :JSON:appendfile.rtd).";
      throw std::runtime_error(errMsg);
    }
  }

  // -- Post Section Processing --
  // Auto add GROUP_TOPOLOGY and/or GROUP_CONNECTIVITY
  if ((bSkipBankGrouping == false) &&
      (xclBin.findSection(ASK_GROUP_TOPOLOGY) == nullptr) &&
      (xclBin.findSection(ASK_GROUP_CONNECTIVITY) == nullptr) &&
      (xclBin.findSection(MEM_TOPOLOGY) != nullptr))
  {
    XUtil::createMemoryBankGrouping(xclBin);
  } 

  // -- Remove Keys --
  for (auto key : keysToRemove) {
    xclBin.removeKey(key);
  }

  // -- Add / Set Keys --
  for (auto keyValue : keyValuePairs) {
    xclBin.setKeyValue(keyValue);
  }

  // -- Dump Sections --
  for (auto section : sectionsToDump) {
    ParameterSectionData psd(section);
    if (psd.getSectionName().empty() &&
        psd.getFormatType() == Section::FT_JSON) {
      xclBin.dumpSections(psd);
    } else {
      xclBin.dumpSection(psd);
    }
  }

  // -- Write out new xclbin image --
  if (!sOutputFile.empty()) {
    xclBin.writeXclBinBinary(sOutputFile, bSkipUUIDInsertion);

    if (!sPrivateKey.empty() && !sCertificate.empty()) {
      signXclBinImage(sOutputFile, sPrivateKey, sCertificate, sDigestAlgorithm, bSignatureDebug);
    }
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

  QUIET("Leaving xclbinutil.");

  return RC_SUCCESS;
}
