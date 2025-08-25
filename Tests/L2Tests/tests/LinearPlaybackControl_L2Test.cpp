
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

using namespace WPEFramework;

#define LINEARPLAYBACKCONTROL_CALLSIGN _T("LinearPlaybackControl")
#define LINEARPLAYBACKCONTROLL2TEST_CALLSIGN _T("L2tests.1")
#define JSON_TIMEOUT (1000)

class LinearPlaybackControlL2Test : public L2TestMocks {
protected:
    std::string mountPoint = "/mnt/streamfs";
    std::string fccDir = mountPoint + "/fcc";
    std::string channelFile = fccDir + "/chan_select0";

    void SetUp() override {
        // Create required directories and file with correct permissions
        mkdir(mountPoint.c_str(), 0777);
        mkdir(fccDir.c_str(), 0777);
        chmod(mountPoint.c_str(), 0777);
        chmod(fccDir.c_str(), 0777);
        // Create the channel file
        std::ofstream ofs(channelFile);
        ofs.close();
        chmod(channelFile.c_str(), 0666);

        // Create required files for status property
        std::ofstream seekFile(fccDir + "/seek0");
        seekFile << "0,0,0,0,0"; // 5 values for seek
        seekFile.close();
        chmod((fccDir + "/seek0").c_str(), 0666);

        std::ofstream trickFile(fccDir + "/trick_play0");
        trickFile << "-4"; // example trick play speed
        trickFile.close();
        chmod((fccDir + "/trick_play0").c_str(), 0666);

        std::ofstream statusFile(fccDir + "/stream_status0");
        statusFile << "0,0"; // 2 values for stream status
        statusFile.close();
        chmod((fccDir + "/stream_status0").c_str(), 0666);

        uint32_t status = Core::ERROR_GENERAL;
        status = ActivateService("LinearPlaybackControl");
        EXPECT_EQ(Core::ERROR_NONE, status);
    }

    void TearDown() override {
        std::remove(channelFile.c_str());
        rmdir(fccDir.c_str());
        rmdir(mountPoint.c_str());
        uint32_t status = Core::ERROR_GENERAL;
        status = DeactivateService("LinearPlaybackControl");
        EXPECT_EQ(Core::ERROR_NONE, status);
    }

    virtual ~LinearPlaybackControlL2Test() override {}
};

/********************************************************
************Test case Details **************************
** 1. TEST SET CHANNEL
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, SetChannel_Test) {
    std::cout << "SetChannel_Test Started" << std::endl;
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
    // First, set a valid channel
    JsonObject setParams;
    setParams["channel"] = "239.0.0.1:1234";
    JsonObject setResults;
    uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "channel@0", setParams, setResults);
    EXPECT_EQ(Core::ERROR_NONE, setResult);

    // Now, get the channel 
    JsonObject getResults;
    uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "channel@0", getResults);
    EXPECT_EQ(Core::ERROR_NONE, getResult);
    
    EXPECT_TRUE(getResults.HasLabel("channel"));
    EXPECT_EQ(getResults["channel"].String(), "239.0.0.1:1234");
    std::cout << "GetChannel_Test Finished" << std::endl;
}

/********************************************************
************Test case Details **************************
** 3. TEST SET SEEK
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, SetSeek_Test) {
    std::cout << "SetSeek_Test Started" << std::endl;
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
    // First, set a valid seek position
    JsonObject setParams;
    setParams["seekPosInSeconds"] = 10;
    JsonObject setResults;
    uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "seek@0", setParams, setResults);
    EXPECT_EQ(Core::ERROR_NONE, setResult);

    // Ensure seek file contains 5 comma-separated values as expected by plugin
    std::ofstream seekFile("/mnt/streamfs/fcc/seek0");
    seekFile << "10,0,0,0,0";
    seekFile.close();

    // Now, get the seek position 
    JsonObject getResults;
    uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "seek@0", getResults);
    EXPECT_EQ(Core::ERROR_NONE, getResult);
    EXPECT_TRUE(getResults.HasLabel("seekPosInSeconds"));
    EXPECT_EQ(getResults["seekPosInSeconds"].Number(), 10);
    std::cout << "GetSeek_Test Finished" << std::endl;
}

/********************************************************
************Test case Details **************************
** 5. TEST SET TRICKPLAY
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, SetTrickplay_Test) {
    std::cout << "SetTrickplay_Test Started" << std::endl;
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
    // First, set a valid trickplay speed
    JsonObject setParams;
    setParams["speed"] = -4;
    JsonObject setResults;
    uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "trickplay@0", setParams, setResults);
    EXPECT_EQ(Core::ERROR_NONE, setResult);

    // Now, get the trickplay speed 
    JsonObject getResults;
    uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "trickplay@0", getResults);
    EXPECT_EQ(Core::ERROR_NONE, getResult);
    std::cout << "GetTrickplay_Test: speed = " << (getResults.HasLabel("speed") ? getResults["speed"].Number() : -999) << std::endl;
    EXPECT_TRUE(getResults.HasLabel("speed"));
    EXPECT_EQ(getResults["speed"].Number(), -4);
    std::cout << "GetTrickplay_Test Finished" << std::endl;
}

/********************************************************
************Test case Details **************************
** 7. TEST GET STATUS
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, GetStatus_Test) {
    std::cout << "GetStatus_Test Started" << std::endl;
    // Ensure status file contains 2 comma-separated values as expected by plugin
    std::ofstream statusFile("/mnt/streamfs/fcc/stream_status");
    statusFile << "0,0";
    statusFile.close();

    // Now, get the status
    JsonObject getResults;
    uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "status@0", getResults);
    EXPECT_EQ(Core::ERROR_NONE, getResult);
    std::cout << "GetStatus_Test: streamSourceLost = " << (getResults.HasLabel("streamSourceLost") ? getResults["streamSourceLost"].Number() : -1) << std::endl;
    std::cout << "GetStatus_Test: streamSourceLossCount = " << (getResults.HasLabel("streamSourceLossCount") ? getResults["streamSourceLossCount"].Number() : -1) << std::endl;
    EXPECT_TRUE(getResults.HasLabel("streamSourceLost"));
    EXPECT_TRUE(getResults.HasLabel("streamSourceLossCount"));
    EXPECT_EQ(getResults["streamSourceLost"].Number(), 0);
    EXPECT_EQ(getResults["streamSourceLossCount"].Number(), 0);
    std::cout << "GetStatus_Test Finished" << std::endl;
}

/********************************************************
************Test case Details **************************
** 8. TEST SET TRACING
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, SetTracing_Test) {
    std::cout << "SetTracing_Test Started" << std::endl;
    JsonObject setParams;
    setParams["tracing"] = true;
    JsonObject setResults;
    uint32_t setResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "tracing", setParams, setResults);
    EXPECT_EQ(Core::ERROR_NONE, setResult);
    std::cout << "SetTracing_Test Finished" << std::endl;
}

/********************************************************
************Test case Details **************************
** 9. TEST GET TRACING
*******************************************************/
TEST_F(LinearPlaybackControlL2Test, GetTracing_Test) {
    std::cout << "GetTracing_Test Started" << std::endl;
    Core::JSON::Boolean tracingResult;
    uint32_t getResult = InvokeServiceMethod(LINEARPLAYBACKCONTROL_CALLSIGN, "tracing", tracingResult);
    EXPECT_EQ(Core::ERROR_NONE, getResult);
    std::cout << "GetTracing_Test Finished" << std::endl;
}


