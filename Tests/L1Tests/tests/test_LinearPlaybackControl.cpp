#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <boost/algorithm/string.hpp>
#include <string>
#include <thread>
#include <chrono>
#include "DemuxerStreamFsFCC.h"
#include "LinearPlaybackControl.h"
#include "LinearConfig.h"
#include <core/core.h>
#include <interfaces/json/JsonData_LinearPlaybackControl.h>
#include <sys/stat.h>
#include <errno.h>
#include <iostream>

// Include Wraps.h for WrapsImpl interface
#include "Wraps.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using namespace JsonData::LinearPlaybackControl;
using ::testing::_;
using ::testing::Return;

// Constants for test values
const std::string TEST_CHANNEL = "HBO";
const uint64_t TEST_SEEK_POS = 123;
const int16_t TEST_SPEED = 4;
const bool TEST_STREAM_LOST = true;
const uint64_t TEST_LOSS_COUNT = 2;

// Helper function to create directory recursively
static bool createDirectoryRecursive(const std::string& path, mode_t mode = 0755) {
    if (path.empty()) {
        std::cerr << "Error: Empty path provided to createDirectoryRecursive" << std::endl;
        return false;
    }

    std::string currentPath;
    size_t pos = 0;

    if (path[0] == '/') {
        currentPath = "/";
        pos = 1;
    }

    while (pos <= path.length()) {
        pos = path.find('/', pos);
        if (pos == std::string::npos) {
            pos = path.length();
        }

        currentPath = path.substr(0, pos);
        if (!currentPath.empty() && currentPath != "/") {
            if (mkdir(currentPath.c_str(), mode) != 0 && errno != EEXIST) {
                std::cerr << "Error: Failed to create directory " << currentPath
                          << ", errno: " << errno << " (" << strerror(errno) << ")" << std::endl;
                return false;
            }
        }

        if (pos < path.length()) {
            ++pos;
        } else {
            break;
        }
    }

    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        std::cerr << "Error: Directory " << path << " does not exist or is not a directory after creation" << std::endl;
        return false;
    }

    return true;
}

// Mock implementation of WrapsImpl
class MockWrapsImpl : public WrapsImpl {
public:
    MOCK_METHOD1(system, int(const char*));
    MOCK_METHOD2(popen, FILE*(const char*, const char*));
    MOCK_METHOD1(pclose, int(FILE*));
    MOCK_METHOD3(syslog, void(int, const char*, va_list));
    MOCK_METHOD2(setmntent, FILE*(const char*, const char*));
    MOCK_METHOD1(getmntent, struct mntent*(FILE*));
    MOCK_METHOD3(v_secure_popen, FILE*(const char*, const char*, va_list));
    MOCK_METHOD1(v_secure_pclose, int(FILE*));
    MOCK_METHOD2(v_secure_system, int(const char*, va_list));
    MOCK_METHOD3(readlink, ssize_t(const char*, char*, size_t));
    MOCK_METHOD1(time, time_t(time_t*));
    MOCK_METHOD1(wpa_ctrl_open, struct wpa_ctrl*(const char* ctrl_path));
    MOCK_METHOD6(wpa_ctrl_request, int(struct wpa_ctrl* ctrl, const char* cmd, size_t cmd_len,
                                       char* reply, size_t* reply_len, void (*msg_cb)(char*, size_t)));
    MOCK_METHOD1(wpa_ctrl_close, void(struct wpa_ctrl* ctrl));
    MOCK_METHOD1(wpa_ctrl_pending, int(struct wpa_ctrl* ctrl));
    MOCK_METHOD3(wpa_ctrl_recv, int(struct wpa_ctrl* ctrl, char* reply, size_t* reply_len));
    MOCK_METHOD1(wpa_ctrl_attach, int(struct wpa_ctrl* ctrl));
    MOCK_METHOD1(unlink, int(const char* filePath));
    MOCK_METHOD3(ioctl, int(int fd, unsigned long request, void* arg));
    MOCK_METHOD2(statvfs, int(const char* path, struct statvfs* buf));
    MOCK_METHOD2(statfs, int(const char* path, struct statfs* buf));
    MOCK_METHOD2(getline, std::istream&(std::istream& is, std::string& line));
    MOCK_METHOD2(mkdir, int(const char* path, mode_t mode));
    MOCK_METHOD5(mount, int(const char* source, const char* target, const char* filesystemtype,
                            unsigned long mountflags, const void*));
    MOCK_METHOD2(stat, int(const char* path, struct stat* info));
    MOCK_METHOD3(open, int(const char* pathname, int flags, mode_t mode));
    MOCK_METHOD1(umount, int(const char* path));
    MOCK_METHOD1(rmdir, int(const char* pathname));
};

