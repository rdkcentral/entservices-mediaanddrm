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

#include <mutex>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <sys/select.h>
#include <sys/time.h>
#include <interfaces/IScreenCapture.h>

#define TEST_LOG(x, ...)                                                                                                                         \
    fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); \
    fflush(stderr);

#define JSON_TIMEOUT (1000)
#define SCREENCAPTURE_CALLSIGN _T("org.rdk.ScreenCapture")
#define SCREENCAPTUREL2TEST_CALLSIGN _T("L2tests.1")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;
using ::WPEFramework::Exchange::IScreenCapture;

typedef enum : uint32_t
{
    ScreenCapture_UploadComplete = 0x00000001,
    ScreenCapture_StateInvalid = 0x00000000
} ScreenCaptureL2test_async_events_t;

class AsyncHandlerMock
{
public:
    AsyncHandlerMock()
    {
    }
    MOCK_METHOD(void, UploadComplete, (const bool &status, const string &message, const string &call_guid));
};

/* Notification Handler Class for COM-RPC*/
class ScreenCaptureNotificationHandler : public Exchange::IScreenCapture::INotification
{
private:
    std::mutex m_mutex;
    std::condition_variable m_condition_variable;
    uint32_t m_event_signalled;

    BEGIN_INTERFACE_MAP(Notification)
    INTERFACE_ENTRY(Exchange::IScreenCapture::INotification)
    END_INTERFACE_MAP

public:
    ScreenCaptureNotificationHandler() {}
    ~ScreenCaptureNotificationHandler() {}

    void UploadComplete(const bool &status, const string &message, const string &call_guid)
    {
        TEST_LOG("UploadComplete event triggered ***\n");
        std::unique_lock<std::mutex> lock(m_mutex);

        TEST_LOG("UploadComplete received: status: %d message: %s call_guid: %s \n", status, message.c_str(), call_guid.c_str());
        /* Notify the requester thread. */
        m_event_signalled |= ScreenCapture_UploadComplete;
        m_condition_variable.notify_one();
    }

    uint32_t WaitForRequestStatus(uint32_t timeout_ms, ScreenCaptureL2test_async_events_t expected_status)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = std::chrono::system_clock::now();
        std::chrono::milliseconds timeout(timeout_ms);
        uint32_t signalled = ScreenCapture_StateInvalid;

        while (!(expected_status & m_event_signalled))
        {
            if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout)
            {
                TEST_LOG("Timeout waiting for request status event");
                break;
            }
        }

        signalled = m_event_signalled;
        return signalled;
    }
};

class ScreenCaptureTest : public L2TestMocks
{
protected:
    virtual ~ScreenCaptureTest() override;

public:
    ScreenCaptureTest();
    void UploadComplete(const bool &status, const string &message, const string &call_guid);

    /**
     * @brief waits for various status change on asynchronous calls
     */
    uint32_t WaitForRequestStatus(uint32_t timeout_ms, ScreenCaptureL2test_async_events_t expected_status);
    uint32_t CreateScreenCaptureInterfaceObjectUsingComRPCConnection();

private:
    std::mutex m_mutex;
    std::condition_variable m_condition_variable;
    uint32_t m_event_signalled;

protected:
    /** @brief Pointer to the IShell interface */
    PluginHost::IShell *m_controller_screenCapture;

    /** @brief Pointer to the IScreenCapture interface */
    Exchange::IScreenCapture *m_screenCapturePlugin;

    Core::Sink<ScreenCaptureNotificationHandler> notify;
};

ScreenCaptureTest::ScreenCaptureTest() : L2TestMocks()
{
    Core::JSONRPC::Message message;
    string response;
    uint32_t status = Core::ERROR_GENERAL;

    m_event_signalled = ScreenCapture_StateInvalid;

    /* Activate plugin in constructor */
    status = ActivateService("org.rdk.ScreenCapture");
    EXPECT_EQ(Core::ERROR_NONE, status);

    if (CreateScreenCaptureInterfaceObjectUsingComRPCConnection() != Core::ERROR_NONE)
    {
        TEST_LOG("Invalid ScreenCapture_Client");
    }
    else
    {
        EXPECT_TRUE(m_controller_screenCapture != nullptr);
        if (m_controller_screenCapture)
        {
            EXPECT_TRUE(m_screenCapturePlugin != nullptr);
            if (m_screenCapturePlugin)
            {
                m_screenCapturePlugin->AddRef();
                m_screenCapturePlugin->Register(&notify);
            }
            else
            {
                TEST_LOG("m_screenCapturePlugin is NULL");
            }
        }
        else
        {
            TEST_LOG("m_controller_screenCapture is NULL");
        }
    }
}

