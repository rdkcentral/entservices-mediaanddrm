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

#include <stdlib.h>
#include <errno.h>
#include <string>
#include <iomanip>
#include <iostream>
#include <algorithm>

#include "UnifiedCASManagementImplementation.h"
#ifdef LMPLAYER_FOUND
#include "LibMediaPlayerImpl.h"
#endif
#include "UtilsLogging.h"

#ifdef ENABLE_DEBUG
#define DBGINFO(fmt, ...) LOGINFO(fmt, ##__VA_ARGS__)
#else
#define DBGINFO(fmt, ...)
#endif

namespace WPEFramework {
    namespace Plugin {

        SERVICE_REGISTRATION(UnifiedCASManagementImplementation, 1, 0);

        UnifiedCASManagementImplementation* UnifiedCASManagementImplementation::_instance = nullptr;

    /* static */ UnifiedCASManagementImplementation* UnifiedCASManagementImplementation::instance(UnifiedCASManagementImplementation* unifiedCASImpl)
    {
        if (unifiedCASImpl != nullptr) {
            _instance = unifiedCASImpl;
        }
        return _instance;
    }

    UnifiedCASManagementImplementation::UnifiedCASManagementImplementation()
        : m_adminLock()
        , m_notifications()
    {
        LOGINFO("UnifiedCASManagementImplementation Constructor");

        // Set static instance
        _instance = this;

#ifdef LMPLAYER_FOUND
        m_player = std::make_shared<LibMediaPlayerImpl>(nullptr);
#else
        m_player = nullptr;
        LOGERR("NO VALID PLAYER AVAILABLE TO USE");
#endif
    }

    UnifiedCASManagementImplementation::~UnifiedCASManagementImplementation() 
    {
        LOGINFO("UnifiedCASManagementImplementation Destructor");

        if (_instance == this) {
            _instance = nullptr;
        }
    }

    /* virtual */ Core::hresult UnifiedCASManagementImplementation::Manage(
        const string& mediaurl, 
        const string& mode, 
        const string& managementType, 
        const string& casinitdata, 
        const string& casocdmid,
        Exchange::IUnifiedCASManagement::SuccessResult& successResult) /* override */
    {
        LOGINFO("Manage: mediaurl=%s, mode=%s, managementType=%s, casocdmid=%s", 
                mediaurl.c_str(), mode.c_str(), managementType.c_str(), casocdmid.c_str());
        
        Core::hresult result = Core::ERROR_NONE;
        successResult.success = false;

        m_adminLock.Lock();

        try {
            if (nullptr == m_player) {
                LOGERR("NO VALID PLAYER AVAILABLE TO USE");
            }
            else if (mode != "MODE_NONE") {
                LOGERR("mode must be MODE_NONE for CAS Management");
            }
            else if (managementType != "MANAGE_FULL" && managementType != "MANAGE_NO_PSI" && managementType != "MANAGE_NO_TUNER") {
                LOGERR("managementType must be MANAGE_FULL, MANAGE_NO_PSI or MANAGE_NO_TUNER for CAS Management");
            }
            else if (casocdmid.empty()) {
                LOGERR("ocdmcasid is mandatory for CAS management session");
            }
            else {
                // Create JSON parameters for MediaPlayer 
                JsonObject jsonParams;
                jsonParams["mediaurl"] = mediaurl;
                jsonParams["mode"] = mode;
                jsonParams["manage"] = managementType;
                jsonParams["casinitdata"] = casinitdata;
                jsonParams["casocdmid"] = casocdmid;

                std::string openParams;
                jsonParams.ToString(openParams);
                LOGINFO("OpenData = %s", openParams.c_str());

                if (false == m_player->openMediaPlayer(openParams, managementType)) {
                    LOGERR("Failed to open MediaPlayer");
                } else {
                    LOGINFO("Successfully opened MediaPlayer for CAS management");
                    successResult.success = true;
                }
            }
        } catch (const std::exception& e) {
            LOGERR("Exception in Manage: %s", e.what());
        }

        m_adminLock.Unlock();

        return result;
    }

