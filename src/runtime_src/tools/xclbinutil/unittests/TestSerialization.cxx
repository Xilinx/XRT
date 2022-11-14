#include <gtest/gtest.h>
#include "ParameterSectionData.h"
#include "XclBinClass.h"

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

TEST(Serialization, ReadXclbin_2018_2) {
   XclBin xclBin;
  
   // Get the file of interest
   fs::path sampleXclbin(TestUtilities::getResourceDir());
   sampleXclbin /= "sample_1_2018.2.xclbin";

   xclBin.readXclBinBinary(sampleXclbin.string(), false /* bMigrateForward */);
}

TEST(Serialization, ReadWriteReadXclbin) {
   XclBin xclBin;
  
   // Get the file of interest
   fs::path sampleXclbin(TestUtilities::getResourceDir());
   sampleXclbin /= ("sample_1_2018.2.xclbin");
  
   xclBin.readXclBinBinary(sampleXclbin.string(), false /* bMigrateForward */);


   xclBin.writeXclBinBinary("ReadWriteReadXclbin.xclbin", true /* Skip UUID insertion */);

   XclBin xclBin2;
   xclBin2.readXclBinBinary("ReadWriteReadXclbin.xclbin", false /* bMigrateForward */);
}
