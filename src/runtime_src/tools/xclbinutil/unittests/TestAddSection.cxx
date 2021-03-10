#include "ParameterSectionData.h"
#include "XclBinClass.h"
#include "globals.h"
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

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

   // Remove Section
   const std::string formattedString = std::string("CLEARING_BITSTREAM:RAW:") + sampleXclbin.string();
   ParameterSectionData psd(formattedString);
   xclBin.addSection(psd);

   // Check to see if section was removed
   pSection = xclBin.findSection(_eKind);
   ASSERT_NE(pSection, nullptr) << "Section  '" << sSection << "' was not added.";
}



