#include <gtest/gtest.h>
#include <iostream>

class PrintTestMD : public ::testing::Test {
};

// Single test with print statement
TEST_F(PrintTestMD, BasicOutputMD) {
    std::cout << "this is a print statement from inputoutput" << std::endl;
}

// TEST_F(PrintTestIO, BasicoutputIOExt) {
//     int service =10;
//     ASSERT (service != 10);

//     std::string name = "Test";

//    // EXPECT_EQ( 0, name == "Test");
//    //values=10;
//     std::cout << "this is a second print statement from inputoutput" << std::endl;
// }



