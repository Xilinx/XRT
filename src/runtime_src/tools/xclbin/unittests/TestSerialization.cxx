#include <gtest/gtest.h>
#include "ParameterSectionData.h"
#include "XclBinClass.h"

TEST(Serialization, ReadXclbin_2018_2) {
   XclBin xclBin;
  
   xclBin.readXclBinBinary("unittests/test_data/sample_1_2018.2.xclbin", false /* bMigrateForward */);
}

TEST(Serialization, ReadWriteReadXclbin) {
   XclBin xclBin;
  
   xclBin.readXclBinBinary("unittests/test_data/sample_1_2018.2.xclbin", false /* bMigrateForward */);
   xclBin.writeXclBinBinary("unittests/ReadWriteReadXclbin.xclbin", true /* Skip UUID insertion */);

   XclBin xclBin2;
   xclBin2.readXclBinBinary("unittests/ReadWriteReadXclbin.xclbin", false /* bMigrateForward */);
}
