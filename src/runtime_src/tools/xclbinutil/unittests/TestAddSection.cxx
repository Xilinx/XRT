#include <gtest/gtest.h>
#include "ParameterSectionData.h"
#include "XclBinClass.h"

TEST(AddSection, AddClearingBitstream) {
   XclBin xclBin;
  
   std::string sSection = "CLEARING_BITSTREAM";

   enum axlf_section_kind _eKind;
   Section::translateSectionKindStrToKind(sSection, _eKind);

   xclBin.readXclBinBinary("unittests/test_data/sample_1_2018.2.xclbin", false /* bMigrateForward */);

   // Check to see if section exists
   const Section * pSection = xclBin.findSection(_eKind);
   ASSERT_EQ(pSection, nullptr) << "Section '" << sSection << "' found.";

   // Remove Section
   const std::string formattedString = "CLEARING_BITSTREAM:RAW:unittests/test_data/sample_1_2018.2.xclbin";
   ParameterSectionData psd(formattedString);
   xclBin.addSection(psd);

   // Check to see if section was removed
   pSection = xclBin.findSection(_eKind);
   ASSERT_NE(pSection, nullptr) << "Section  '" << sSection << "' was not added.";
}



