#include "ParameterSectionData.h"
#include "XclBinClass.h"
#include "globals.h"
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

// Simple Add Test
TEST(AddSection, AddClearingBitstream) {
   XclBin xclBin;
  
   std::string sSection = "CLEARING_BITSTREAM";

   enum axlf_section_kind _eKind;
   Section::translateSectionKindStrToKind(sSection, _eKind);

   // Get the file of interest
   boost::filesystem::path sampleXclbin(TestUtilities::getResourceDir());
   sampleXclbin /= "sample_1_2018.2.xclbin";

   xclBin.readXclBinBinary(sampleXclbin.string(), false /* bMigrateForward */);

   // Check to see if section exists
   const Section * pSection = xclBin.findSection(_eKind);
   ASSERT_EQ(pSection, nullptr) << "Section '" << sSection << "' found.";

   // Add Section
   const std::string formattedString = std::string("CLEARING_BITSTREAM:RAW:") + sampleXclbin.string();
   ParameterSectionData psd(formattedString);
   xclBin.addSection(psd);

   // Check to see if section was removed
   pSection = xclBin.findSection(_eKind);
   ASSERT_NE(pSection, nullptr) << "Section  '" << sSection << "' was not added.";
}

// Add or replace test
TEST(AddSection, AddReplaceClearingBitstream) {
   XclBin xclBin;
  
   // We will be testing using the CLEARING_BITSTREAM section  
   const std::string sSection = "CLEARING_BITSTREAM";
   enum axlf_section_kind _eKind;
   Section::translateSectionKindStrToKind(sSection, _eKind);

   // Load the xclbin image of interest
   boost::filesystem::path sampleXclbin(TestUtilities::getResourceDir());
   sampleXclbin /= "sample_1_2018.2.xclbin";
   xclBin.readXclBinBinary(sampleXclbin.string(), false /* bMigrateForward */);

   // Check that the CLEARING_BITSTREAM section does not exist
   const Section * pSection = xclBin.findSection(_eKind);
   ASSERT_EQ(pSection, nullptr) << "Section '" << sSection << "' found.";

   {
      // Add dummy unique data to the "clearning bitstream" section
      boost::filesystem::path uniqueData1(TestUtilities::getResourceDir());
      uniqueData1 /= "unique_data1.bin";

      const std::string formattedString = sSection + ":RAW:" + uniqueData1.string();
      ParameterSectionData psd(formattedString);
      xclBin.addReplaceSection(psd);

      // Check to see if section was added
      pSection = xclBin.findSection(_eKind);
      ASSERT_NE(pSection, nullptr) << "Section  '" << sSection << "' was not added.";
   }

   // Record the contents that was read in
   std::ostringstream uniqueData1Contents;
   pSection->dumpContents(uniqueData1Contents, Section::FT_RAW);

   {
      // Replace the contents of the CLEARING_BITSTREAM section
      boost::filesystem::path uniqueData2(TestUtilities::getResourceDir());
      uniqueData2 /= "unique_data2.bin";

      const std::string formattedString = sSection + ":RAW:" + uniqueData2.string();
      ParameterSectionData psd(formattedString);
      xclBin.addReplaceSection(psd);

      // Check to see if section is still there
      pSection = xclBin.findSection(_eKind);
      ASSERT_NE(pSection, nullptr) << "Section  '" << sSection << "' does not exist.";
   }

   // Record the contents that was read in
   std::ostringstream uniqueData2Contents;
   pSection->dumpContents(uniqueData2Contents, Section::FT_RAW);

   // Validate the data is different
   ASSERT_TRUE(uniqueData1Contents.str().compare(uniqueData2Contents.str())) << "Data contents was not replaced";
}

// Add or replace test
TEST(AddSection, AddMergeIPLayout) {
   // We will be testing using the clearing bitstream section  
   const std::string sSection = "IP_LAYOUT";
   enum axlf_section_kind _eKind;
   Section::translateSectionKindStrToKind(sSection, _eKind);

   // Start with an empty xclbin image
   XclBin xclBin;

   // Check that the IP_LAYOUT does not exist
   const Section * pSection = xclBin.findSection(_eKind);
   ASSERT_EQ(pSection, nullptr) << "Section '" << sSection << "' found.";

   {
      // Add an IP_LAYOUT section
      boost::filesystem::path ip_layoutBase(TestUtilities::getResourceDir());
      ip_layoutBase /= "ip_layout_base.json";

      const std::string formattedString = sSection + ":JSON:" + ip_layoutBase.string();
      ParameterSectionData psd(formattedString);
      xclBin.addMergeSection(psd);

      // Check to see if section was added
      pSection = xclBin.findSection(_eKind);
      ASSERT_NE(pSection, nullptr) << "Section  '" << sSection << "' was not added.";
   }

   {
      // Merge additional data into the IP_LAYOUT seciton
      boost::filesystem::path ip_layoutMerge(TestUtilities::getResourceDir());
      ip_layoutMerge /= "ip_layout_merge.json";

      const std::string formattedString = sSection + ":JSON:" + ip_layoutMerge.string();
      ParameterSectionData psd(formattedString);
      xclBin.addMergeSection(psd);

      // Check to see if section is still there
      pSection = xclBin.findSection(_eKind);
      ASSERT_NE(pSection, nullptr) << "Section  '" << sSection << "' does not exist.";
   }

   // Dump the resulting image to disk
   const std::string outputFile = "ip_layout_merged_output.json";
   std::fstream oDumpFile(outputFile, std::ifstream::out | std::ifstream::binary);
   ASSERT_TRUE(oDumpFile.is_open()) << "Unable to open the file for writing: " << outputFile;
   pSection->dumpContents(oDumpFile, Section::FT_JSON);

   // Validate the JSON images on disk
   std::stringstream obuffer;
   {
      boost::property_tree::ptree ptOutput;
      boost::property_tree::read_json(outputFile, ptOutput);
      boost::property_tree::write_json(obuffer, ptOutput);
   }

   boost::filesystem::path ip_layoutMergeExpect(TestUtilities::getResourceDir());
   ip_layoutMergeExpect /= "ip_layout_merged_expected.json";
   std::stringstream ebuffer;
   {
      boost::property_tree::ptree ptExpected;
      boost::property_tree::read_json(ip_layoutMergeExpect.string(), ptExpected);
      boost::property_tree::write_json(ebuffer, ptExpected);
   }

   ASSERT_TRUE(obuffer.str().compare(ebuffer.str()) == 0) 
      << "Unexpected JSON file produced." <<  std::endl 
      << "Output  : " << outputFile << std::endl 
      << "Expected: " << ip_layoutMergeExpect;
}



