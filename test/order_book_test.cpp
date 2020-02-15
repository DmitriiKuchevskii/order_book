#include <sstream>
#include <memory>
#include <charconv>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "OrderBook.h"

TEST(a, b)
{
    ASSERT_THAT(split("1,2,3,4,5,6,7,8"), testing::ElementsAre("1","2","3","4","5","6","7","8"));
}