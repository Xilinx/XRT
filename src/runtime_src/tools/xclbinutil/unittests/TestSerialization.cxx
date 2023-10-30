#include <gtest/gtest.h>
#include "ParameterSectionData.h"
#include "XclBinClass.h"

#include <filesystem>
#include "globals.h"

TEST(Serialization, ReadXclbin_2018_2) {
   XclBin xclBin;
  
   // Get the file of interest
   std::filesystem::path sampleXclbin(TestUtilities::getResourceDir());
   sampleXclbin /= "sample_1_2018.2.xclbin";

   xclBin.readXclBinBinary(sampleXclbin.string(), false /* bMigrateForward */);
}

TEST(Serialization, ReadWriteReadXclbin) {
   XclBin xclBin;
  
   // Get the file of interest
   std::filesystem::path sampleXclbin(TestUtilities::getResourceDir());
   sampleXclbin /= ("sample_1_2018.2.xclbin");
  
   xclBin.readXclBinBinary(sampleXclbin.string(), false /* bMigrateForward */);


   xclBin.writeXclBinBinary("ReadWriteReadXclbin.xclbin", true /* Skip UUID insertion */);

   XclBin xclBin2;
   xclBin2.readXclBinBinary("ReadWriteReadXclbin.xclbin", false /* bMigrateForward */);
}
