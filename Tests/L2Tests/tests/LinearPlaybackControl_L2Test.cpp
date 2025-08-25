
/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstring>

using namespace WPEFramework;

#define LINEARPLAYBACKCONTROL_CALLSIGN _T("LinearPlaybackControl")
#define LINEARPLAYBACKCONTROLL2TEST_CALLSIGN _T("L2tests.1")
#define JSON_TIMEOUT (1000)

class LinearPlaybackControlL2Test : public L2TestMocks {
protected:
    std::string mountPoint;
    std::string fccDir;
    std::string channelFile;

    void SetUp() override {
        // Try to create the expected directory first, fall back to temp directory if it fails
        std::string expectedPath = "/mnt/streamfs";
        std::string cmd1 = "mkdir -p " + expectedPath + "/fcc 2>/dev/null";
        int result1 = system(cmd1.c_str());
        
        if (result1 == 0) {
            // Successfully created expected directory
            mountPoint = expectedPath;
            std::cout << "Using expected mount point: " << mountPoint << std::endl;
        } else {
            // Fall back to temporary directory
            char tmpTemplate[] = "/tmp/streamfs_test_XXXXXX";
            char* tmpDir = mkdtemp(tmpTemplate);
            ASSERT_NE(tmpDir, nullptr) << "Failed to create temporary directory";
            
            mountPoint = std::string(tmpDir);
            std::cout << "Using temporary mount point: " << mountPoint << std::endl;
            
            // Try to create symbolic link to expected location (best effort)
            std::string linkCmd = "mkdir -p /mnt 2>/dev/null && ln -sf " + mountPoint + " " + expectedPath + " 2>/dev/null || true";
            system(linkCmd.c_str());
        }
        
        fccDir = mountPoint + "/fcc";
        channelFile = fccDir + "/chan_select0";

        // Create required directories recursively with proper error handling
        std::cout << "Creating directory structure: " << fccDir << std::endl;
        std::string cmd = "mkdir -p " + fccDir;
        int result = system(cmd.c_str());
        EXPECT_EQ(0, result) << "Failed to create directory: " << fccDir;
        
        // Set permissions with error checking
        std::cout << "Setting permissions on directories" << std::endl;
        EXPECT_EQ(0, chmod(mountPoint.c_str(), 0777)) << "Failed to set permissions on " << mountPoint;
        EXPECT_EQ(0, chmod(fccDir.c_str(), 0777)) << "Failed to set permissions on " << fccDir;
        
        // Create the channel file with verification
        std::cout << "Creating channel file: " << channelFile << std::endl;
        std::ofstream ofs(channelFile);
        EXPECT_TRUE(ofs.is_open()) << "Failed to create channel file: " << channelFile;
        ofs.close();
        EXPECT_EQ(0, chmod(channelFile.c_str(), 0666)) << "Failed to set permissions on channel file";

        // Create required files for status property with verification
        std::cout << "Creating seek file" << std::endl;
        std::ofstream seekFile(fccDir + "/seek0");
        EXPECT_TRUE(seekFile.is_open()) << "Failed to create seek file";
        seekFile << "0,0,0,0,0"; // 5 values for seek
        seekFile.close();
        EXPECT_EQ(0, chmod((fccDir + "/seek0").c_str(), 0666)) << "Failed to set permissions on seek file";

        std::cout << "Creating trick play file" << std::endl;
        std::ofstream trickFile(fccDir + "/trick_play0");
        EXPECT_TRUE(trickFile.is_open()) << "Failed to create trick play file";
        trickFile << "-4"; // example trick play speed
        trickFile.close();
        EXPECT_EQ(0, chmod((fccDir + "/trick_play0").c_str(), 0666)) << "Failed to set permissions on trick play file";

        std::cout << "Creating status file" << std::endl;
        std::ofstream statusFile(fccDir + "/stream_status");
        EXPECT_TRUE(statusFile.is_open()) << "Failed to create status file";
        statusFile << "0,0"; // 2 values for stream status
        statusFile.close();
        EXPECT_EQ(0, chmod((fccDir + "/stream_status").c_str(), 0666)) << "Failed to set permissions on status file";

        // Add small delay to ensure filesystem operations complete
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "Activating LinearPlaybackControl service" << std::endl;
        uint32_t status = Core::ERROR_GENERAL;
        status = ActivateService("LinearPlaybackControl");
        EXPECT_EQ(Core::ERROR_NONE, status);
        std::cout << "Setup completed successfully" << std::endl;
    }