// Global test fixture to initialize Wraps::impl
class WrapsTestFixture : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        Wraps::setImpl(nullptr); // Reset impl to avoid conflicts
        static MockWrapsImpl mock;
        Wraps::setImpl(&mock);

        ON_CALL(mock, popen(_, _))
            .WillByDefault([](const char*, const char*) -> FILE* {
                return std::tmpfile();
            });
        ON_CALL(mock, pclose(_))
            .WillByDefault([](FILE* fp) -> int {
                if (fp) fclose(fp);
                return 0;
            });
        ON_CALL(mock, v_secure_popen(_, _, _))
            .WillByDefault([](const char*, const char*, va_list) -> FILE* {
                return std::tmpfile();
            });
        ON_CALL(mock, v_secure_pclose(_))
            .WillByDefault([](FILE* fp) -> int {
                if (fp) fclose(fp);
                return 0;
            });
        ON_CALL(mock, system(_)).WillByDefault(Return(0));
        ON_CALL(mock, v_secure_system(_, _)).WillByDefault(Return(0));
        ON_CALL(mock, syslog(_, _, _)).WillByDefault([](int, const char*, va_list) {});
        ON_CALL(mock, readlink(_, _, _)).WillByDefault(Return(0));
        ON_CALL(mock, time(_)).WillByDefault([](time_t* t) -> time_t {
            time_t now = std::time(nullptr);
            if (t) *t = now;
            return now;
        });
        EXPECT_CALL(mock, syslog(_, _, _)).Times(testing::AnyNumber());
    }

    static void TearDownTestSuite() {
        Wraps::setImpl(nullptr); // Clean up after test suite
    }

    void TearDown() override {
        Wraps::setImpl(nullptr);
    }
};

// DemuxerRealTest for real DemuxerStreamFsFCC implementation
class DemuxerRealTest : public WrapsTestFixture {
protected:
    LinearConfig::Config config;
    DemuxerStreamFsFCC* demuxer;
    std::string testDir;
    std::string trickPlayFile;
    std::string seekFile;
    std::string channelFile;
    std::string statusFile;

    void SetUp() override {
        config.MountPoint = "/tmp/test_mount";
        testDir = "/tmp/test_dir/fcc";
        ASSERT_TRUE(createDirectoryRecursive(testDir)) << "Failed to create test directory: " << testDir;
        ASSERT_FALSE(config.MountPoint.Value().empty()) << "Config MountPoint is empty";

        demuxer = new DemuxerStreamFsFCC(&config, 2);
        ASSERT_NE(demuxer, nullptr) << "DemuxerStreamFsFCC creation failed";

        trickPlayFile = demuxer->getTrickPlayFile();
        seekFile = testDir + "/seek2";
        channelFile = testDir + "/channel2";
        statusFile = testDir + "/stream_status2";

        chmod(testDir.c_str(), 0777);
        std::cout << "Demuxer initialized with trickPlayFile: " << trickPlayFile << std::endl;
    }

    void TearDown() override {
        std::remove(trickPlayFile.c_str());
        std::remove(seekFile.c_str());
        std::remove(channelFile.c_str());
        std::remove(statusFile.c_str());
        delete demuxer;
    }

    void createFile(const std::string& path, const std::string& content) {
        std::cout << "Creating file: " << path << std::endl;
        std::ofstream file(path);
        ASSERT_TRUE(file.is_open()) << "Failed to create file: " << path;
        file << content;
        file.close();
        chmod(path.c_str(), 0666);
    }

    void makeFileReadOnly(const std::string& path) {
        std::cout << "Making file read-only: " << path << std::endl;
        chmod(path.c_str(), S_IRUSR);
    }
};

// TestableLinearPlaybackControl for accessing protected/private members
class TestableLinearPlaybackControl : public LinearPlaybackControl {
public:
    TestableLinearPlaybackControl() = default;
    ~TestableLinearPlaybackControl() override = default;

