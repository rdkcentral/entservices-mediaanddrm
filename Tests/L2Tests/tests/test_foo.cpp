#include <gtest/gtest.h>
#include <iostream>


class PrintTest : public ::testing::Test {
};

// Single test with print statement
TEST_F(PrintTest, BasicOutput) {
    std::cout << "this is a print statement" << std::endl;
}