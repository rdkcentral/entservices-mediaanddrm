
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
#include <errno.h>

using namespace WPEFramework;

#define LINEARPLAYBACKCONTROL_CALLSIGN _T("LinearPlaybackControl")
#define LINEARPLAYBACKCONTROLL2TEST_CALLSIGN _T("L2tests.1")
#define JSON_TIMEOUT (1000)

class LinearPlaybackControlL2Test : public L2TestMocks {
protected:
    std::string mountPoint;
    std::string fccDir;
    std::string channelFile;
    bool usingExpectedLocation;
    
    // Helper function to update files in both locations
    void updateFileInBothLocations(const std::string& relativePath, const std::string& content) {
        auto writeAndSync = [](const std::string& path, const std::string& data) -> bool {
            std::ofstream file(path);
            if (file.is_open()) {
                file << data;
                file.flush();  // Ensure data is written to OS buffer
                file.close();
                // Sync to ensure data is flushed to disk
                sync();
                return true;
            }
            return false;
        };
        
        // Update in our test directory
        std::string testFilePath = fccDir + "/" + relativePath;
        writeAndSync(testFilePath, content);
        
        // Also update in expected location if we're not using it as primary
        if (!usingExpectedLocation) {
            std::string expectedFilePath = "/mnt/streamfs/fcc/" + relativePath;
            writeAndSync(expectedFilePath, content);
        }
    }

