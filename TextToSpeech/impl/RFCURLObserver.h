/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#ifndef _TTS_RFC_H_
#define _TTS_RFC_H_
#include "TTSCommon.h"
#include "TTSConfiguration.h"
#include <interfaces/ISystemServices.h>

namespace TTS {

class RFCURLObserver {
public:
    static RFCURLObserver* getInstance();
    void triggerRFC(WPEFramework::PluginHost::IShell* service, TTSConfiguration* config);
    ~RFCURLObserver();

private:
    RFCURLObserver()
        : _systemServicesNotification(*this)
    {};
    RFCURLObserver(const RFCURLObserver&) = delete;
    RFCURLObserver& operator=(const RFCURLObserver&) = delete;

    class SystemServicesNotification : public WPEFramework::Exchange::ISystemServices::INotification
    {
    private:
        SystemServicesNotification(const SystemServicesNotification&) = delete;
        SystemServicesNotification& operator=(const SystemServicesNotification&) = delete;

    public:
        explicit SystemServicesNotification(RFCURLObserver& parent)
            : _parent(parent)
        {
        }
        ~SystemServicesNotification() override = default;

    public:
        void OnDeviceMgtUpdateReceived(const string& source, const string& type, const bool success) override
        {
            _parent.onDeviceMgtUpdateReceivedHandler(source, type, success);
        }

        BEGIN_INTERFACE_MAP(SystemServicesNotification)
        INTERFACE_ENTRY(WPEFramework::Exchange::ISystemServices::INotification)
        END_INTERFACE_MAP

    private:
        RFCURLObserver& _parent;
    };

    void fetchURLFromConfig();
    void registerNotification(WPEFramework::PluginHost::IShell* service);
    void onDeviceMgtUpdateReceivedHandler(const string& source, const string& type, const bool success);

    WPEFramework::Core::Sink<SystemServicesNotification> _systemServicesNotification;
    WPEFramework::Exchange::ISystemServices* _systemServicesPlugin{nullptr};
    bool _eventRegistered{false};
    TTSConfiguration* m_defaultConfig{nullptr};
};

}
#endif
