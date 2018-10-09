#include <gtest/gtest.h>
#include "ParameterSectionData.h"
#include "XclBin.h"

TEST(RemoveSection, RemoveBitstream) {
   XclBin xclBin;
  
   std::string sSection = "BITSTREAM";

   enum axlf_section_kind _eKind;
   Section::translateSectionKindStrToKind(sSection, _eKind);

   xclBin.readXclBinBinary("unittests/test_data/sample_1_2018.2.xclbin", false /* bMigrateForward */);

   // Check to see if section exists
   const Section * pSection = xclBin.findSection(_eKind);
   ASSERT_NE(pSection, nullptr) << "Section '" << sSection << "' not found.";

   // Remove Section
   xclBin.removeSection(sSection);

   // Check to see if section was removed
   pSection = xclBin.findSection(_eKind);
   ASSERT_EQ(pSection, nullptr) << "Section  '" << sSection << "' was not removed.";
}