    void SetUp() override {
        // Try to use the expected mount point that the plugin is configured for
        std::string expectedPath = "/mnt/streamfs";
        
        // Check if we can write to the expected location
        if (access("/mnt", F_OK) == 0 && access("/mnt", W_OK) == 0) {
            // We have access to /mnt, try to use expected path
            mountPoint = expectedPath;
            usingExpectedLocation = true;
            std::cout << "Using expected mount point: " << mountPoint << std::endl;
        } else {
            // Generate unique directory name for test isolation
            auto now = std::chrono::high_resolution_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
            std::string uniqueId = std::to_string(timestamp);
            
            // Fall back to local test directory
            char* cwd = getcwd(nullptr, 0);
            if (cwd) {
                mountPoint = std::string(cwd) + "/test_streamfs_" + uniqueId;
                free(cwd);
            } else {
                mountPoint = "./test_streamfs_" + uniqueId;
            }
            
            usingExpectedLocation = false;
            std::cout << "Using test mount point: " << mountPoint << std::endl;
            
            // Try to create symbolic link to expected location for plugin compatibility
            if (access("/mnt", F_OK) == 0) {
                // /mnt exists, try to create symlink even if we don't have write permissions
                std::string linkCmd = "sudo ln -sf " + mountPoint + " " + expectedPath + " 2>/dev/null || ln -sf " + mountPoint + " " + expectedPath + " 2>/dev/null";
                int linkResult = system(linkCmd.c_str());
                if (linkResult == 0) {
                    std::cout << "Created symbolic link from " + expectedPath + " to " + mountPoint << std::endl;
                } else {
                    std::cout << "Failed to create symbolic link, plugin may not find files" << std::endl;
                    // As a workaround, try to make the plugin use our directory by creating /mnt if it doesn't exist
                    system("mkdir -p /mnt 2>/dev/null || true");
                    system(("ln -sf " + mountPoint + " " + expectedPath + " 2>/dev/null || echo 'Link creation failed'").c_str());
                }
            } else {
                std::cout << "No access to /mnt directory for symbolic link creation" << std::endl;
            }
        }
        
        fccDir = mountPoint + "/fcc";
        channelFile = fccDir + "/chan_select0";

        // Create required directories recursively with better error handling
        std::cout << "Creating directory structure: " << fccDir << std::endl;
        std::string cmd = "mkdir -p " + fccDir;
        int result = system(cmd.c_str());
        if (result != 0) {
            // Try alternative approach if mkdir fails
            std::cout << "attempting to create directory manually" << std::endl;
            if (mkdir(mountPoint.c_str(), 0755) != 0 && errno != EEXIST) {
                std::cout << "Failed to create mount point directory" << std::endl;
            }
            if (mkdir(fccDir.c_str(), 0755) != 0 && errno != EEXIST) {
                std::cout << "Failed to create fcc directory" << std::endl;
            }
        }
        
        // Verify directories exist before proceeding
        struct stat st;
        if (stat(fccDir.c_str(), &st) != 0) {
            std::cout << "FCC directory does not exist: " << fccDir << std::endl;
            FAIL() << "Cannot create required test directory structure";
        }
        
        // Set permissions with error checking (but don't fail if it doesn't work)
        std::cout << "Setting permissions on directories" << std::endl;
        if (chmod(mountPoint.c_str(), 0777) != 0) {
            std::cout << "Could not set permissions on " << mountPoint << std::endl;
        }
        if (chmod(fccDir.c_str(), 0777) != 0) {
            std::cout << "Could not set permissions on " << fccDir << std::endl;
        }
        
        // Helper function to create and verify file
        auto createFileWithRetry = [this](const std::string& filePath, const std::string& content, int retries = 3) -> bool {
            for (int i = 0; i < retries; i++) {
                std::ofstream file(filePath);
                if (file.is_open()) {
                    file << content;
                    file.close();
                    
                    // Verify file was created successfully
                    std::ifstream verifyFile(filePath);
                    if (verifyFile.is_open()) {
                        verifyFile.close();
                        chmod(filePath.c_str(), 0666); // Try to set permissions, ignore failures
                        return true;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return false;
        };
        
        // Create the channel file
        std::cout << "Creating channel file: " << channelFile << std::endl;
        EXPECT_TRUE(createFileWithRetry(channelFile, "")) << "Failed to create channel file: " << channelFile;

        // Create required files for status property
        std::cout << "Creating seek file" << std::endl;
        EXPECT_TRUE(createFileWithRetry(fccDir + "/seek0", "0,0,0,0,0")) << "Failed to create seek file";

        std::cout << "Creating trick play file" << std::endl;
        EXPECT_TRUE(createFileWithRetry(fccDir + "/trick_play0", "-4")) << "Failed to create trick play file";

        std::cout << "Creating status file" << std::endl;
        EXPECT_TRUE(createFileWithRetry(fccDir + "/stream_status", "0,0")) << "Failed to create status file";

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::cout << "Activating LinearPlaybackControl service" << std::endl;
        uint32_t status = Core::ERROR_GENERAL;
        
        std::string expectedFccDir = "/mnt/streamfs/fcc";
        bool createdExpectedFiles = false;
        
        if (mountPoint != "/mnt/streamfs") {
            std::cout << "Creating backup files in expected location: " << expectedFccDir << std::endl;
            
            //create the expected directory structure
            if (system("mkdir -p /mnt/streamfs/fcc 2>/dev/null") == 0) {
                // Copy test files to the expected location
                auto copyFile = [](const std::string& src, const std::string& dst) -> bool {
                    std::ifstream source(src);
                    if (!source.is_open()) return false;
                    
                    std::ofstream dest(dst);
                    if (!dest.is_open()) return false;
                    
                    dest << source.rdbuf();
                    return source.good() && dest.good();
                };
                
                // Copy all test files to expected location
                if (copyFile(channelFile, expectedFccDir + "/chan_select0") &&
                    copyFile(fccDir + "/seek0", expectedFccDir + "/seek0") &&
                    copyFile(fccDir + "/trick_play0", expectedFccDir + "/trick_play0") &&
                    copyFile(fccDir + "/stream_status", expectedFccDir + "/stream_status")) {
                    
                    createdExpectedFiles = true;
                    std::cout << "Successfully copied test files to expected location" << std::endl;
                }
            }
        }
        
        status = ActivateService("LinearPlaybackControl");
        EXPECT_EQ(Core::ERROR_NONE, status);
        std::cout << "Setup completed successfully" << std::endl;
    }

    void TearDown() override {
        std::cout << "TearDown: Deactivating LinearPlaybackControl service" << std::endl;
        uint32_t status = Core::ERROR_GENERAL;
        status = DeactivateService("LinearPlaybackControl");
        EXPECT_EQ(Core::ERROR_NONE, status);
        
        // Clean up files
        std::cout << "TearDown: Cleaning up test files" << std::endl;
        std::remove(channelFile.c_str());
        std::remove((fccDir + "/seek0").c_str());
        std::remove((fccDir + "/trick_play0").c_str());
        std::remove((fccDir + "/stream_status").c_str());
        
        if (!usingExpectedLocation) {
            std::remove("/mnt/streamfs/fcc/chan_select0");
            std::remove("/mnt/streamfs/fcc/seek0");
            std::remove("/mnt/streamfs/fcc/trick_play0");
            std::remove("/mnt/streamfs/fcc/stream_status");
            
            system("rmdir /mnt/streamfs/fcc 2>/dev/null || true");
            system("rmdir /mnt/streamfs 2>/dev/null || true");
        }
        
        std::string cmd = "rm -rf " + mountPoint;
        system(cmd.c_str());
        std::cout << "TearDown: Removed test directory: " << mountPoint << std::endl;
        
        system("rm -f /mnt/streamfs 2>/dev/null || true");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    virtual ~LinearPlaybackControlL2Test() override {}
};


/********************************************************
************Test case Details **************************
** 1. TEST SET CHANNEL
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, SetChannel_Test) {
    std::cout << "SetChannel_Test Started" << std::endl;
    
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
    
    std::ifstream testFile(channelFile);
    EXPECT_TRUE(testFile.is_open()) << "Channel file should exist and be readable: " << channelFile;
    testFile.close();
    
    // First, set a valid channel
    JsonObject setParams;
    setParams["channel"] = "239.0.0.1:1234";
    JsonObject setResults;
    uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "channel@0", setParams, setResults);
    EXPECT_EQ(Core::ERROR_NONE, setResult);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    updateFileInBothLocations("chan_select0", "239.0.0.1:1234");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Now, get the channel 
    JsonObject getResults;
    uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "channel@0", getResults);
    EXPECT_EQ(Core::ERROR_NONE, getResult);
    
    if (getResult == Core::ERROR_NONE) {
        EXPECT_TRUE(getResults.HasLabel("channel"));
        if (getResults.HasLabel("channel")) {
            EXPECT_EQ(getResults["channel"].String(), "239.0.0.1:1234");
        }
    }
    
    std::cout << "GetChannel_Test Finished" << std::endl;
}

/********************************************************
************Test case Details **************************
** 3. TEST SET SEEK
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, SetSeek_Test) {
    std::cout << "SetSeek_Test Started" << std::endl;
    
    std::string seekFilePath = fccDir + "/seek0";
    std::ifstream testFile(seekFilePath);
    EXPECT_TRUE(testFile.is_open()) << "Seek file should exist and be readable: " << seekFilePath;
    testFile.close();
    
    JsonObject setParams;
    setParams["seekPosInSeconds"] = 10;
    JsonObject setResults;
    uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "seek@0", setParams, setResults);
    EXPECT_EQ(Core::ERROR_NONE, setResult);
    std::cout << "SetSeek_Test Finished" << std::endl;
}

/********************************************************
************Test case Details **************************
** 4. TEST GET SEEK
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, GetSeek_Test) {
    std::cout << "GetSeek_Test Started" << std::endl;
    
    std::string seekFilePath = fccDir + "/seek0";
    
    auto writeSeekFile = [&](const std::string& content) -> bool {
        updateFileInBothLocations("seek0", content);
        
        std::ifstream verify(seekFilePath);
        if (verify.is_open()) {
            std::string readContent;
            std::getline(verify, readContent);
            verify.close();
            
            if (readContent == content) {
                chmod(seekFilePath.c_str(), 0666); // Ignore chmod failures
                return true;
            }
        }
        return false;
    };
    
    EXPECT_TRUE(writeSeekFile("0,0,0,0,0")) << "Failed to create initial seek file for test";
    
    // First, set a valid seek position
    JsonObject setParams;
    setParams["seekPosInSeconds"] = 10;
    JsonObject setResults;
    uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "seek@0", setParams, setResults);
    EXPECT_EQ(Core::ERROR_NONE, setResult);

    // Wait for the SET operation to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_TRUE(writeSeekFile("10,0,0,0,0")) << "Failed to update seek file with expected format";

    // Wait for file to be fully written and synced
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Verify the file was written correctly before attempting GET
    {
        std::ifstream verifyFile(seekFilePath);
        if (verifyFile.is_open()) {
            std::string content;
            std::getline(verifyFile, content);
            verifyFile.close();
            EXPECT_EQ(content, "10,0,0,0,0") << "Seek file content mismatch before GET";
        } else {
            FAIL() << "Could not verify seek file before GET operation";
        }
    }

    // Now, get the seek position 
    JsonObject getResults;
    uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "seek@0", getResults);
    EXPECT_EQ(Core::ERROR_NONE, getResult);
    
    if (getResult == Core::ERROR_NONE) {
        EXPECT_TRUE(getResults.HasLabel("seekPosInSeconds"));
        if (getResults.HasLabel("seekPosInSeconds")) {
            EXPECT_EQ(getResults["seekPosInSeconds"].Number(), 10);
        }
    }
    
    std::cout << "GetSeek_Test Finished" << std::endl;
}

/********************************************************
************Test case Details **************************
** 5. TEST SET TRICKPLAY
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, SetTrickplay_Test) {
    std::cout << "SetTrickplay_Test Started" << std::endl;
    
    std::string trickPlayFilePath = fccDir + "/trick_play0";
    std::ifstream testFile(trickPlayFilePath);
    EXPECT_TRUE(testFile.is_open()) << "Trick play file should exist and be readable: " << trickPlayFilePath;
    testFile.close();
    
    JsonObject setParams;
    setParams["speed"] = -4;
    JsonObject setResults;
    uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "trickplay@0", setParams, setResults);
    EXPECT_EQ(Core::ERROR_NONE, setResult);
    std::cout << "SetTrickplay_Test Finished" << std::endl;
}

/********************************************************
************Test case Details **************************
** 6. TEST GET TRICKPLAY
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, GetTrickplay_Test) {
    std::cout << "GetTrickplay_Test Started" << std::endl;
    
    std::string trickPlayFilePath = fccDir + "/trick_play0";
    std::ifstream testFile(trickPlayFilePath);
    EXPECT_TRUE(testFile.is_open()) << "Trick play file should exist and be readable: " << trickPlayFilePath;
    testFile.close();
    
    // First, set a valid trickplay speed
    JsonObject setParams;
    setParams["speed"] = -4;
    JsonObject setResults;
    uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "trickplay@0", setParams, setResults);
    EXPECT_EQ(Core::ERROR_NONE, setResult);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    updateFileInBothLocations("trick_play0", "-4");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Now, get the trickplay speed 
    JsonObject getResults;
    uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "trickplay@0", getResults);
    EXPECT_EQ(Core::ERROR_NONE, getResult);
   
    if (getResult == Core::ERROR_NONE) {
        EXPECT_TRUE(getResults.HasLabel("speed")) << "Response should contain 'speed' field";
        if (getResults.HasLabel("speed")) {
            EXPECT_EQ(getResults["speed"].Number(), -4);
        }
    }
    
    std::cout << "GetTrickplay_Test Finished" << std::endl;
}

/********************************************************
************Test case Details **************************
** 7. TEST GET STATUS
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, GetStatus_Test) {
    std::cout << "GetStatus_Test Started" << std::endl;
    
    std::string statusFilePath = fccDir + "/stream_status";
    updateFileInBothLocations("stream_status", "0,0");
    
    std::ifstream verify(statusFilePath);
    bool fileWritten = false;
    if (verify.is_open()) {
        std::string content;
        std::getline(verify, content);
        verify.close();
        if (content == "0,0") {
            fileWritten = true;
            chmod(statusFilePath.c_str(), 0666);
        }
    }
    
    EXPECT_TRUE(fileWritten) << "Failed to create/update status file: " << statusFilePath;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Now, get the status
    JsonObject getResults;
    uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "status@0", getResults);
    EXPECT_EQ(Core::ERROR_NONE, getResult);
    
    if (getResult == Core::ERROR_NONE) {
        EXPECT_TRUE(getResults.HasLabel("streamSourceLost"));
        EXPECT_TRUE(getResults.HasLabel("streamSourceLossCount"));
        if (getResults.HasLabel("streamSourceLost")) {
            EXPECT_EQ(getResults["streamSourceLost"].Number(), 0);
        }
        if (getResults.HasLabel("streamSourceLossCount")) {
            EXPECT_EQ(getResults["streamSourceLossCount"].Number(), 0);
        }
    }
    
    std::cout << "GetStatus_Test Finished" << std::endl;
}

/********************************************************
************Test case Details **************************
** 8. TEST SET&GET TRACING
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, SetAndGetTracing_Test) {
    std::cout << "SetAndGetTracing_Test Started" << std::endl;

    // First, set tracing to true
    JsonObject setParams;
    setParams["tracing"] = true;
    JsonObject setResults;
    uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "tracing", setParams, setResults);
    EXPECT_EQ(Core::ERROR_NONE, setResult);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Now get the tracing value
    JsonObject getResults;
    uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "tracing", getResults);
    EXPECT_EQ(Core::ERROR_NONE, getResult);
    
    EXPECT_TRUE(getResults.HasLabel("tracing"));
    EXPECT_TRUE(getResults["tracing"].Boolean());

    std::cout << "SetAndGetTracing_Test Finished" << std::endl;
}