    std::unique_ptr<DemuxerStreamFsFCC>& demuxer() { return getDemuxer(); }
    bool& streamFSEnabled() { return getStreamFSEnabled(); }

    void AddRef() const override {}
    uint32_t Release() const override { return 0; }

    using LinearPlaybackControl::endpoint_set_channel;
    using LinearPlaybackControl::endpoint_get_channel;
    using LinearPlaybackControl::endpoint_set_seek;
    using LinearPlaybackControl::endpoint_get_seek;
    using LinearPlaybackControl::endpoint_set_trickplay;
    using LinearPlaybackControl::endpoint_get_trickplay;
    using LinearPlaybackControl::endpoint_get_status;
    using LinearPlaybackControl::endpoint_set_tracing;
    using LinearPlaybackControl::endpoint_get_tracing;
    using LinearPlaybackControl::callDemuxer;

    const string Initialize(PluginHost::IShell* service) override { return LinearPlaybackControl::Initialize(service); }
    void Deinitialize(PluginHost::IShell* service) override { LinearPlaybackControl::Deinitialize(service); }
    string Information() const override { return LinearPlaybackControl::Information(); }

    void speedchangedNotify(const std::string& data) { LinearPlaybackControl::speedchangedNotify(data); }
};

// Mock IShell
class MockShell : public WPEFramework::PluginHost::IShell {
public:
    MOCK_CONST_METHOD0(AddRef, void());
    MOCK_CONST_METHOD0(Release, uint32_t());
    MOCK_METHOD1(QueryInterface, void*(uint32_t id));
    MOCK_METHOD2(EnableWebServer, void(const string& URL, const string& prefix));
    MOCK_METHOD0(DisableWebServer, void());
    MOCK_CONST_METHOD0(SystemPath, string());
    MOCK_CONST_METHOD0(PluginPath, string());
    MOCK_CONST_METHOD0(Startup, WPEFramework::PluginHost::IShell::startup());
    MOCK_METHOD1(Startup, Core::hresult(const WPEFramework::PluginHost::IShell::startup value));
    MOCK_CONST_METHOD0(Resumed, bool());
    MOCK_METHOD1(Resumed, Core::hresult(bool value));
    MOCK_CONST_METHOD1(Metadata, Core::hresult(string& info));
    MOCK_CONST_METHOD0(Model, string());
    MOCK_CONST_METHOD0(Background, bool());
    MOCK_CONST_METHOD0(Accessor, string());
    MOCK_CONST_METHOD0(WebPrefix, string());
    MOCK_CONST_METHOD0(Locator, string());
    MOCK_CONST_METHOD0(ClassName, string());
    MOCK_CONST_METHOD0(Versions, string());
    MOCK_CONST_METHOD0(Callsign, string());
    MOCK_CONST_METHOD0(PersistentPath, string());
    MOCK_CONST_METHOD0(VolatilePath, string());
    MOCK_CONST_METHOD0(DataPath, string());
    MOCK_CONST_METHOD0(ProxyStubPath, string());
    MOCK_CONST_METHOD0(SystemRootPath, string());
    MOCK_METHOD1(SystemRootPath, uint32_t(const string& path));
    MOCK_CONST_METHOD1(Substitute, string(const string& input));
    MOCK_CONST_METHOD0(HashKey, string());
    MOCK_CONST_METHOD0(ConfigLine, string());
    MOCK_METHOD1(ConfigLine, uint32_t(const string& config));
    MOCK_CONST_METHOD1(IsSupported, bool(uint8_t version));
    MOCK_METHOD0(SubSystems, WPEFramework::PluginHost::ISubSystem*());
    MOCK_METHOD1(Notify, void(const string& message));
    MOCK_METHOD1(Register, void(WPEFramework::PluginHost::IPlugin::INotification* notification));
    MOCK_METHOD1(Unregister, void(WPEFramework::PluginHost::IPlugin::INotification* notification));
    MOCK_CONST_METHOD0(State, WPEFramework::PluginHost::IShell::state());
    MOCK_METHOD2(QueryInterfaceByCallsign, void*(uint32_t id, const string& name));
    MOCK_METHOD1(Activate, uint32_t(WPEFramework::PluginHost::IShell::reason why));
    MOCK_METHOD1(Deactivate, uint32_t(WPEFramework::PluginHost::IShell::reason why));
    MOCK_METHOD1(Unavailable, uint32_t(WPEFramework::PluginHost::IShell::reason why));
    MOCK_METHOD1(Hibernate, Core::hresult(uint32_t timeout));
    MOCK_CONST_METHOD0(Reason, WPEFramework::PluginHost::IShell::reason());
    MOCK_METHOD0(COMLink, WPEFramework::PluginHost::IShell::ICOMLink*());
    MOCK_METHOD2(Submit, uint32_t(uint32_t id, const WPEFramework::Core::ProxyType<WPEFramework::Core::JSON::IElement>& element));
    MOCK_METHOD0(Version, string());
    MOCK_METHOD0(Major, uint8_t());
    MOCK_METHOD0(Minor, uint8_t());
    MOCK_METHOD0(Patch, uint8_t());
    MOCK_METHOD0(AutoStart, bool());
    MOCK_METHOD2(Hibernate, uint32_t(const string&, uint32_t));
    MOCK_METHOD2(Wakeup, uint32_t(const string&, uint32_t));
};

