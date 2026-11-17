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

#pragma once

#include "Module.h"
#include <interfaces/Ids.h>
#include "tracing/Logging.h"

#include <interfaces/ISystemAudioPlayer.h>
#include <interfaces/IPowerManager.h>
#include "impl/AudioPlayer.h"
#include "impl/logger.h"
#include "impl/SecurityParameters.h"
#include <vector>
#include <regex>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>

#define CHECK_SAP_PARAMETER_RETURN_ON_FAIL(param) do {\
    if(!parameters.HasLabel(param)) { \
        LOGERR("Parameter \"%s\" is not found", param); \
        returnResponse(false); \
    } } while(0)

#define CHECK_SAP_PARAMETER_URL_VALID_RETURN_ON_FAIL(param) do {\
        if(param[0] == '\0') { \
            returnResponse(false); \
        } \
        std::regex uriRegex("(file:///.*|((ws|wss)://.*)|data://.*|(https?://.*)|(http://.*))"); \
        bool status = std::regex_match(param, uriRegex); \
        if(!status) { \
            returnResponse(false); \
        } \
} while(0)

#define CHECK_SAP_CONFIG_RETURN_ON_FAIL(param) do {\
    if(!config.HasLabel(param)) { \
        LOGERR("Parameter \"%s\" is not found", param); \
        returnResponse(false); \
    } } while(0)

#define getNumberConfigParameter(paramName, param) { \
    if (Core::JSON::Variant::type::NUMBER == config[paramName].Content()) \
        param = config[paramName].Number(); \
    else \
        try { param = std::stoi( config[paramName].String()); } \
        catch (...) { param = 0; } \
}

const static std::map<std::string, std::regex> patternMap = {
    {"websocket", std::regex("^(ws|wss)://.*")},
    {"data", std::regex("^data://.*")},
    {"filesrc", std::regex("^file:///.*")},
    {"httpsrc", std::regex("^(http|https)://.*")}
};

namespace WPEFramework {
namespace Plugin {

    class SystemAudioPlayerImplementation : public Exchange::ISystemAudioPlayer, public SAPEventCallback {
    public:
        enum Event {
                ONSAPEVENT,
                ONPOWERSTANDBY
            };

        // Tracks org.rdk.PowerManager availability and receives its power mode pre-change events.
        class PowerStateObserver : public PluginHost::IPlugin::INotification,
                                   public Exchange::IPowerManager::IModePreChangeNotification {
        public:
            PowerStateObserver() = delete;
            PowerStateObserver(const PowerStateObserver&) = delete;
            PowerStateObserver& operator=(const PowerStateObserver&) = delete;

            explicit PowerStateObserver(SystemAudioPlayerImplementation& parent)
                : _parent(parent) {
            }
            ~PowerStateObserver() = default;

#ifdef USE_THUNDER_R4
            void Activated(const string& callsign, PluginHost::IShell* /* plugin */) override {
                _parent.OnPluginActivated(callsign);
            }
            void Deactivated(const string& callsign, PluginHost::IShell* /* plugin */) override {
                _parent.OnPluginDeactivated(callsign);
            }
            void Unavailable(const string& callsign, PluginHost::IShell* /* plugin */) override {
                _parent.OnPluginDeactivated(callsign);
            }
#else
            void StateChange(PluginHost::IShell* plugin) override {
                if (plugin != nullptr) {
                    const PluginHost::IShell::state state = plugin->State();
                    if (state == PluginHost::IShell::ACTIVATED) {
                        _parent.OnPluginActivated(plugin->Callsign());
                    } else if (state == PluginHost::IShell::DEACTIVATION) {
                        _parent.OnPluginDeactivated(plugin->Callsign());
                    }
                }
            }
#endif /* USE_THUNDER_R4 */

            void OnPowerModePreChange(const Exchange::IPowerManager::PowerState currentState,
                                     const Exchange::IPowerManager::PowerState newState,
                                     const int transactionId, const int stateChangeAfter) override {
                _parent.OnPowerModePreChange(currentState, newState, transactionId, stateChangeAfter);
            }

            BEGIN_INTERFACE_MAP(PowerStateObserver)
            INTERFACE_ENTRY(PluginHost::IPlugin::INotification)
            INTERFACE_ENTRY(Exchange::IPowerManager::IModePreChangeNotification)
            END_INTERFACE_MAP

        private:
            SystemAudioPlayerImplementation& _parent;
        };

        class EXTERNAL Job : public Core::IDispatch {
        protected:
             Job(SystemAudioPlayerImplementation *sap, Event event, string &data,
                 const int transactionId, const uint64_t powerRequestToken)
                : _sap(sap)
                , _event(event)
                , _data(data)
                , _transactionId(transactionId)
                , _powerRequestToken(powerRequestToken) {
                if (_sap != nullptr) {
                    _sap->AddRef();
                }
            }

       public:
            Job() = delete;
            Job(const Job&) = delete;
            Job& operator=(const Job&) = delete;
            ~Job() {
                if (_sap != nullptr) {
                    _sap->Release();
                }
            }

