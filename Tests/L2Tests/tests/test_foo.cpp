#include <gtest/gtest.h>
#include <iostream>


class PrintTestMD : public ::testing::Test {
};

// Single test with print statement
TEST_F(PrintTestMD, BasicOutputMD) {
    std::cout << "this is a print statement" << std::endl;
}