// LinearPlaybackControlPluginTest for plugin lifecycle and JSON-RPC endpoints
class LinearPlaybackControlPluginTest : public WrapsTestFixture {
protected:
    TestableLinearPlaybackControl lpc;
    MockShell mockShell;
    LinearConfig::Config config;
    std::string testDir;
    std::string trickPlayFile;
    std::string seekFile;
    std::string channelFile;
    std::string statusFile;

    void SetUp() override {
        config.MountPoint = "/tmp/test_mount";
        testDir = "/tmp/test_dir/fcc";
        ASSERT_TRUE(createDirectoryRecursive(testDir)) << "Failed to create test directory: " << testDir;
        chmod(testDir.c_str(), 0777);

        lpc.streamFSEnabled() = true;
        lpc.demuxer() = std::unique_ptr<DemuxerStreamFsFCC>(new DemuxerStreamFsFCC(&config, 0));
        ASSERT_TRUE(lpc.demuxer() != nullptr) << "Demuxer initialization failed";

        trickPlayFile = lpc.demuxer()->getTrickPlayFile();
        seekFile = testDir + "/seek0";
        channelFile = testDir + "/channel0";
        statusFile = testDir + "/stream_status0";

        std::cout << "Test setup: trickPlayFile=" << trickPlayFile
                  << ", seekFile=" << seekFile
                  << ", channelFile=" << channelFile
                  << ", statusFile=" << statusFile << std::endl;
    }

    void TearDown() override {
        std::remove(trickPlayFile.c_str());
        std::remove(seekFile.c_str());
        std::remove(channelFile.c_str());
        std::remove(statusFile.c_str());
        lpc.demuxer().reset();
    }

    void createFile(const std::string& path, const std::string& content) {
        std::cout << "Creating file: " << path << std::endl;
        std::ofstream file(path);
        ASSERT_TRUE(file.is_open()) << "Failed to create file: " << path;
        file << content;
        file.close();
        chmod(path.c_str(), 0666);
    }

    void makeFileReadOnly(const std::string& path) {
        std::cout << "Making file read-only: " << path << std::endl;
        chmod(path.c_str(), S_IRUSR);
    }
};

// Tests for DemuxerRealTest
TEST_F(DemuxerRealTest, ConstructorSetsCorrectFilePaths) {
    EXPECT_EQ(demuxer->getTrickPlayFile(), "/tmp/test_mount/fcc/trick_play2");
}

TEST_F(DemuxerRealTest, OpenCloseDoesNotCrash) {
    EXPECT_EQ(demuxer->open(), IDemuxer::IO_STATUS::OK);
    EXPECT_EQ(demuxer->close(), IDemuxer::IO_STATUS::OK);
}

TEST_F(DemuxerRealTest, SetChannel_FileError) {
    createFile(channelFile, "");
    makeFileReadOnly(channelFile);
    EXPECT_EQ(demuxer->setChannel(TEST_CHANNEL), IDemuxer::IO_STATUS::OK);
}

// Tests for LinearPlaybackControlPluginTest
TEST_F(LinearPlaybackControlPluginTest, ConstructorDestructor) {
    SUCCEED();
}