/**
 * @brief Destructor for ScreenCapture L2 test class
 */
ScreenCaptureTest::~ScreenCaptureTest()
{
    Core::hresult status = Core::ERROR_GENERAL;
    m_event_signalled = ScreenCapture_StateInvalid;

    if (m_screenCapturePlugin)
    {
        m_screenCapturePlugin->Unregister(&notify);
        m_screenCapturePlugin->Release();
    }
    status = DeactivateService("org.rdk.ScreenCapture");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

/**
 *
 * @param[in] message from ScreenCapture on the change
 */
void ScreenCaptureTest::UploadComplete(const bool &status, const string &message, const string &call_guid)
{
    TEST_LOG("UploadComplete event triggered ***\n");
    std::unique_lock<std::mutex> lock(m_mutex);

    TEST_LOG("UploadComplete received: status: %d message: %s call_guid: %s \n", status, message.c_str(), call_guid.c_str());

    /* Notify the requester thread. */
    m_event_signalled |= ScreenCapture_UploadComplete;
    m_condition_variable.notify_one();
}

/**
 * @brief waits for various status change on asynchronous calls
 *
 * @param[in] timeout_ms timeout for waiting
 */
uint32_t ScreenCaptureTest::WaitForRequestStatus(uint32_t timeout_ms, ScreenCaptureL2test_async_events_t expected_status)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto now = std::chrono::system_clock::now();
    std::chrono::milliseconds timeout(timeout_ms);
    uint32_t signalled = ScreenCapture_StateInvalid;

    while (!(expected_status & m_event_signalled))
    {
        if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout)
        {
            TEST_LOG("Timeout waiting for request status event");
            break;
        }
    }

    signalled = m_event_signalled;

    return signalled;
}

