
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include "L2Tests.h"
#include "L2TestsMock.h"
#include "PlayerMock.h"

#define JSON_TIMEOUT   (1000)
#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define PLAYERINFO_CALLSIGN _T("PlayerInfo")
#define PLAYERINFOL2TEST_CALLSIGN _T("L2tests.1")

using namespace WPEFramework;
using namespace WPEFramework::Exchange;
using ::testing::NiceMock;
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
// AudioCodecs_Success Test
// ------------------------------------------------------------
TEST_F(PlayerInfo_L2test, AudioCodecs_Success) {
    std::cout << "Entering AudioCodecs_Success test" << std::endl;

    auto* mockPlayer = new NiceMock<MockPlayerProperties>();
    auto* mockAudioIterator = new NiceMock<MockAudioCodecIterator>();

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

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod(PLAYERINFO_CALLSIGN, "audiocodecs", params, result);
    std::cout << "Service method invoked, status=" << status << std::endl;
    EXPECT_EQ(status, Core::ERROR_NONE);
}

// ------------------------------------------------------------
// VideoCodecs_Success Test
// ------------------------------------------------------------
TEST_F(PlayerInfo_L2test, VideoCodecs_Success) {
    std::cout << "Entering VideoCodecs_Success test" << std::endl;

    auto* mockPlayer = new NiceMock<MockPlayerProperties>();
    auto* mockVideoIterator = new NiceMock<MockVideoCodecIterator>();

    IPlayerProperties::VideoCodec dummyCodec = IPlayerProperties::VIDEO_H264;

    EXPECT_CALL(*mockVideoIterator, IsValid())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mockVideoIterator, Next(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(dummyCodec),
            ::testing::Return(true)))
        .WillOnce(::testing::Return(false));
    EXPECT_CALL(*mockPlayer, VideoCodecs(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(mockVideoIterator),
            ::testing::Return(Core::ERROR_NONE)));

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod(PLAYERINFO_CALLSIGN, "videocodecs", params, result);
    std::cout << "Service method invoked, status=" << status << std::endl;
    EXPECT_EQ(status, Core::ERROR_NONE);
}