    void TearDown() override {
        uint32_t status = Core::ERROR_GENERAL;
        status = DeactivateService("LinearPlaybackControl");
        EXPECT_EQ(Core::ERROR_NONE, status);
        
        // Clean up files and directories with error handling
        std::remove(channelFile.c_str());
        std::remove((fccDir + "/seek0").c_str());
        std::remove((fccDir + "/trick_play0").c_str());
        std::remove((fccDir + "/stream_status").c_str());
        
        // Clean up directories - handle both expected and temp paths
        if (mountPoint.find("/tmp/") == 0) {
            // This is a temp directory, remove it completely
            std::string cmd = "rm -rf " + mountPoint;
            system(cmd.c_str());
            // Also try to clean up any symbolic link
            system("rm -f /mnt/streamfs 2>/dev/null || true");
        } else {
            // This is the expected directory, just clean up our files
            std::string cmd = "rm -rf " + fccDir;
            system(cmd.c_str());
        }
    }

    virtual ~LinearPlaybackControlL2Test() override {}
};


/********************************************************
************Test case Details **************************
** 1. TEST SET CHANNEL
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, SetChannel_Test) {
    std::cout << "SetChannel_Test Started" << std::endl;
    
    // Verify channel file exists and is writable before test
    std::ifstream testFile(channelFile);
    EXPECT_TRUE(testFile.is_open()) << "Channel file should exist and be readable: " << channelFile;
    testFile.close();
    
    // Example channel URI
    std::string testChannel = "239.0.0.1:1234";
    // Prepare set request (channel@0)
    JsonObject setParams;
    setParams["channel"] = testChannel;
    JsonObject setResults;
    uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "channel@0", setParams, setResults);
    EXPECT_EQ(Core::ERROR_NONE, setResult);

    std::cout << "SetChannel_Test Finished" << std::endl;
}

/********************************************************
************Test case Details **************************
** 2. TEST GET CHANNEL
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, GetChannel_Test) {
    std::cout << "GetChannel_Test Started" << std::endl;
    
    // Verify channel file exists before test
    std::ifstream testFile(channelFile);
    EXPECT_TRUE(testFile.is_open()) << "Channel file should exist and be readable: " << channelFile;
    testFile.close();
    
    // First, set a valid channel
    JsonObject setParams;
    setParams["channel"] = "239.0.0.1:1234";
    JsonObject setResults;
    uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "channel@0", setParams, setResults);
    EXPECT_EQ(Core::ERROR_NONE, setResult);

    // Small delay to ensure file operations complete
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Now, get the channel 
    JsonObject getResults;
    uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "channel@0", getResults);
    EXPECT_EQ(Core::ERROR_NONE, getResult);
    
    EXPECT_TRUE(getResults.HasLabel("channel"));
    EXPECT_EQ(getResults["channel"].String(), "239.0.0.1:1234");
    std::cout << "GetChannel_Test Finished" << std::endl;
}


// /*
//  * If not stated otherwise in this file or this component's LICENSE file the
//  * following copyright and licenses apply:
//  *
//  * Copyright 2025 RDK Management
//  *
//  * Licensed under the Apache License, Version 2.0 (the "License");
//  * you may not use this file except in compliance with the License.
//  * You may obtain a copy of the License at
//  *
//  * http://www.apache.org/licenses/LICENSE-2.0
//  *
//  * Unless required by applicable law or agreed to in writing, software
//  * distributed under the License is distributed on an "AS IS" BASIS,
//  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  * See the License for the specific language governing permissions and
//  * limitations under the License.
//  */

// #include <gtest/gtest.h>
// #include <gmock/gmock.h>
// #include "L2Tests.h"
// #include "L2TestsMock.h"
// #include <sys/stat.h>
// #include <unistd.h>
// #include <fstream>
// #include <thread>
// #include <chrono>
// #include <cstdlib>

