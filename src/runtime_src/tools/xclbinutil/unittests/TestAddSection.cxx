#include "ParameterSectionData.h"
#include "XclBinClass.h"
#include "globals.h"
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

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
TEST(AddReplaceSection, AddReplaceClearingBitstream) {
   XclBin xclBin;
  
   // We will be testing using the clearing bitstream section  
   const std::string sSection = "CLEARING_BITSTREAM";
   enum axlf_section_kind _eKind;
   Section::translateSectionKindStrToKind(sSection, _eKind);

   // Load the xclbin image of interest
   boost::filesystem::path sampleXclbin(TestUtilities::getResourceDir());
   sampleXclbin /= "sample_1_2018.2.xclbin";
   xclBin.readXclBinBinary(sampleXclbin.string(), false /* bMigrateForward */);

   // Check that the clearing bitstream" section does not exist
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
      // Replace the contents of the "clearning bitstream" section
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