    /* virtual */ Core::hresult UnifiedCASManagementImplementation::Unmanage(Exchange::IUnifiedCASManagement::SuccessResult& successResult) /* override */
    {
        LOGINFO("Unmanage: Destroying CAS management session");
        
        Core::hresult result = Core::ERROR_NONE;
        successResult.success = false;

        m_adminLock.Lock();

        try {
            if (nullptr == m_player) {
                LOGERR("NO VALID PLAYER AVAILABLE TO USE");
            }
            else {
                if (false == m_player->closeMediaPlayer()) {
                    LOGERR("Failed to close MediaPlayer");
                    LOGWARN("Error in destroying CAS Management Session...");
                } else {
                    LOGINFO("Successful in destroying CAS Management Session...");
                    successResult.success = true;
                }
            }
        } catch (const std::exception& e) {
            LOGERR("Exception in Unmanage: %s", e.what());
        }

        m_adminLock.Unlock();

        return result;
    }

    /* virtual */ Core::hresult UnifiedCASManagementImplementation::Send(const string& payload, 
                                                                        const string& source,
                                                                        Exchange::IUnifiedCASManagement::SuccessResult& successResult) /* override */
    {
        LOGINFO("Send: payload=%s, source=%s", payload.c_str(), source.c_str());
        
        Core::hresult result = Core::ERROR_NONE;
        successResult.success = false;

        m_adminLock.Lock();

        try {
            if (nullptr == m_player) {
                LOGERR("NO VALID PLAYER AVAILABLE TO USE");
            }
            else {
                // Create JSON parameters for MediaPlayer
                JsonObject jsonParams;
                jsonParams["payload"] = payload;
                jsonParams["source"] = source;

                std::string data;
                jsonParams.ToString(data);
                LOGINFO("Send Data = %s", data.c_str());

                if (false == m_player->requestCASData(data)) {
                    LOGERR("requestCASData failed");
                } else {
                    LOGINFO("UnifiedCASManagement send Data succeeded.. Calling Play");
                    successResult.success = true;
                }
            }
        } catch (const std::exception& e) {
            LOGERR("Exception in Send: %s", e.what());
        }

        m_adminLock.Unlock();

        return result;
    }

    // Notification management
    /* virtual */ Core::hresult UnifiedCASManagementImplementation::Register(Exchange::IUnifiedCASManagement::INotification* notification) /* override */
    {
        LOGINFO("Register notification callback");
        
        Core::hresult result = Core::ERROR_GENERAL;

        ASSERT(nullptr != notification);

        m_adminLock.Lock();

        auto it = std::find(m_notifications.begin(), m_notifications.end(), notification);
        if (it == m_notifications.end()) {
            m_notifications.push_back(notification);
            notification->AddRef();
            result = Core::ERROR_NONE;
            LOGINFO("Notification callback registered successfully");
        } else {
            result = Core::ERROR_ALREADY_CONNECTED;
            LOGWARN("Notification callback already registered");
        }

        m_adminLock.Unlock();

        return result;
    }

    /* virtual */ Core::hresult UnifiedCASManagementImplementation::Unregister(Exchange::IUnifiedCASManagement::INotification* notification) /* override */
    {
        LOGINFO("Unregister notification callback");
        
        Core::hresult result = Core::ERROR_GENERAL;

        ASSERT(nullptr != notification);

        m_adminLock.Lock();

        auto it = std::find(m_notifications.begin(), m_notifications.end(), notification);
        if (it != m_notifications.end()) {
            (*it)->Release();
            m_notifications.erase(it);
            result = Core::ERROR_NONE;
            LOGINFO("Notification callback unregistered successfully");
        } else {
            result = Core::ERROR_UNKNOWN_KEY;
            LOGWARN("Notification callback not found for unregistration");
        }

        m_adminLock.Unlock();

        return result;
    }

    // Event notification method - called by MediaPlayer or internal events
    void UnifiedCASManagementImplementation::event_data(const string& payload, const string& source)
    {
        LOGINFO("event_data: payload=%s, source=%s", payload.c_str(), source.c_str());

        m_adminLock.Lock();

        // Notify all registered COM-RPC clients
        for (auto& notification : m_notifications) {
            if (notification != nullptr) {
                notification->Event_data(payload, source);
            }
        }

        LOGINFO("Event dispatched to %zu registered clients", m_notifications.size());
        m_adminLock.Unlock();
    }

    } // namespace Plugin
} // namespace WPEFramework