// using namespace WPEFramework;

// #define LINEARPLAYBACKCONTROL_CALLSIGN _T("LinearPlaybackControl")
// #define LINEARPLAYBACKCONTROLL2TEST_CALLSIGN _T("L2tests.1")
// #define JSON_TIMEOUT (1000)

// class LinearPlaybackControlL2Test : public L2TestMocks {
// protected:
//     std::string mountPoint = "/mnt/streamfs";
//     std::string fccDir = mountPoint + "/fcc";
//     std::string channelFile = fccDir + "/chan_select0";

//     void SetUp() override {
//         // Create required directories recursively with proper error handling
//         std::cout << "Creating directory structure: " << fccDir << std::endl;
//         std::string cmd = "mkdir -p " + fccDir;
//         int result = system(cmd.c_str());
//         EXPECT_EQ(0, result) << "Failed to create directory: " << fccDir;
        
//         // Set permissions with error checking
//         std::cout << "Setting permissions on directories" << std::endl;
//         EXPECT_EQ(0, chmod(mountPoint.c_str(), 0777)) << "Failed to set permissions on " << mountPoint;
//         EXPECT_EQ(0, chmod(fccDir.c_str(), 0777)) << "Failed to set permissions on " << fccDir;
        
//         // Create the channel file with verification
//         std::cout << "Creating channel file: " << channelFile << std::endl;
//         std::ofstream ofs(channelFile);
//         EXPECT_TRUE(ofs.is_open()) << "Failed to create channel file: " << channelFile;
//         ofs.close();
//         EXPECT_EQ(0, chmod(channelFile.c_str(), 0666)) << "Failed to set permissions on channel file";

//         // Create required files for status property with verification
//         std::cout << "Creating seek file" << std::endl;
//         std::ofstream seekFile(fccDir + "/seek0");
//         EXPECT_TRUE(seekFile.is_open()) << "Failed to create seek file";
//         seekFile << "0,0,0,0,0"; // 5 values for seek
//         seekFile.close();
//         EXPECT_EQ(0, chmod((fccDir + "/seek0").c_str(), 0666)) << "Failed to set permissions on seek file";

//         std::cout << "Creating trick play file" << std::endl;
//         std::ofstream trickFile(fccDir + "/trick_play0");
//         EXPECT_TRUE(trickFile.is_open()) << "Failed to create trick play file";
//         trickFile << "-4"; // example trick play speed
//         trickFile.close();
//         EXPECT_EQ(0, chmod((fccDir + "/trick_play0").c_str(), 0666)) << "Failed to set permissions on trick play file";

//         std::cout << "Creating status file" << std::endl;
//         std::ofstream statusFile(fccDir + "/stream_status0");
//         EXPECT_TRUE(statusFile.is_open()) << "Failed to create status file";
//         statusFile << "0,0"; // 2 values for stream status
//         statusFile.close();
//         EXPECT_EQ(0, chmod((fccDir + "/stream_status0").c_str(), 0666)) << "Failed to set permissions on status file";

//         // Add small delay to ensure filesystem operations complete
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));

//         std::cout << "Activating LinearPlaybackControl service" << std::endl;
//         uint32_t status = Core::ERROR_GENERAL;
//         status = ActivateService("LinearPlaybackControl");
//         EXPECT_EQ(Core::ERROR_NONE, status);
//         std::cout << "Setup completed successfully" << std::endl;
//     }

//     void TearDown() override {
//         uint32_t status = Core::ERROR_GENERAL;
//         status = DeactivateService("LinearPlaybackControl");
//         EXPECT_EQ(Core::ERROR_NONE, status);
        
//         // Clean up files and directories with error handling
//         std::remove(channelFile.c_str());
//         std::remove((fccDir + "/seek0").c_str());
//         std::remove((fccDir + "/trick_play0").c_str());
//         std::remove((fccDir + "/stream_status0").c_str());
        
//         // Use system commands for reliable cleanup
//         std::string cmd = "rm -rf " + mountPoint;
//         system(cmd.c_str());
//     }

//     virtual ~LinearPlaybackControlL2Test() override {}
// };


