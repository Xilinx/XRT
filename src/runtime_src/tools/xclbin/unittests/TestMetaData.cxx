#include <gtest/gtest.h>
#include "XclBinClass.h"
#include "ParameterSectionData.h"

#include <string>

using std::string;

TEST(MetaData, AddingMissingFile) {
  XclBin xclBin;
  const std::string formattedString = "BUILD_METADATA:junk.json:json";
  ParameterSectionData psd(formattedString);

  ASSERT_THROW (xclBin.addSection(psd), std::runtime_error);
}

TEST(MetaData, AddingValidFile) {
  XclBin xclBin;
  const std::string formattedString = "BUILD_METADATA:JSON:unittests/test_data/metadata.json";
  ParameterSectionData psd(formattedString);
  xclBin.addSection(psd);
}


