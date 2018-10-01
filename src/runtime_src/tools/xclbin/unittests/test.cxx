#include <gtest/gtest.h>

#include <string>
using std::string;

const char *foundExpected   = "expected";
const char *unexpected      = "unexpected";
const char *expected        = "expected";

TEST(ExampleTest, ExpectedEqualPASSES) {
    EXPECT_STREQ(expected, foundExpected);
}

TEST(ExampleTest, ExpectedNotEqualFAILS) {
    EXPECT_STREQ(expected, unexpected);
}