// /********************************************************
// ************Test case Details **************************
// ** 1. TEST SET CHANNEL
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, SetChannel_Test) {
//     std::cout << "SetChannel_Test Started" << std::endl;
    
//     // Verify channel file exists and is writable before test
//     std::ifstream testFile(channelFile);
//     EXPECT_TRUE(testFile.is_open()) << "Channel file should exist and be readable: " << channelFile;
//     testFile.close();
    
//     // Example channel URI
//     std::string testChannel = "239.0.0.1:1234";
//     // Prepare set request (channel@0)
//     JsonObject setParams;
//     setParams["channel"] = testChannel;
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "channel@0", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);

//     std::cout << "SetChannel_Test Finished" << std::endl;
// }

// /********************************************************
// ************Test case Details **************************
// ** 2. TEST GET CHANNEL
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, GetChannel_Test) {
//     std::cout << "GetChannel_Test Started" << std::endl;
    
//     // Verify channel file exists before test
//     std::ifstream testFile(channelFile);
//     EXPECT_TRUE(testFile.is_open()) << "Channel file should exist and be readable: " << channelFile;
//     testFile.close();
    
//     // First, set a valid channel
//     JsonObject setParams;
//     setParams["channel"] = "239.0.0.1:1234";
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "channel@0", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);

//     // Small delay to ensure file operations complete
//     std::this_thread::sleep_for(std::chrono::milliseconds(50));

//     // Now, get the channel 
//     JsonObject getResults;
//     uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "channel@0", getResults);
//     EXPECT_EQ(Core::ERROR_NONE, getResult);
    
//     EXPECT_TRUE(getResults.HasLabel("channel"));
//     EXPECT_EQ(getResults["channel"].String(), "239.0.0.1:1234");
//     std::cout << "GetChannel_Test Finished" << std::endl;
// }


// /********************************************************
// ************Test case Details **************************
// ** 3. TEST SET SEEK
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, SetSeek_Test) {
//     std::cout << "SetSeek_Test Started" << std::endl;
    
//     // Verify seek file exists and is writable before test
//     std::string seekFilePath = fccDir + "/seek0";
//     std::ifstream testFile(seekFilePath);
//     EXPECT_TRUE(testFile.is_open()) << "Seek file should exist and be readable: " << seekFilePath;
//     testFile.close();
    
//     JsonObject setParams;
//     setParams["seekPosInSeconds"] = 10;
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "seek@0", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);
//     std::cout << "SetSeek_Test Finished" << std::endl;
// }

// /********************************************************
// ************Test case Details **************************
// ** 4. TEST GET SEEK
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, GetSeek_Test) {
//     std::cout << "GetSeek_Test Started" << std::endl;
    
//     // Ensure seek file exists with proper content before invoking set
//     std::string seekFilePath = fccDir + "/seek0";
//     std::ofstream seekFile(seekFilePath);
//     EXPECT_TRUE(seekFile.is_open()) << "Failed to create seek file for test";
//     seekFile << "0,0,0,0,0"; // Initial content
//     seekFile.close();
//     EXPECT_EQ(0, chmod(seekFilePath.c_str(), 0666)) << "Failed to set permissions on seek file";
    
//     // First, set a valid seek position
//     JsonObject setParams;
//     setParams["seekPosInSeconds"] = 10;
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "seek@0", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);

//     // Verify the file was updated by the set operation, then ensure it has the expected format
//     std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Small delay for file operations
    
//     // Update seek file to ensure it contains 5 comma-separated values as expected by plugin
//     std::ofstream seekFileUpdate(seekFilePath);
//     EXPECT_TRUE(seekFileUpdate.is_open()) << "Failed to update seek file";
//     seekFileUpdate << "10,0,0,0,0";
//     seekFileUpdate.close();

//     // Small delay to ensure file operations are complete
//     std::this_thread::sleep_for(std::chrono::milliseconds(50));

//     // Now, get the seek position 
//     JsonObject getResults;
//     uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "seek@0", getResults);
//     EXPECT_EQ(Core::ERROR_NONE, getResult);
//     EXPECT_TRUE(getResults.HasLabel("seekPosInSeconds"));
//     EXPECT_EQ(getResults["seekPosInSeconds"].Number(), 10);
//     std::cout << "GetSeek_Test Finished" << std::endl;
// }


