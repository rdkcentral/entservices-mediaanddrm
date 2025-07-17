/*
* Copyright 2025 RDK Management
* Licensed under the Apache License, Version 2.0
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include "L2Tests.h"
#include "L2TestsMock.h"
// #include "PlayerInfoMock.h"
// #include "MockPlayerPropertiesImpl.h"
#include <interfaces/IPlayerInfo.h>

#define JSON_TIMEOUT   (1000)
#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define PLAYERINFO_CALLSIGN _T("PlayerInfo")
#define PLAYERINFOL2TEST_CALLSIGN _T("L2tests.1")

using namespace WPEFramework;
using namespace WPEFramework::Exchange;
using ::testing::NiceMock;

// ------------------------------------------------------------
//   MockPlayerPropertiesImpl
// ------------------------------------------------------------
class MockPlayerProperties : public IPlayerProperties {
public:
    MOCK_METHOD(uint32_t, AudioCodecs,
        (WPEFramework::Exchange::IPlayerProperties::IAudioCodecIterator*&),
        (const, override));
    MOCK_METHOD(uint32_t, VideoCodecs,
        (WPEFramework::Exchange::IPlayerProperties::IVideoCodecIterator*&),
        (const, override));
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (uint32_t), (override));
    MOCK_METHOD(uint32_t, Resolution, (Exchange::IPlayerProperties::PlaybackResolution&), (const, override));
    MOCK_METHOD(uint32_t, IsAudioEquivalenceEnabled, (bool&), (const, override));
};

// ------------------------------------------------------------
//  AudioCodecIterator Mock
// ------------------------------------------------------------
class MockAudioCodecIterator : public IPlayerProperties::IAudioCodecIterator {
public:
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(void, Reset, (uint32_t), (override));
    MOCK_METHOD(bool, Next, (Exchange::IPlayerProperties::AudioCodec&), (override));
    MOCK_METHOD(uint32_t, Count, (), (const, override));
    MOCK_METHOD(bool, Previous, (Exchange::IPlayerProperties::AudioCodec&), (override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t), (override));
    MOCK_METHOD(WPEFramework::Exchange::IPlayerProperties::AudioCodec, Current, (), (const, override));
};

// ------------------------------------------------------------
//  VideoCodecIterator Mock
// ------------------------------------------------------------
class MockVideoCodecIterator : public IPlayerProperties::IVideoCodecIterator {
public:
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(void, Reset, (uint32_t), (override));
    MOCK_METHOD(bool, Next, (Exchange::IPlayerProperties::VideoCodec&), (override));
    MOCK_METHOD(uint32_t, Count, (), (const, override));
    MOCK_METHOD(bool, Previous, (Exchange::IPlayerProperties::VideoCodec&), (override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t), (override));
    MOCK_METHOD(WPEFramework::Exchange::IPlayerProperties::VideoCodec, Current, (), (const, override));
};

// ------------------------------------------------------------
// Test Class
// ------------------------------------------------------------
class PlayerInfo_L2test : public L2TestMocks {
protected:
    Core::JSONRPC::Message message;
    string response;
    virtual ~PlayerInfo_L2test() override;

public:
    PlayerInfo_L2test();
};

PlayerInfo_L2test::PlayerInfo_L2test() : L2TestMocks() {
    TEST_LOG("PLAYERINFO Constructor");
    uint32_t status = ActivateService(PLAYERINFO_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);
}

PlayerInfo_L2test::~PlayerInfo_L2test() {
    TEST_LOG("PLAYERINFO Destructor");
    uint32_t status = DeactivateService(PLAYERINFO_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);
}

// ------------------------------------------------------------
//  Dummy Test
// ------------------------------------------------------------
TEST_F(PlayerInfo_L2test, DummyTest) {
    std::cout << "Dummy Test executed!" << std::endl;
}

// ------------------------------------------------------------
// AudioCodecs_Success Test
// ------------------------------------------------------------
TEST_F(PlayerInfo_L2test, AudioCodecs_Success) {
    auto* mockPlayer = new NiceMock<MockPlayerProperties>();

    auto* mockAudioIterator = new NiceMock<MockAudioCodecIterator>();
    auto* mockVideoIterator = new NiceMock<MockVideoCodecIterator>();

    IPlayerProperties::AudioCodec dummyCodec = IPlayerProperties::AUDIO_AC3;

    EXPECT_CALL(*mockAudioIterator, IsValid())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mockAudioIterator, Next(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(dummyCodec),
            ::testing::Return(true)))
        .WillOnce(::testing::Return(false));

    EXPECT_CALL(*mockPlayer, AudioCodecs(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(mockAudioIterator),
            ::testing::Return(Core::ERROR_NONE)));

    EXPECT_CALL(*mockPlayer, VideoCodecs(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(mockVideoIterator),
            ::testing::Return(Core::ERROR_NONE)));

    EXPECT_CALL(*mockPlayer, QueryInterface(::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod(PLAYERINFO_CALLSIGN, "audiocodecs", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);

}
