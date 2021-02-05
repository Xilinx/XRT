#include <gtest/gtest.h>
#include "XclBinClass.h"
#include "ParameterSectionData.h"
#include "globals.h"

#include <string>
#include <boost/filesystem.hpp>

TEST(MetaData, AddingMissingFile) {
  XclBin xclBin;
  const std::string formattedString = "BUILD_METADATA:JSON:junk.json";
  ParameterSectionData psd(formattedString);

  ASSERT_THROW (xclBin.addSection(psd), std::runtime_error);
}

TEST(MetaData, AddingValidFile) {
  // Get the file of interest
  boost::filesystem::path sampleMetadata(TestUtilities::getResourceDir());
  sampleMetadata /= "metadata.json";

  const std::string formattedString = std::string("BUILD_METADATA:JSON:") + sampleMetadata.string();
  ParameterSectionData psd(formattedString);

  XclBin xclBin;
  xclBin.addSection(psd);
}