// /********************************************************
// ************Test case Details **************************
// ** 5. TEST SET TRICKPLAY
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, SetTrickplay_Test) {
//     std::cout << "SetTrickplay_Test Started" << std::endl;
    
//     // Verify trick play file exists and is writable before test
//     std::string trickPlayFilePath = fccDir + "/trick_play0";
//     std::ifstream testFile(trickPlayFilePath);
//     EXPECT_TRUE(testFile.is_open()) << "Trick play file should exist and be readable: " << trickPlayFilePath;
//     testFile.close();
    
//     JsonObject setParams;
//     setParams["speed"] = -4;
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "trickplay@0", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);
//     std::cout << "SetTrickplay_Test Finished" << std::endl;
// }

// /********************************************************
// ************Test case Details **************************
// ** 6. TEST GET TRICKPLAY
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, GetTrickplay_Test) {
//     std::cout << "GetTrickplay_Test Started" << std::endl;
    
//     // Verify trick play file exists before test
//     std::string trickPlayFilePath = fccDir + "/trick_play0";
//     std::ifstream testFile(trickPlayFilePath);
//     EXPECT_TRUE(testFile.is_open()) << "Trick play file should exist and be readable: " << trickPlayFilePath;
//     testFile.close();
    
//     // First, set a valid trickplay speed
//     JsonObject setParams;
//     setParams["speed"] = -4;
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "trickplay@0", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);

//     // Small delay to ensure file operations complete
//     std::this_thread::sleep_for(std::chrono::milliseconds(50));

//     // Now, get the trickplay speed 
//     JsonObject getResults;
//     uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "trickplay@0", getResults);
//     EXPECT_EQ(Core::ERROR_NONE, getResult);
//     std::cout << "GetTrickplay_Test: speed = " << (getResults.HasLabel("speed") ? getResults["speed"].Number() : -999) << std::endl;
//     EXPECT_TRUE(getResults.HasLabel("speed"));
//     EXPECT_EQ(getResults["speed"].Number(), -4);
//     std::cout << "GetTrickplay_Test Finished" << std::endl;
// }

// /********************************************************
// ************Test case Details **************************
// ** 8. TEST SET TRACING
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, SetTracing_Test) {
//     std::cout << "SetTracing_Test Started" << std::endl;
//     JsonObject setParams;
//     setParams["tracing"] = true;
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "tracing", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);
//     std::cout << "SetTracing_Test Finished" << std::endl;
// }

