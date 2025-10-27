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

#include <stdlib.h>
#include <errno.h>
#include <string>
#include <iomanip>
#include <iostream>
#include <algorithm>

#include "UnifiedCASManagementImplementation.h"
#include "LibMediaPlayerImpl.h"
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

    /* virtual */ Core::hresult UnifiedCASManagementImplementation::Manage(const string& mediaurl, 
                                                                          const string& mode, 
                                                                          const string& manage, 
                                                                          const string& casinitdata, 
                                                                          const string& casocdmid) /* override */
    {
        LOGINFO("Manage: mediaurl=%s, mode=%s, manage=%s, casocdmid=%s", 
                mediaurl.c_str(), mode.c_str(), manage.c_str(), casocdmid.c_str());
        
        Core::hresult result = Core::ERROR_GENERAL;

        m_adminLock.Lock();

        try {
            if (nullptr == m_player) {
                LOGERR("NO VALID PLAYER AVAILABLE TO USE");
                result = Core::ERROR_UNAVAILABLE;
            }
            else if (mode != "MODE_NONE") {
                LOGERR("mode must be MODE_NONE for CAS Management");
                result = Core::ERROR_BAD_REQUEST;
            }
            else if (manage != "MANAGE_FULL" && manage != "MANAGE_NO_PSI" && manage != "MANAGE_NO_TUNER") {
                LOGERR("manage must be MANAGE_ ... FULL, NO_PSI or NO_TUNER for CAS Management");
                result = Core::ERROR_BAD_REQUEST;
            }
            else if (casocdmid.empty()) {
                LOGERR("ocdmcasid is mandatory for CAS management session");
                result = Core::ERROR_BAD_REQUEST;
            }
            else {
                // Create JSON parameters for MediaPlayer - same as original JSON-RPC implementation
                JsonObject jsonParams;
                jsonParams["mediaurl"] = mediaurl;
                jsonParams["mode"] = mode;
                jsonParams["manage"] = manage;
                jsonParams["casinitdata"] = casinitdata;
                jsonParams["casocdmid"] = casocdmid;

                std::string openParams;
                jsonParams.ToString(openParams);
                LOGINFO("OpenData = %s", openParams.c_str());

                if (false == m_player->openMediaPlayer(openParams, manage)) {
                    LOGERR("Failed to open MediaPlayer");
                    result = Core::ERROR_GENERAL;
                } else {
                    LOGINFO("Successfully opened MediaPlayer for CAS management");
                    result = Core::ERROR_NONE;
                }
            }
        } catch (const std::exception& e) {
            LOGERR("Exception in Manage: %s", e.what());
            result = Core::ERROR_GENERAL;
        }

        m_adminLock.Unlock();

        return result;
    }

    /* virtual */ Core::hresult UnifiedCASManagementImplementation::Unmanage() /* override */
    {
        LOGINFO("Unmanage: Destroying CAS management session");
        
        Core::hresult result = Core::ERROR_GENERAL;

        m_adminLock.Lock();

        try {
            if (nullptr == m_player) {
                LOGERR("NO VALID PLAYER AVAILABLE TO USE");
                result = Core::ERROR_UNAVAILABLE;
            }
            else {
                if (false == m_player->closeMediaPlayer()) {
                    LOGERR("Failed to close MediaPlayer");
                    LOGWARN("Error in destroying CAS Management Session...");
                    result = Core::ERROR_GENERAL;
                } else {
                    LOGINFO("Successful in destroying CAS Management Session...");
                    result = Core::ERROR_NONE;
                }
            }
        } catch (const std::exception& e) {
            LOGERR("Exception in Unmanage: %s", e.what());
            result = Core::ERROR_GENERAL;
        }

        m_adminLock.Unlock();

        return result;
    }

    /* virtual */ Core::hresult UnifiedCASManagementImplementation::Send(const string& payload, 
                                                                        const string& source) /* override */
    {
        LOGINFO("Send: payload=%s, source=%s", payload.c_str(), source.c_str());
        
        Core::hresult result = Core::ERROR_GENERAL;

        m_adminLock.Lock();

        try {
            if (nullptr == m_player) {
                LOGERR("NO VALID PLAYER AVAILABLE TO USE");
                result = Core::ERROR_UNAVAILABLE;
            }
            else {
                // Create JSON parameters for MediaPlayer - same as original JSON-RPC implementation
                JsonObject jsonParams;
                jsonParams["payload"] = payload;
                jsonParams["source"] = source;

                std::string data;
                jsonParams.ToString(data);
                LOGINFO("Send Data = %s", data.c_str());

                if (false == m_player->requestCASData(data)) {
                    LOGERR("requestCASData failed");
                    result = Core::ERROR_GENERAL;
                } else {
                    LOGINFO("UnifiedCASManagement send Data succeeded.. Calling Play");
                    result = Core::ERROR_NONE;
                }
            }
        } catch (const std::exception& e) {
            LOGERR("Exception in Send: %s", e.what());
            result = Core::ERROR_GENERAL;
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
    void UnifiedCASManagementImplementation::Event_data(const string& payload, const string& source)
    {
        LOGINFO("Event_data: payload=%s, source=%s", payload.c_str(), source.c_str());

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
