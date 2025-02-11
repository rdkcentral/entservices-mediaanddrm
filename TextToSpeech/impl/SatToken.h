/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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

#ifndef _TTS_SATTOKEN_H_
#define _TTS_SATTOKEN_H_
#include "TTSCommon.h"
#include <mutex>

namespace WPEFramework {
namespace Plugin {
namespace TTS {

class SatToken {
public:
    static SatToken* getInstance(const string callsign);
    string getSAT() { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_SatToken; 
    };

private:
    SatToken(){};
    SatToken(const string callsign);
    SatToken(const  SatToken&) = delete;
    SatToken& operator=(const  SatToken&) = delete;

    string getSecurityToken();
    void serviceAccessTokenChangedEventHandler (const JsonObject& parameters);
    void getServiceAccessToken();

    WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>* m_authService{nullptr};
    string m_SatToken;
    string m_callsign;
    std::mutex m_mutex;
    bool m_eventRegistered{false};
};
}
}
}
#endif
