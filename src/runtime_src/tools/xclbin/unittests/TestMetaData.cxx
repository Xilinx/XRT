#include <gtest/gtest.h>
#include "XclBin.h"
#include "ParameterSectionData.h"

#include <string>

using std::string;

TEST(MetaData, AddingMissingFile) {
  XclBin xclBin;
  const std::string formattedString = "BUILD_METADATA:junk.json:json";
  ParameterSectionData psd(formattedString);
  xclBin.addSection(psd);
}

TEST(MetaData, AddingValidFile) {
  XclBin xclBin;
  const std::string formattedString = "BUILD_METADATA:unittests/test_data/metadata.json:json";
  ParameterSectionData psd(formattedString);
  xclBin.addSection(psd);
}


