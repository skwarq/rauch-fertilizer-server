#include "xf1.hpp"

#include <gtest/gtest.h>

TEST(Xf1, RejectsBadLength)
{
    rauch::Xf1Frame frame{};
    EXPECT_THROW(rauch::validate_and_decode(frame), std::runtime_error);
}

TEST(Xf1, SafeFilenameRemovesPathCharacters)
{
    rauch::Xf1Record record{ "RAUCH", "A/B test" };
    EXPECT_EQ(rauch::safe_filename(record), "A_B_test.xf1");
}

TEST(Xf1, SafeFilenameHandlesWindowsReservedNamesAndInvalidCharacters)
{
    EXPECT_EQ(rauch::safe_filename({ "RAUCH", "CON" }), "_CON.xf1");
    EXPECT_EQ(rauch::safe_filename({ "RAUCH", "fertilizer:one. " }), "fertilizer_one.xf1");
}

TEST(Xf1, RejectsLengthWithHighBitSetWithoutSignedShift)
{
    rauch::Xf1Frame frame{};
    frame[3] = 0x80;
    EXPECT_THROW(rauch::validate_and_decode(frame), std::runtime_error);
}