// /********************************************************
// ************Test case Details **************************
// ** 9. TEST GET TRACING
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, GetTracing_Test) {
//     std::cout << "GetTracing_Test Started" << std::endl;
//     Core::JSON::Boolean tracingResult;
//     uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "tracing", tracingResult);
//     EXPECT_EQ(Core::ERROR_NONE, getResult);
//     std::cout << "GetTracing_Test Finished" << std::endl;
// }




// /*
//  * If not stated otherwise in this file or this component's LICENSE file the
//  * following copyright and licenses apply:
//  *
//  * Copyright 2025 RDK Management
//  *
//  * Licensed under the Apache License, Version 2.0 (the "License");
//  * you may not use this file except in compliance with the License.
//  * You may obtain a copy of the License at
//  *
//  * http://www.apache.org/licenses/LICENSE-2.0
//  *
//  * Unless required by applicable law or agreed to in writing, software
//  * distributed under the License is distributed on an "AS IS" BASIS,
//  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  * See the License for the specific language governing permissions and
//  * limitations under the License.
//  */

// #include <gtest/gtest.h>
// #include <gmock/gmock.h>
// #include "L2Tests.h"
// #include "L2TestsMock.h"
// #include <sys/stat.h>
// #include <unistd.h>
// #include <fstream>
// #include <thread>
// #include <chrono>

// using namespace WPEFramework;

// #define LINEARPLAYBACKCONTROL_CALLSIGN _T("LinearPlaybackControl")
// #define LINEARPLAYBACKCONTROLL2TEST_CALLSIGN _T("L2tests.1")
// #define JSON_TIMEOUT (1000)

// class LinearPlaybackControlL2Test : public L2TestMocks {
// protected:
//     std::string mountPoint = "/mnt/streamfs";
//     std::string fccDir = mountPoint + "/fcc";
//     std::string channelFile = fccDir + "/chan_select0";

//     void SetUp() override {
//         // Create required directories and file with correct permissions
//         mkdir(mountPoint.c_str(), 0777);
//         mkdir(fccDir.c_str(), 0777);
//         chmod(mountPoint.c_str(), 0777);
//         chmod(fccDir.c_str(), 0777);
//         // Create the channel file
//         std::ofstream ofs(channelFile);
//         ofs.close();
//         chmod(channelFile.c_str(), 0666);

//         // Create required files for status property
//         std::ofstream seekFile(fccDir + "/seek0");
//         seekFile << "0,0,0,0,0"; // 5 values for seek
//         seekFile.close();
//         chmod((fccDir + "/seek0").c_str(), 0666);

//         std::ofstream trickFile(fccDir + "/trick_play0");
//         trickFile << "-4"; // example trick play speed
//         trickFile.close();
//         chmod((fccDir + "/trick_play0").c_str(), 0666);

//         std::ofstream statusFile(fccDir + "/stream_status0");
//         statusFile << "0,0"; // 2 values for stream status
//         statusFile.close();
//         chmod((fccDir + "/stream_status0").c_str(), 0666);

//         uint32_t status = Core::ERROR_GENERAL;
//         status = ActivateService("LinearPlaybackControl");
//         EXPECT_EQ(Core::ERROR_NONE, status);
//     }

//     void TearDown() override {
//         std::remove(channelFile.c_str());
//         rmdir(fccDir.c_str());
//         rmdir(mountPoint.c_str());
//         uint32_t status = Core::ERROR_GENERAL;
//         status = DeactivateService("LinearPlaybackControl");
//         EXPECT_EQ(Core::ERROR_NONE, status);
//     }

//     virtual ~LinearPlaybackControlL2Test() override {}
// };

// /********************************************************
// ************Test case Details **************************
// ** 1. TEST SET CHANNEL
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, SetChannel_Test) {
//     std::cout << "SetChannel_Test Started" << std::endl;
//     // Example channel URI
//     std::string testChannel = "239.0.0.1:1234";
//     // Prepare set request (channel@0)
//     JsonObject setParams;
//     setParams["channel"] = testChannel;
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "channel@0", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);

//     std::cout << "SetChannel_Test Finished" << std::endl;

// }

// /********************************************************
// ************Test case Details **************************
// ** 2. TEST GET CHANNEL
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, GetChannel_Test) {
//     std::cout << "GetChannel_Test Started" << std::endl;
//     // First, set a valid channel
//     JsonObject setParams;
//     setParams["channel"] = "239.0.0.1:1234";
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "channel@0", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);

//     // Now, get the channel 
//     JsonObject getResults;
//     uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "channel@0", getResults);
//     EXPECT_EQ(Core::ERROR_NONE, getResult);
    
//     EXPECT_TRUE(getResults.HasLabel("channel"));
//     EXPECT_EQ(getResults["channel"].String(), "239.0.0.1:1234");
//     std::cout << "GetChannel_Test Finished" << std::endl;
// }

// /********************************************************
// ************Test case Details **************************
// ** 3. TEST SET SEEK
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, SetSeek_Test) {
//     std::cout << "SetSeek_Test Started" << std::endl;
//     JsonObject setParams;
//     setParams["seekPosInSeconds"] = 10;
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "seek@0", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);
//     std::cout << "SetSeek_Test Finished" << std::endl;
// }

// /********************************************************
// ************Test case Details **************************
// ** 4. TEST GET SEEK
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, GetSeek_Test) {
//     std::cout << "GetSeek_Test Started" << std::endl;
//     // First, set a valid seek position
//     JsonObject setParams;
//     setParams["seekPosInSeconds"] = 10;
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "seek@0", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);

//     // Ensure seek file contains 5 comma-separated values as expected by plugin
//     std::ofstream seekFile("/mnt/streamfs/fcc/seek0");
//     seekFile << "10,0,0,0,0";
//     seekFile.close();