// COM-RPC Changes
uint32_t ScreenCaptureTest::CreateScreenCaptureInterfaceObjectUsingComRPCConnection()
{
    Core::hresult return_value = Core::ERROR_GENERAL;
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> Engine_ScreenCapture;
    Core::ProxyType<RPC::CommunicatorClient> Client_ScreenCapture;

    TEST_LOG("Creating Engine_ScreenCapture");
    Engine_ScreenCapture = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    Client_ScreenCapture = Core::ProxyType<RPC::CommunicatorClient>::Create(Core::NodeId("/tmp/communicator"), Core::ProxyType<Core::IIPCServer>(Engine_ScreenCapture));

    TEST_LOG("Creating Engine_ScreenCapture Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    Engine_ScreenCapture->Announcements(mClient_ScreenCapture->Announcement());
#endif
    if (!Client_ScreenCapture.IsValid())
    {
        TEST_LOG("Invalid Client_ScreenCapture");
    }
    else
    {
        m_controller_screenCapture = Client_ScreenCapture->Open<PluginHost::IShell>(_T("org.rdk.ScreenCapture"), ~0, 3000);
        if (m_controller_screenCapture)
        {
            m_screenCapturePlugin = m_controller_screenCapture->QueryInterface<Exchange::IScreenCapture>();
            return_value = Core::ERROR_NONE;
        }
    }
    return return_value;
}

TEST_F(ScreenCaptureTest, No_URL)
{
    uint32_t signalled = ScreenCapture_StateInvalid;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    params["callGUID"] = "12345";
    status = InvokeServiceMethod("org.rdk.ScreenCapture", "uploadScreenCapture", params, result);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
 }

TEST_F(ScreenCaptureTest, Upload_Success)
{
    uint32_t signalled = ScreenCapture_StateInvalid;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(sockfd != -1);
    sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(11111);
    ASSERT_FALSE(bind(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0);
    ASSERT_FALSE(listen(sockfd, 10) < 0);
    fd_set set;
    struct timeval timeout;

    DRMScreenCapture drmHandle = {0, 1280, 720, 5120, 32};
    uint8_t* buffer = (uint8_t*) malloc(5120 * 720);
    memset(buffer, 0xff, 5120 * 720);

    EXPECT_CALL(*p_drmScreenCaptureApiImplMock, Init())
        .Times(1)
        .WillOnce(
            ::testing::Return(&drmHandle));

    ON_CALL(*p_drmScreenCaptureApiImplMock, GetScreenInfo(::testing::_))
        .WillByDefault(
            ::testing::Return(true));

    ON_CALL(*p_drmScreenCaptureApiImplMock, ScreenCapture(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Invoke(
            [&](DRMScreenCapture* handle, uint8_t* output, uint32_t size) {
                memcpy(output, buffer, size);
                return true;
            }));

    EXPECT_CALL(*p_drmScreenCaptureApiImplMock, Destroy(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(true));
    
    // Initialize the set
    FD_ZERO(&set);
    FD_SET(sockfd, &set);

    // Set the timeout to 5 seconds
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    std::thread thread = std::thread([&]()
                                     {
        int rv = select(sockfd + 1, &set, NULL, NULL, &timeout);
        if (rv == 0) {
            // Timeout occurred, no connection was made
            TEST_LOG("Timeout occurred, closing socket.");
        } 
        else {
            auto addrlen = sizeof(sockaddr);
            const int connection = accept(sockfd, (struct sockaddr*)&sockaddr, (socklen_t*)&addrlen);
            ASSERT_FALSE(connection < 0);
            char buffer[2048] = { 0 };
            ASSERT_TRUE(read(connection, buffer, 2048) > 0);

            std::string reqHeader(buffer);
            EXPECT_TRUE(std::string::npos != reqHeader.find("Content-Type: image/png"));

            std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
            send(connection, response.c_str(), response.size(), 0);

            close(connection);
        } });

    params["url"] = "http://127.0.0.1:11111";
    params["callGUID"] = "12345";
    status = InvokeServiceMethod("org.rdk.ScreenCapture", "uploadScreenCapture", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    TEST_LOG("After InvokeServiceMethod ***\n");
    signalled = notify.WaitForRequestStatus(JSON_TIMEOUT, ScreenCapture_UploadComplete);
    EXPECT_TRUE(signalled & ScreenCapture_UploadComplete);
    free(buffer);
    thread.join();
    close(sockfd);
    TEST_LOG("End of test case ***\n");
}

TEST_F(ScreenCaptureTest, Upload_Failed)
{
    uint32_t signalled = ScreenCapture_StateInvalid;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(sockfd != -1);
    sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(11111);
    ASSERT_FALSE(bind(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0);
    ASSERT_FALSE(listen(sockfd, 10) < 0);
    fd_set set;
    struct timeval timeout;

    // Initialize the set
    FD_ZERO(&set);
    FD_SET(sockfd, &set);

    // Set the timeout to 5 seconds
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    std::thread thread = std::thread([&]()
    {
        int rv = select(sockfd + 1, &set, NULL, NULL, &timeout);
        if (rv == 0) {
            // Timeout occurred, no connection was made
            TEST_LOG("Timeout occurred, closing socket.");
        } 
        else {
            auto addrlen = sizeof(sockaddr);
            const int connection = accept(sockfd, (struct sockaddr*)&sockaddr, (socklen_t*)&addrlen);
            ASSERT_FALSE(connection < 0);
            char buffer[2048] = { 0 };
            ASSERT_TRUE(read(connection, buffer, 2048) > 0);

            std::string reqHeader(buffer);
            EXPECT_TRUE(std::string::npos != reqHeader.find("Content-Type: image/png"));

            std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
            send(connection, response.c_str(), response.size(), 0);

            close(connection);
        } });
    params["url"] = "http://127.0.0.1:1111";
    params["callGUID"] = "1234";
    status = InvokeServiceMethod("org.rdk.ScreenCapture", "uploadScreenCapture", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    TEST_LOG("After InvokeServiceMethod ***\n");
    signalled = notify.WaitForRequestStatus(JSON_TIMEOUT, ScreenCapture_UploadComplete);
    thread.join();
    close(sockfd);
    TEST_LOG("End of test case ***\n");
}