TEST_F(LinearPlaybackControlPluginTest, InitializeSuccess) {
    EXPECT_CALL(mockShell, ConfigLine()).WillOnce(Return("{\"mountPoint\": \"/tmp/test_mount\"}"));
    EXPECT_EQ(lpc.Initialize(&mockShell), "");
}

TEST_F(LinearPlaybackControlPluginTest, InitializeSuccessWithStreamFSEnabled) {
    EXPECT_CALL(mockShell, ConfigLine())
        .WillOnce(Return("{\"mountPoint\": \"/tmp/test_mount\", \"isStreamFSEnabled\": true}"));
    EXPECT_EQ(lpc.Initialize(&mockShell), "");
}

TEST_F(LinearPlaybackControlPluginTest, InitializeSuccessWithStreamFSDisabled) {
    EXPECT_CALL(mockShell, ConfigLine())
        .WillOnce(Return("{\"mountPoint\": \"/tmp/test_mount\", \"isStreamFSEnabled\": false}"));
    EXPECT_EQ(lpc.Initialize(&mockShell), "");
}

TEST_F(LinearPlaybackControlPluginTest, InitializeInvalidJson) {
    EXPECT_CALL(mockShell, ConfigLine()).WillOnce(Return("{invalid_json}"));
    EXPECT_EQ(lpc.Initialize(&mockShell), "");
}

TEST_F(LinearPlaybackControlPluginTest, DeinitializeWithNullService) {
    EXPECT_CALL(mockShell, ConfigLine())
        .WillOnce(Return("{\"mountPoint\": \"/tmp/test_mount\", \"isStreamFSEnabled\": true}"));
    lpc.Initialize(&mockShell);
    lpc.Deinitialize(nullptr);
    SUCCEED();
}

TEST_F(LinearPlaybackControlPluginTest, InformationReturnsEmptyString) {
    EXPECT_EQ(lpc.Information(), "");
}

TEST_F(LinearPlaybackControlPluginTest, SetChannel_MissingChannel) {
    ChannelData params;

    uint32_t result = lpc.endpoint_set_channel("0", params);
    EXPECT_EQ(result, Core::ERROR_BAD_REQUEST);
}

TEST_F(LinearPlaybackControlPluginTest, SetChannel_FileError) {
    ChannelData params;
    params.Channel = TEST_CHANNEL;
    createFile(channelFile, "");
    makeFileReadOnly(channelFile);

    uint32_t result = lpc.endpoint_set_channel("0", params);
    EXPECT_EQ(result, Core::ERROR_NONE);
}

TEST_F(LinearPlaybackControlPluginTest, SetChannel_InvalidDemuxerId) {
    ChannelData params;
    params.Channel = TEST_CHANNEL;

    uint32_t result = lpc.endpoint_set_channel("invalid_id", params);
    EXPECT_EQ(result, Core::ERROR_NONE);
}

TEST_F(LinearPlaybackControlPluginTest, GetChannel_ValidResponse) {
    createFile(channelFile, TEST_CHANNEL);
    ChannelData params;

    uint32_t result = lpc.endpoint_get_channel("0", params);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(params.Channel.Value(), TEST_CHANNEL);
}

TEST_F(LinearPlaybackControlPluginTest, GetChannel_FileError) {
    ChannelData params;

    uint32_t result = lpc.endpoint_get_channel("0", params);
    EXPECT_EQ(result, Core::ERROR_NONE);
}

TEST_F(LinearPlaybackControlPluginTest, SetSeek_MissingSeekPos) {
    SeekData params;

    uint32_t result = lpc.endpoint_set_seek("0", params);
    EXPECT_EQ(result, Core::ERROR_BAD_REQUEST);
}

TEST_F(LinearPlaybackControlPluginTest, SetSeek_FileError) {
    SeekData params;
    params.SeekPosInSeconds = TEST_SEEK_POS;
    createFile(seekFile, "");
    makeFileReadOnly(seekFile);

    uint32_t result = lpc.endpoint_set_seek("0", params);
    EXPECT_EQ(result, Core::ERROR_NONE);
}

TEST_F(LinearPlaybackControlPluginTest, GetSeek_FileError) {
    SeekData params;

    uint32_t result = lpc.endpoint_get_seek("0", params);
    EXPECT_EQ(result, Core::ERROR_READ_ERROR);
}

