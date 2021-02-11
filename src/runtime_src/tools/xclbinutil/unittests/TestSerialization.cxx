#include <gtest/gtest.h>
#include "ParameterSectionData.h"
#include "XclBinClass.h"

#include "globals.h"
#include <boost/filesystem.hpp>

TEST(Serialization, ReadXclbin_2018_2) {
   XclBin xclBin;
  
   // Get the file of interest
   boost::filesystem::path sampleXclbin(TestUtilities::getResourceDir());
   sampleXclbin /= "sample_1_2018.2.xclbin";

   xclBin.readXclBinBinary(sampleXclbin.string(), false /* bMigrateForward */);
}

TEST(Serialization, ReadWriteReadXclbin) {
   XclBin xclBin;
  
   // Get the file of interest
   boost::filesystem::path sampleXclbin(TestUtilities::getResourceDir());
   sampleXclbin /= ("sample_1_2018.2.xclbin");
  
   xclBin.readXclBinBinary(sampleXclbin.string(), false /* bMigrateForward */);


   xclBin.writeXclBinBinary("ReadWriteReadXclbin.xclbin", true /* Skip UUID insertion */);

   XclBin xclBin2;
   xclBin2.readXclBinBinary("ReadWriteReadXclbin.xclbin", false /* bMigrateForward */);
}