       public:
            static Core::ProxyType<Core::IDispatch> Create(SystemAudioPlayerImplementation *sap, Event event, string data,
                                                        const int transactionId = 0, const uint64_t powerRequestToken = 0) {
#ifndef USE_THUNDER_R4
                return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(sap, event, data, transactionId, powerRequestToken)));
#else
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(sap, event, data, transactionId, powerRequestToken)));
#endif /* USE_THUNDER_R4 */
            }

            virtual void Dispatch() {
                _sap->Dispatch(_event, _data, _transactionId, _powerRequestToken);
            }

        private:
            SystemAudioPlayerImplementation *_sap;
            const Event _event;
            const string _data;
            const int _transactionId;
            const uint64_t _powerRequestToken;
        };

    public:
        // We do not allow this plugin to be copied !!
        SystemAudioPlayerImplementation(const SystemAudioPlayerImplementation&) = delete;
        SystemAudioPlayerImplementation& operator=(const SystemAudioPlayerImplementation&) = delete;

        // Starts monitoring; Configure(nullptr) stops it before release. The destructor also cleans up.
        virtual Core::hresult Configure(PluginHost::IShell* service);
        virtual Core::hresult Register(INotification* sink) override ;
        virtual Core::hresult Unregister(INotification* sink) override ;

        virtual Core::hresult Open(const string &input, string &output /* @out */) override ;
        virtual Core::hresult Play(const string &input, string &output /* @out */) override ;
        virtual Core::hresult PlayBuffer(const string &input, string &output /* @out */) override ;
        virtual Core::hresult Pause(const string &input, string &output /* @out */) override ;
        virtual Core::hresult Resume(const string &input, string &output /* @out */) override ;
        virtual Core::hresult Stop(const string &input, string &output /* @out */) override ;
        virtual Core::hresult Close(const string &input, string &output /* @out */) override ;
        virtual Core::hresult SetMixerLevels(const string &input, string &output /* @out */) override ;
        virtual Core::hresult SetSmartVolControl(const string &input, string &output /* @out */) override ;
        virtual Core::hresult IsPlaying(const string &input, string &output /* @out */) override ;
        virtual Core::hresult Config(const string &input, string &output /* @out */) override ;
        virtual Core::hresult GetPlayerSessionId(const string &input, string &output /* @out */) override ;

        virtual void onSAPEvent(uint32_t id,std::string message) override; 
      
        BEGIN_INTERFACE_MAP(SystemAudioPlayerImplementation)
        INTERFACE_ENTRY(Exchange::ISystemAudioPlayer)
        END_INTERFACE_MAP

    private:       
        std::map<int, AudioPlayer*> objectMap;
        AudioPlayer* getObjectFromMap(int key);
        mutable Core::CriticalSection _adminLock;
        std::list<Exchange::ISystemAudioPlayer::INotification*> _notificationClients;

        void dispatchEvent(Event, JsonObject &params);
        void Dispatch(Event event, string data, int transactionId, uint64_t powerRequestToken);
        void OpenMapping(AudioType audioType,SourceType sourceType,PlayMode mode,int &playerid);
        bool GetSessionFromUrl(string url,int &playerid);
        bool SameModeNotPlaying(AudioPlayer*,int &playerid);
        bool CloseMapping(int key);
        impl::SecurityParameters extractSecurityParams(const JsonObject& params) const;

        void OnPluginActivated(const string& callsign);
        void OnPluginDeactivated(const string& callsign);
        void OnPowerModePreChange(const Exchange::IPowerManager::PowerState currentState,
                                  const Exchange::IPowerManager::PowerState newState,
                                  int transactionId, int stateChangeAfter);
        void ConnectPowerManager();
        void DisconnectPowerManager();
        void ReleasePowerMonitoring();
        void StopActivePlayers();
        void CompletePowerModePreChange(int transactionId, uint64_t powerRequestToken);
        void RequestPowerModeChangeDelay(int transactionId, uint64_t powerRequestToken, int delaySec);
        void PowerDelayMonitor(int transactionId, uint64_t powerRequestToken);
        void StopPowerDelayMonitor();

        mutable Core::CriticalSection _powerLock;
        PluginHost::IShell* _service;
        Exchange::IPowerManager* _powerManager;
        Core::Sink<PowerStateObserver> _powerStateObserver;
        uint32_t _powerPreChangeClientId{0};
        // Incremented for each pre-change or disconnect to invalidate older queued work.
        uint64_t _powerRequestToken{0};
        std::mutex _powerDelayControlMutex;
        std::mutex _powerDelayMutex;
        std::condition_variable _powerDelayCv;
        std::thread _powerDelayThread;
        bool _powerDelayDone{true};
        std::mutex _playbackLock;
        std::atomic<bool> _playbackAllowed{true};

    public:
        SystemAudioPlayerImplementation();
        virtual ~SystemAudioPlayerImplementation();        
        friend class Job;
    };

} // namespace Plugin
} // namespace WPEFramework