TEST_F(LinearPlaybackControlPluginTest, SetTrickPlay_ValidInput) {
    TrickplayData params;
    params.Speed = TEST_SPEED;

    createFile(trickPlayFile, "");
    uint32_t result = lpc.endpoint_set_trickplay("0", params);
    EXPECT_EQ(result, Core::ERROR_NONE);

    std::ifstream file(trickPlayFile);
    std::string content;
    std::getline(file, content);
    EXPECT_EQ(content, std::to_string(TEST_SPEED));
}

TEST_F(LinearPlaybackControlPluginTest, SetTrickPlay_MissingSpeed) {
    TrickplayData params;

    uint32_t result = lpc.endpoint_set_trickplay("0", params);
    EXPECT_EQ(result, Core::ERROR_BAD_REQUEST);
}

TEST_F(LinearPlaybackControlPluginTest, SetTrickPlay_FileError) {
    TrickplayData params;
    params.Speed = TEST_SPEED;
    createFile(trickPlayFile, "");
    makeFileReadOnly(trickPlayFile);

    uint32_t result = lpc.endpoint_set_trickplay("0", params);
    EXPECT_EQ(result, Core::ERROR_NONE);
}

TEST_F(LinearPlaybackControlPluginTest, GetTrickPlay_ValidResponse) {
    createFile(trickPlayFile, std::to_string(TEST_SPEED));
    TrickplayData params;

    uint32_t result = lpc.endpoint_get_trickplay("0", params);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(params.Speed.Value(), TEST_SPEED);
}

TEST_F(LinearPlaybackControlPluginTest, GetTrickPlay_FileError) {
    TrickplayData params;

    uint32_t result = lpc.endpoint_get_trickplay("0", params);
    EXPECT_EQ(result, Core::ERROR_READ_ERROR);
}

TEST_F(LinearPlaybackControlPluginTest, GetStatus_FileError) {
    StatusData params;

    uint32_t result = lpc.endpoint_get_status("0", params);
    EXPECT_EQ(result, Core::ERROR_READ_ERROR);
}

TEST_F(LinearPlaybackControlPluginTest, SetTracing_ValidInput) {
    TracingData params;
    params.Tracing = true;

    uint32_t result = lpc.endpoint_set_tracing(params);
    EXPECT_EQ(result, Core::ERROR_NONE);
}

TEST_F(LinearPlaybackControlPluginTest, GetTracing_ValidResponse) {
    TracingData params;

    uint32_t result = lpc.endpoint_get_tracing(params);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(params.Tracing.Value(), true);
}

TEST_F(LinearPlaybackControlPluginTest, CallDemuxer_StreamFSDisabled) {
    lpc.streamFSEnabled() = false;

    uint32_t result = lpc.callDemuxer("0", [](IDemuxer*) { return Core::ERROR_NONE; });
    EXPECT_EQ(result, Core::ERROR_BAD_REQUEST);
}

TEST_F(LinearPlaybackControlPluginTest, CallDemuxer_InvalidDemuxerId) {
    uint32_t result = lpc.callDemuxer("invalid_id", [](IDemuxer*) { return Core::ERROR_NONE; });
    EXPECT_EQ(result, Core::ERROR_NONE);
}

TEST_F(LinearPlaybackControlPluginTest, CallDemuxer_Success) {
    uint32_t result = lpc.callDemuxer("0", [](IDemuxer* demuxer) {
        EXPECT_NE(demuxer, nullptr);
        return Core::ERROR_NONE;
    });
    EXPECT_EQ(result, Core::ERROR_NONE);
}

TEST_F(LinearPlaybackControlPluginTest, SpeedChangedNotify) {
    createFile(trickPlayFile, "4");
    lpc.speedchangedNotify("4");
    SUCCEED();
}

TEST_F(LinearPlaybackControlPluginTest, SpeedChangedNotify_InvalidSpeed) {
    lpc.speedchangedNotify("invalid");
    SUCCEED();
}

TEST_F(LinearPlaybackControlPluginTest, Initialize_InvalidConfig) {
    EXPECT_CALL(mockShell, ConfigLine()).WillOnce(Return("{\"mountPoint\": \"\"}"));
    EXPECT_EQ(lpc.Initialize(&mockShell), "");
}