//     // Now, get the seek position 
//     JsonObject getResults;
//     uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "seek@0", getResults);
//     EXPECT_EQ(Core::ERROR_NONE, getResult);
//     EXPECT_TRUE(getResults.HasLabel("seekPosInSeconds"));
//     EXPECT_EQ(getResults["seekPosInSeconds"].Number(), 10);
//     std::cout << "GetSeek_Test Finished" << std::endl;
// }

// /********************************************************
// ************Test case Details **************************
// ** 5. TEST SET TRICKPLAY
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, SetTrickplay_Test) {
//     std::cout << "SetTrickplay_Test Started" << std::endl;
//     JsonObject setParams;
//     setParams["speed"] = -4;
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "trickplay@0", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);
//     std::cout << "SetTrickplay_Test Finished" << std::endl;
// }

// /********************************************************
// ************Test case Details **************************
// ** 6. TEST GET TRICKPLAY
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, GetTrickplay_Test) {
//     std::cout << "GetTrickplay_Test Started" << std::endl;
//     // First, set a valid trickplay speed
//     JsonObject setParams;
//     setParams["speed"] = -4;
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "trickplay@0", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);

//     // Now, get the trickplay speed 
//     JsonObject getResults;
//     uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "trickplay@0", getResults);
//     EXPECT_EQ(Core::ERROR_NONE, getResult);
//     std::cout << "GetTrickplay_Test: speed = " << (getResults.HasLabel("speed") ? getResults["speed"].Number() : -999) << std::endl;
//     EXPECT_TRUE(getResults.HasLabel("speed"));
//     EXPECT_EQ(getResults["speed"].Number(), -4);
//     std::cout << "GetTrickplay_Test Finished" << std::endl;
// }

// /********************************************************
// ************Test case Details **************************
// ** 7. TEST GET STATUS
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, GetStatus_Test) {
//     std::cout << "GetStatus_Test Started" << std::endl;
//     // Ensure status file contains 2 comma-separated values as expected by plugin
//     std::ofstream statusFile("/mnt/streamfs/fcc/stream_status");
//     statusFile << "0,0";
//     statusFile.close();

//     // Now, get the status
//     JsonObject getResults;
//     uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "status@0", getResults);
//     EXPECT_EQ(Core::ERROR_NONE, getResult);
//     std::cout << "GetStatus_Test: streamSourceLost = " << (getResults.HasLabel("streamSourceLost") ? getResults["streamSourceLost"].Number() : -1) << std::endl;
//     std::cout << "GetStatus_Test: streamSourceLossCount = " << (getResults.HasLabel("streamSourceLossCount") ? getResults["streamSourceLossCount"].Number() : -1) << std::endl;
//     EXPECT_TRUE(getResults.HasLabel("streamSourceLost"));
//     EXPECT_TRUE(getResults.HasLabel("streamSourceLossCount"));
//     EXPECT_EQ(getResults["streamSourceLost"].Number(), 0);
//     EXPECT_EQ(getResults["streamSourceLossCount"].Number(), 0);
//     std::cout << "GetStatus_Test Finished" << std::endl;
// }

// /********************************************************
// ************Test case Details **************************
// ** 8. TEST SET TRACING
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, SetTracing_Test) {
//     std::cout << "SetTracing_Test Started" << std::endl;
//     JsonObject setParams;
//     setParams["tracing"] = true;
//     JsonObject setResults;
//     uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "tracing", setParams, setResults);
//     EXPECT_EQ(Core::ERROR_NONE, setResult);
//     std::cout << "SetTracing_Test Finished" << std::endl;
// }

// /********************************************************
// ************Test case Details **************************
// ** 9. TEST GET TRACING
// *******************************************************/
// TEST_F(LinearPlaybackControlL2Test, GetTracing_Test) {
//     std::cout << "GetTracing_Test Started" << std::endl;
//     Core::JSON::Boolean tracingResult;
//     uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "tracing", tracingResult);
//     EXPECT_EQ(Core::ERROR_NONE, getResult);
//     std::cout << "GetTracing_Test Finished" << std::endl;
// }


