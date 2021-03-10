#include <gtest/gtest.h>
#include "ParameterSectionData.h"

TEST(ParameterSectionData, ValidTuple) {
   const std::string sOption = "BUILD_METADATA:JSON:myfile.json";

   ParameterSectionData *pPSD = new ParameterSectionData(sOption);

   EXPECT_STREQ("BUILD_METADATA", pPSD->getSectionName().c_str());
   EXPECT_EQ(Section::FT_JSON, pPSD->getFormatType());
   EXPECT_STREQ("myfile.json", pPSD->getFile().c_str());
}

TEST(ParameterSectionData, FileColon) {
   const std::string sOption = "BUILD_METADATA:JSON:C:\\file.json";

   ParameterSectionData *pPSD = new ParameterSectionData(sOption);

   EXPECT_STREQ("BUILD_METADATA", pPSD->getSectionName().c_str());
   EXPECT_EQ(Section::FT_JSON, pPSD->getFormatType());
   EXPECT_STREQ("C:\\file.json", pPSD->getFile().c_str());
}

TEST(ParameterSectionData, EmptySectionWithJSON) {
   const std::string sOption = ":json:myfile.json";

   ParameterSectionData *pPSD = new ParameterSectionData(sOption);

   EXPECT_STREQ("", pPSD->getSectionName().c_str());
   EXPECT_EQ(Section::FT_JSON, pPSD->getFormatType());
   EXPECT_STREQ("myfile.json", pPSD->getFile().c_str());
}

TEST(ParameterSectionData, EmptySectionWithHTML) {
   const std::string sOption = ":html:myfile.json";

   ASSERT_THROW (new ParameterSectionData(sOption), std::runtime_error);
}



