#include <cstdlib>
#include "gtest/gtest.h"

extern "C" void app_main(void)
{
    testing::InitGoogleTest();
    int result = RUN_ALL_TESTS();
    exit(result);
}
