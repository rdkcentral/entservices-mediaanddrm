/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
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
 **/

#ifndef MODULE_NAME
#define MODULE_NAME ScreenCaptureCOMRPCTestApp
#endif

#include <iostream>
#include <WPEFramework/com/com.h>
#include <WPEFramework/core/core.h>
#include "WPEFramework/interfaces/IScreenCapture.h"

#include <chrono>
#include <iomanip>
#include <sstream>

using namespace WPEFramework;

// RAII Wrapper for IScreenCapture
class ScreenCaptureProxy {
    public:
        explicit ScreenCaptureProxy(Exchange::IScreenCapture* screenCapture) : _screenCapture(screenCapture) {}
        ~ScreenCaptureProxy() {
            if (_screenCapture != nullptr) {
                std::cout << "Releasing ScreenCapture proxy..." << std::endl;
                _screenCapture->Release();
            }
        }

        Exchange::IScreenCapture* operator->() const { return _screenCapture; }
        Exchange::IScreenCapture* Get() const { return _screenCapture; }

    private:
        Exchange::IScreenCapture* _screenCapture;
};

// Generic Logger class
class Logger {
public:
    template<typename... Args>
    static void LogEvent(const std::string& eventName, Args&&... args) {
        std::cout << CurrentTimestamp() << " " << eventName;
        ((std::cout << (sizeof...(Args) > 1 ? " " : "") << args), ...);
        std::cout << std::endl;
    }

    static std::string CurrentTimestamp() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t t = system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count() << "]";
        return oss.str();
    }
};

/********************************* Test All IScreenCapture Events **********************************/
class ScreenCaptureEventHandler : public Exchange::IScreenCapture::INotification {
    public:

    public:
        ScreenCaptureEventHandler() : _refCount(1) {}
        void AddRef() const override {
            _refCount.fetch_add(1, std::memory_order_relaxed);
        }
        uint32_t Release() const override {
            uint32_t count = _refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (count == 0) {
                delete this;
            }
            return count;
        }

        void UploadComplete(const bool& status, const std::string& message, const std::string& call_guid) override{
            Logger::LogEvent("UploadComplete -", "Status:", status, ", Message:", message, ", Call GUID:", call_guid);
        }

        BEGIN_INTERFACE_MAP(ScreenCaptureEventHandler)
            INTERFACE_ENTRY(Exchange::IScreenCapture::INotification)
        END_INTERFACE_MAP

    private:
        mutable std::atomic<uint32_t> _refCount;
};

int main(int argc, char* argv[])
{
    std::string callsign = (argc > 1) ? argv[1] : "org.rdk.ScreenCapture";
    std::string uploadUrl = (argc > 2) ? argv[2] : "http://localhost:8000/cgi-bin/upload.cgi";
    /******************************************* Init *******************************************/
    // Get environment variables
    const char* thunderAccess = std::getenv("THUNDER_ACCESS");
    std::string envThunderAccess = (thunderAccess != nullptr) ? thunderAccess : "/tmp/communicator";
    Logger::LogEvent("Using THUNDER_ACCESS:", envThunderAccess);
    Logger::LogEvent("Using callsign:", callsign);
    Logger::LogEvent("Using upload URL:", uploadUrl);

    // Initialize COMRPC
    Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), envThunderAccess.c_str());
    Core::ProxyType<RPC::CommunicatorClient> client = Core::ProxyType<RPC::CommunicatorClient>::Create(
            Core::NodeId(envThunderAccess.c_str()));

    if (client.IsValid() == false) {
        Logger::LogEvent("Failed to create COMRPC client.");
        return 1;
    }

    /************************************* Plugin Connector **************************************/
    // Create a proxy for the ScreenCapture plugin
    Exchange::IScreenCapture* rawScreenCapture = client->Open<Exchange::IScreenCapture>(_T(callsign.c_str()));
    if (rawScreenCapture == nullptr) {
        Logger::LogEvent("Failed to connect to ScreenCapture plugin.");
        return 1;
    }
    Logger::LogEvent("Connected to FrameRate plugin.");

    // Use RAII wrapper for ScreenCapture proxy
    ScreenCaptureProxy screenCapture(rawScreenCapture);

    /************************************ Subscribe to Events ************************************/
    ScreenCaptureEventHandler eventHandler;
    screenCapture->Register(&eventHandler);
    Logger::LogEvent("Event handler registered.");

    /************************************* Test All Methods **************************************/
    // virtual Core::hresult UploadScreenCapture(const string& url /* @in */, const string& callGUID /* @in */, Result &result /* @out  */ ) = 0;

    Exchange::IScreenCapture::Result result;
    // Generate unique GUID
    std::string callGUID = "test-call-guid-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    if (screenCapture->UploadScreenCapture(uploadUrl, callGUID, result) == Core::ERROR_NONE) {
        Logger::LogEvent("UploadScreenCapture succeeded.");
    } else {
        Logger::LogEvent("UploadScreenCapture failed.");
    }

    Logger::LogEvent("Waiting for events... Press Ctrl+C to exit.");
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }

    /******************************************* Clean-Up *****************************************/
    // ScreenCaptureProxy destructor will automatically release the proxy
    Logger::LogEvent("Exiting...");
    screenCapture->Unregister(&eventHandler);
    return 0;
}
