#include <gtest/gtest.h>
#include "ParameterSectionData.h"
#include "XclBinClass.h"
#include "globals.h"

#if __has_include(<filesystem>)
  #include <filesystem>
  namespace fs = std::filesystem;
#else
  error "Missing the <filesystem> header."
#endif

TEST(RemoveSection, RemoveBitstream) {
   XclBin xclBin;
  
   std::string sSection = "BITSTREAM";

   enum axlf_section_kind _eKind;
   Section::translateSectionKindStrToKind(sSection, _eKind);

   // Get the file of interest
   fs::path sampleXclbin(TestUtilities::getResourceDir());
   sampleXclbin /= "sample_1_2018.2.xclbin";

   xclBin.readXclBinBinary(sampleXclbin.string(), false /* bMigrateForward */);

   // Check to see if section exists
   const Section * pSection = xclBin.findSection(_eKind);
   ASSERT_NE(pSection, nullptr) << "Section '" << sSection << "' not found.";

   // Remove Section
   xclBin.removeSection(sSection);

   // Check to see if section was removed
   pSection = xclBin.findSection(_eKind);
   ASSERT_EQ(pSection, nullptr) << "Section  '" << sSection << "' was not removed.";
}



