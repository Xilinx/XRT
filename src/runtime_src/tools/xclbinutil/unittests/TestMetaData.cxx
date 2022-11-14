#include <gtest/gtest.h>
#include "XclBinClass.h"
#include "ParameterSectionData.h"
#include "globals.h"

#if __has_include(<filesystem>)
  #include <filesystem>
  namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
  #include <experimental/filesystem>
  namespace fs = std::experimental::filesystem;
#else
  error "Missing the <filesystem> header."
#endif
#include <string>

TEST(MetaData, AddingMissingFile) {
  XclBin xclBin;
  const std::string formattedString = "BUILD_METADATA:JSON:junk.json";
  ParameterSectionData psd(formattedString);

  ASSERT_THROW (xclBin.addSection(psd), std::runtime_error);
}

TEST(MetaData, AddingValidFile) {
  // Get the file of interest
  fs::path sampleMetadata(TestUtilities::getResourceDir());
  sampleMetadata /= "metadata.json";

  const std::string formattedString = std::string("BUILD_METADATA:JSON:") + sampleMetadata.string();
  ParameterSectionData psd(formattedString);

  XclBin xclBin;
  xclBin.addSection(psd);
}


