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

#define TEST_LOG(x, ...) fprintf( stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

#define JSON_TIMEOUT   (1000)
#define SCREENCAPTURE_CALLSIGN  _T("org.rdk.ScreenCapture")
#define SCREENCAPTUREL2TEST_CALLSIGN _T("L2tests.1")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;
using ::WPEFramework::Exchange::IScreenCapture;

class AsyncHandlerMock
{
    public:
        AsyncHandlerMock()
        {
        }
};

class ScreenCaptureTest : public L2TestMocks {
protected:
    virtual ~ScreenCaptureTest() override;

    public:
    ScreenCaptureTest();
};

ScreenCaptureTest:: ScreenCaptureTest():L2TestMocks()
{
    Core::JSONRPC::Message message;
    string response;
    uint32_t status = Core::ERROR_GENERAL;

    /* Activate plugin in constructor */
    status = ActivateService("org.rdk.ScreenCapture");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

/**
 * @brief Destructor for ScreenCapture L2 test class
 */
ScreenCaptureTest::~ScreenCaptureTest()
{
    uint32_t status = Core::ERROR_GENERAL;
    
    status = DeactivateService("org.rdk.ScreenCapture");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

TEST_F(ScreenCaptureTest, No_URL)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    
    params["callGUID"] = "12345";
    status = InvokeServiceMethod("org.rdk.ScreenCapture", "uploadScreenCapture", params, result);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
}

TEST_F(ScreenCaptureTest, Upload)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(sockfd != -1);
    sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(11111);
    ASSERT_FALSE(bind(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0);
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

    std::thread thread = std::thread([&]() {
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
        }
    });
    
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    
    params["url"] = "http://127.0.0.1:11111";
    params["callGUID"] = "12345";
    status = InvokeServiceMethod("org.rdk.ScreenCapture", "uploadScreenCapture", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    free(buffer);
    thread.join();
    close(sockfd);
    TEST_LOG("End of test case ***\n");
}

TEST_F(ScreenCaptureTest, Upload_Failed)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(sockfd != -1);
    sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(11111);
    ASSERT_FALSE(bind(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0);
    ASSERT_FALSE(listen(sockfd, 10) < 0);
    fd_set set;
    struct timeval timeout;

    // Initialize the set
    FD_ZERO(&set);
    FD_SET(sockfd, &set);

    // Set the timeout to 5 seconds
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    std::thread thread = std::thread([&]() {
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
        }
    });
    
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    
    params["url"] = "http://127.0.0.1:1111";
    params["callGUID"] = "1234";
    status = InvokeServiceMethod("org.rdk.ScreenCapture", "uploadScreenCapture", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    thread.join();
    close(sockfd);
    TEST_LOG("End of test case ***\n");
}
