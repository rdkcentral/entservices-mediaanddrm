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

#include "RFCURLObserver.h"
#include "UtilsgetRFCConfig.h"
#include "UtilsLogging.h"

#define SYSTEMSERVICE_CALLSIGN "org.rdk.System"

using namespace WPEFramework;

namespace TTS {

RFCURLObserver* RFCURLObserver::getInstance() {
    static RFCURLObserver *instance = new RFCURLObserver();
    return instance;
}

void RFCURLObserver::triggerRFC(PluginHost::IShell* service, TTSConfiguration* config)
{
    m_defaultConfig = config;
    fetchURLFromConfig();
    registerNotification(service);
}

void RFCURLObserver::fetchURLFromConfig() {
    bool  m_rfcURLSet = false;
    RFC_ParamData_t param;
    #ifndef UNIT_TESTING
    m_rfcURLSet = Utils::getRFCConfig("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.TextToSpeech.URL", param);
    #endif
    if (m_rfcURLSet) {
	TTSLOG_INFO("Received RFC URL %s\n",param.value);
        m_defaultConfig->setRFCEndPoint(param.value);
    } else {
        TTSLOG_ERROR("Error: Reading URL RFC failed.");
    }
}

void RFCURLObserver::registerNotification(PluginHost::IShell* service) {
    if (_systemServicesPlugin == nullptr && !_eventRegistered) {
        _systemServicesPlugin = service->QueryInterfaceByCallsign<Exchange::ISystemServices>(SYSTEMSERVICE_CALLSIGN);
        if (_systemServicesPlugin != nullptr) {
            if (Core::ERROR_NONE == _systemServicesPlugin->Register(&_systemServicesNotification)) {
                _eventRegistered = true;
                TTSLOG_INFO("ISystemServices::Register event registered for onDeviceMgtUpdateReceived");
            } else {
                TTSLOG_ERROR("Failed to register ISystemServices notification handler");
                _eventRegistered = false;
            }
        } else {
            TTSLOG_ERROR("Failed to get SystemServices instance");
        }
    }
}

void RFCURLObserver::onDeviceMgtUpdateReceivedHandler(const string& source, const string& type, const bool success) {
    TTSLOG_INFO("onDeviceMgtUpdateReceived notification received - source: %s, type: %s, success: %d", 
                source.c_str(), type.c_str(), success);
    if (source == "rfc" && success) {
        fetchURLFromConfig();
    }
}

RFCURLObserver::~RFCURLObserver() {
    // Clean up resources
    if (_systemServicesPlugin != nullptr) {
        if (_eventRegistered) {
            _systemServicesPlugin->Unregister(&_systemServicesNotification);
            _eventRegistered = false;
        }
        _systemServicesPlugin->Release();
        _systemServicesPlugin = nullptr;
    }
}

}
