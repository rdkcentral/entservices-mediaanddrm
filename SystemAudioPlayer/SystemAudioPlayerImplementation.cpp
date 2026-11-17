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

#include "SystemAudioPlayerImplementation.h"
#include <sys/prctl.h>
#include "impl/Helper.h"
#include "base64.h"
#include "UtilsJsonRpc.h"
#include <algorithm>
#include <chrono>

#define SAP_MAJOR_VERSION 1
#define SAP_MINOR_VERSION 0

#define POWERMANAGER_CALLSIGN "org.rdk.PowerManager"
#define PLAYBACK_STOPPED_POWER_STATE_CHANGE "PLAYBACK_STOPPED_POWER_STATE_CHANGE"
#define POWER_PRECHANGE_CLIENT_NAME "SystemAudioPlayer"
#define DEEP_SLEEP_TEARDOWN_BUDGET_SEC 5
#define DEEP_SLEEP_DELAY_RENEW_MARGIN_SEC 1
#define DEEP_SLEEP_DELAY_EXTENSION_SEC 2
#define DEEP_SLEEP_MAX_DELAY_SEC 30

#define GET_STR(map, key, def) ((map.HasLabel(key) && !map[key].String().empty() && map[key].String() != "null") ? map[key].String() : def)
#define CONVERT_PARAMETERS_TOJSON() JsonObject parameters, response; parameters.FromString(input);
#define CONVERT_PARAMETERS_FROMJSON() response.ToString(output);

#undef returnResponse
#define returnResponse(success) \
    response["success"] = success; \
    CONVERT_PARAMETERS_FROMJSON(); \
    LOGTRACEMETHODFIN(); \
    return (Core::ERROR_NONE);

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(SystemAudioPlayerImplementation, SAP_MAJOR_VERSION, SAP_MINOR_VERSION);


    SystemAudioPlayerImplementation::SystemAudioPlayerImplementation()
        : _adminLock()
        , _powerLock()
        , _service(nullptr)
        , _powerManager(nullptr)
        , _powerStateObserver(*this)
    {
        AudioPlayer::Init(this);
        SAPLOG_INFO("SAP: SystemAudioPlayerImplementation Constructor\n");
    }

    SystemAudioPlayerImplementation::~SystemAudioPlayerImplementation()
    {
        ReleasePowerMonitoring();
        AudioPlayer::DeInit();
        SAPLOG_INFO("SAP: SystemAudioPlayerImplementation Destructor\n");
    }

    Core::hresult SystemAudioPlayerImplementation::Configure(PluginHost::IShell* service)
    {
        if (service == nullptr) {
            SAPLOG_ERROR("SAP: Cannot configure without a service\n");
            return Core::ERROR_INVALID_PARAMETER;
        }

        _powerLock.Lock();
        if (_service != nullptr) {
            _powerLock.Unlock();
            SAPLOG_ERROR("SAP: SystemAudioPlayerImplementation is already configured\n");
            return Core::ERROR_ILLEGAL_STATE;
        }
        _service = service;
        _service->AddRef();
        _powerLock.Unlock();

        // Must not hold _powerLock here: the framework may synchronously report already active plugins.
        _service->Register(&_powerStateObserver);
        ConnectPowerManager();

        return Core::ERROR_NONE;
    }

    void SystemAudioPlayerImplementation::ReleasePowerMonitoring()
    {
        PluginHost::IShell* service = nullptr;

        _powerLock.Lock();
        service = _service;
        _service = nullptr;
        _powerLock.Unlock();

        if (service != nullptr) {
            service->Unregister(&_powerStateObserver);
            DisconnectPowerManager();
            service->Release();
            SAPLOG_INFO("SAP: Power state monitoring released\n");
        }
    }

    void SystemAudioPlayerImplementation::OnPluginActivated(const string& callsign)
    {
        if (callsign == POWERMANAGER_CALLSIGN) {
            SAPLOG_INFO("SAP: %s activated\n", callsign.c_str());
            ConnectPowerManager();
        }
    }

    void SystemAudioPlayerImplementation::OnPluginDeactivated(const string& callsign)
    {
        if (callsign == POWERMANAGER_CALLSIGN) {
            SAPLOG_INFO("SAP: %s deactivated\n", callsign.c_str());
            DisconnectPowerManager();
        }
    }

    void SystemAudioPlayerImplementation::ConnectPowerManager()
    {
        std::lock_guard<std::mutex> control(_powerDelayControlMutex);
        _powerLock.Lock();
        if ((_service != nullptr) && (_powerManager == nullptr)) {
            Exchange::IPowerManager* powerManager = _service->QueryInterfaceByCallsign<Exchange::IPowerManager>(POWERMANAGER_CALLSIGN);
            if (powerManager != nullptr) {
                if (powerManager->Register(&_powerStateObserver) == Core::ERROR_NONE) {
                    uint32_t clientId = 0;
                    const Core::hresult result = powerManager->AddPowerModePreChangeClient(POWER_PRECHANGE_CLIENT_NAME, clientId);
                    if ((result == Core::ERROR_NONE) && (clientId != 0)) {
                        _powerManager = powerManager;
                        _powerPreChangeClientId = clientId;
                        SAPLOG_INFO("SAP: Registered power mode pre-change client %u\n", clientId);
                    } else {
                        SAPLOG_ERROR("SAP: AddPowerModePreChangeClient failed: %u, client id %u\n", result, clientId);
                        const Core::hresult unregisterResult = powerManager->Unregister(&_powerStateObserver);
                        if (unregisterResult != Core::ERROR_NONE) {
                            SAPLOG_ERROR("SAP: Failed to unregister power mode pre-change notifications: %u\n", unregisterResult);
                        }
                        powerManager->Release();
                    }
                } else {
                    SAPLOG_ERROR("SAP: Failed to register for power mode pre-change notifications\n");
                    powerManager->Release();
                }
            } else {
                SAPLOG_WARNING("SAP: %s not available, power mode pre-changes will not be handled yet\n", POWERMANAGER_CALLSIGN);
            }
        }
        _powerLock.Unlock();
    }

    void SystemAudioPlayerImplementation::DisconnectPowerManager()
    {
        Exchange::IPowerManager* powerManager = nullptr;
        uint32_t clientId = 0;

        {
            std::lock_guard<std::mutex> control(_powerDelayControlMutex);
            StopPowerDelayMonitor();
            _powerLock.Lock();
            powerManager = _powerManager;
            clientId = _powerPreChangeClientId;
            _powerManager = nullptr;
            _powerPreChangeClientId = 0;
            ++_powerRequestToken;
            _powerLock.Unlock();
        }

        if (powerManager != nullptr) {
            const Core::hresult removeResult = powerManager->RemovePowerModePreChangeClient(clientId);
            if (removeResult != Core::ERROR_NONE) {
                SAPLOG_ERROR("SAP: RemovePowerModePreChangeClient failed: %u\n", removeResult);
            }
            const Core::hresult unregisterResult = powerManager->Unregister(&_powerStateObserver);
            if (unregisterResult != Core::ERROR_NONE) {
                SAPLOG_ERROR("SAP: Failed to unregister power mode pre-change notifications: %u\n", unregisterResult);
            }
            powerManager->Release();
            SAPLOG_INFO("SAP: Unregistered from power mode pre-change notifications\n");
        }
    }

    void SystemAudioPlayerImplementation::OnPowerModePreChange(const Exchange::IPowerManager::PowerState currentState,
                                                              const Exchange::IPowerManager::PowerState newState,
                                                              int transactionId, int stateChangeAfter)
    {
        SAPLOG_INFO("SAP: Power mode pre-change from %d to %d, transaction %d, stateChangeAfter %d\n",
                    static_cast<int>(currentState), static_cast<int>(newState), transactionId, stateChangeAfter);

        uint64_t powerRequestToken;
        {
            std::lock_guard<std::mutex> control(_powerDelayControlMutex);
            StopPowerDelayMonitor();
            _powerLock.Lock();
            if (_powerManager == nullptr) {
                _powerLock.Unlock();
                SAPLOG_WARNING("SAP: Ignoring power mode pre-change after PowerManager disconnected\n");
                return;
            }
            powerRequestToken = ++_powerRequestToken;
            _powerLock.Unlock();

            switch (newState) {
            case Exchange::IPowerManager::POWER_STATE_ON:
                _playbackAllowed = true;
                break;
            case Exchange::IPowerManager::POWER_STATE_OFF:
            case Exchange::IPowerManager::POWER_STATE_STANDBY:
            case Exchange::IPowerManager::POWER_STATE_STANDBY_LIGHT_SLEEP:
            case Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP:
                _playbackAllowed = false;
                break;
            default:
                break;
            }

            if ((newState == Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP) ||
                (newState == Exchange::IPowerManager::POWER_STATE_OFF)) {
                RequestPowerModeChangeDelay(transactionId, powerRequestToken, DEEP_SLEEP_TEARDOWN_BUDGET_SEC);
                {
                    std::lock_guard<std::mutex> lock(_powerDelayMutex);
                    _powerDelayDone = false;
                }
                _powerDelayThread = std::thread(&SystemAudioPlayerImplementation::PowerDelayMonitor, this, transactionId, powerRequestToken);
            }
        }
        switch (newState) {
        case Exchange::IPowerManager::POWER_STATE_OFF:
        case Exchange::IPowerManager::POWER_STATE_STANDBY:
        case Exchange::IPowerManager::POWER_STATE_STANDBY_LIGHT_SLEEP:
        case Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP:
            // Stopping pipelines can block; do it off the notification thread.
            Core::IWorkerPool::Instance().Submit(Job::Create(this, ONPOWERSTANDBY, std::to_string(static_cast<int>(newState)),
                                                           transactionId, powerRequestToken));
            break;
        default:
            CompletePowerModePreChange(transactionId, powerRequestToken);
            break;
        }
    }

    void SystemAudioPlayerImplementation::RequestPowerModeChangeDelay(int transactionId, uint64_t powerRequestToken, int delaySec)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_powerLock);
        if ((_powerManager != nullptr) && (_powerRequestToken == powerRequestToken)) {
            const Core::hresult result = _powerManager->DelayPowerModeChangeBy(_powerPreChangeClientId, transactionId, delaySec);
            if (result != Core::ERROR_NONE) {
                SAPLOG_ERROR("SAP: DelayPowerModeChangeBy failed for transaction %d: %u\n", transactionId, result);
            } else {
                SAPLOG_INFO("SAP: Reserved %d seconds for power pre-change transaction %d\n", delaySec, transactionId);
            }
        }
    }

    void SystemAudioPlayerImplementation::PowerDelayMonitor(int transactionId, uint64_t powerRequestToken)
    {
        const auto start = std::chrono::steady_clock::now();
        int reservedSec = DEEP_SLEEP_TEARDOWN_BUDGET_SEC;
        std::unique_lock<std::mutex> lock(_powerDelayMutex);
        while (!_powerDelayCv.wait_until(lock, start + std::chrono::seconds(reservedSec - DEEP_SLEEP_DELAY_RENEW_MARGIN_SEC),
                                        [this] { return _powerDelayDone; })) {
            if (reservedSec >= DEEP_SLEEP_MAX_DELAY_SEC) {
                SAPLOG_ERROR("SAP: Shutdown still pending with a %d-second delay budget for transaction %d; no further delay requested\n",
                             DEEP_SLEEP_MAX_DELAY_SEC, transactionId);
                break;
            }
            const int extensionSec = std::min(DEEP_SLEEP_DELAY_EXTENSION_SEC, DEEP_SLEEP_MAX_DELAY_SEC - reservedSec);
            SAPLOG_WARNING("SAP: Shutdown still pending for transaction %d; extending reservation by %d seconds\n",
                           transactionId, extensionSec);
            lock.unlock();
            // PowerManager measures each delay from now, not from the previous deadline.
            RequestPowerModeChangeDelay(transactionId, powerRequestToken, DEEP_SLEEP_DELAY_RENEW_MARGIN_SEC + extensionSec);
            lock.lock();
            reservedSec += extensionSec;
        }
    }

    // Caller holds _powerDelayControlMutex, never _powerLock: the monitor may be requesting a delay.
    void SystemAudioPlayerImplementation::StopPowerDelayMonitor()
    {
        if (_powerDelayThread.joinable()) {
            {
                std::lock_guard<std::mutex> lock(_powerDelayMutex);
                _powerDelayDone = true;
            }
            _powerDelayCv.notify_all();
            _powerDelayThread.join();
        }
    }

    void SystemAudioPlayerImplementation::CompletePowerModePreChange(int transactionId, uint64_t powerRequestToken)
    {
        std::lock_guard<std::mutex> control(_powerDelayControlMutex);
        if (_powerRequestToken != powerRequestToken) {
            return;
        }
        StopPowerDelayMonitor();
        Core::SafeSyncType<Core::CriticalSection> lock(_powerLock);
        if (_powerManager != nullptr) {
            const Core::hresult result = _powerManager->PowerModePreChangeComplete(_powerPreChangeClientId, transactionId);
            if (result != Core::ERROR_NONE) {
                SAPLOG_ERROR("SAP: PowerModePreChangeComplete failed for transaction %d: %u\n", transactionId, result);
            } else {
                SAPLOG_INFO("SAP: Completed power mode pre-change transaction %d\n", transactionId);
            }
        }
    }

    void SystemAudioPlayerImplementation::StopActivePlayers()
    {
        std::vector<int> stoppedIds;

        _adminLock.Lock();
        for (auto& entry : objectMap) {
            AudioPlayer* player = entry.second;
            if ((player != nullptr) && player->isActive()) {
                SAPLOG_INFO("SAP: Stopping player id %d due to power state change\n", entry.first);
                player->Stop();
                stoppedIds.push_back(entry.first);
            }
        }
        _adminLock.Unlock();

        for (int id : stoppedIds) {
            onSAPEvent(static_cast<uint32_t>(id), PLAYBACK_STOPPED_POWER_STATE_CHANGE);
        }
    }

    Core::hresult SystemAudioPlayerImplementation::Register(Exchange::ISystemAudioPlayer::INotification* sink)
    {
        _adminLock.Lock();

        // Make sure a sink is not registered multiple times.
        ASSERT(std::find(_notificationClients.begin(), _notificationClients.end(), sink) == _notificationClients.end());

        _notificationClients.push_back(sink);
        sink->AddRef();

        _adminLock.Unlock();

        TRACE_L1("Registered a sink on the browser %p", sink);
        return Core::ERROR_NONE;
    }

    Core::hresult SystemAudioPlayerImplementation::Unregister(Exchange::ISystemAudioPlayer::INotification* sink)
    {
        _adminLock.Lock();

        std::list<Exchange::ISystemAudioPlayer::INotification*>::iterator index(std::find(_notificationClients.begin(), _notificationClients.end(), sink));

        // Make sure you do not unregister something you did not register !!!
        ASSERT(index != _notificationClients.end());

        if (index != _notificationClients.end()) {
            (*index)->Release();
            _notificationClients.erase(index);
            TRACE_L1("Unregistered a sink on the browser %p", sink);
        }

        _adminLock.Unlock();
        return Core::ERROR_NONE;
    }

    Core::hresult SystemAudioPlayerImplementation::Open(const string &input, string &output)
    {   
        SAPLOG_INFO("SystemAudioPlayerImplementation Got Open request :%s\n",input.c_str());
        CONVERT_PARAMETERS_TOJSON();
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("audiotype");
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("sourcetype");
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("playmode");
        SAPLOG_INFO("SAP: SystemAudioPlayerImplementation Open\n");

        auto audioType = audioTypeFromString(parameters["audiotype"].String());
        if(audioType == AudioType::AudioType_None) {
            SAPLOG_INFO("SystemAudioPlayerImplementation Open Audiotype :%s is not supported", parameters["audiotype"].String().c_str());
            returnResponse(false);
        }

        auto sourceType = sourceTypeFromString(parameters["sourcetype"].String());
        if(sourceType == SourceType::SourceType_None) {
            SAPLOG_INFO("SystemAudioPlayerImplementation Open SourceType :%s is not supported", parameters["sourcetype"].String().c_str());
            returnResponse(false);
        }

        auto playMode = playModeFromString(parameters["playmode"].String());
        if(playMode == PlayMode::PlayMode_None) {
            SAPLOG_INFO("SystemAudioPlayerImplementation Open PlayMode :%s is not supported", parameters["playmode"].String().c_str());
            returnResponse(false);
        }

        int id = -1;
        _adminLock.Lock();      
        OpenMapping(audioType,sourceType,playMode,id);        
        _adminLock.Unlock();
        response["id"] = (int) id;
        returnResponse(true);
    }

    Core::hresult SystemAudioPlayerImplementation::Config(const string &input, string &output)
    {
        SAPLOG_INFO("SystemAudioPlayerImplementation Got Config request :%s\n",input.c_str());
        CONVERT_PARAMETERS_TOJSON();
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("id");
        int id = -1, rate = 0, channels = 0;
        bool ret = false;
        string format, layout;
        AudioPlayer *player;
        getNumberParameter("id", id);
        _adminLock.Lock();
         player = getObjectFromMap(id);
        _adminLock.Unlock();
        if(player != NULL)
        {
            //TODO parse pcmconfig and pass
            if( player->getAudioType() == AudioType::PCM)
            {
                CHECK_SAP_PARAMETER_RETURN_ON_FAIL("pcmconfig");
                JsonObject config = parameters["pcmconfig"].Object();
                CHECK_SAP_CONFIG_RETURN_ON_FAIL("format");
                CHECK_SAP_CONFIG_RETURN_ON_FAIL("rate");
                CHECK_SAP_CONFIG_RETURN_ON_FAIL("channels");
                CHECK_SAP_CONFIG_RETURN_ON_FAIL("layout");
                format = config["format"].String();
                layout = config["layout"].String();
                getNumberConfigParameter("rate",rate);
                getNumberConfigParameter("channels",channels);
                SAPLOG_INFO("SAP: Do PCM config ");
                ret= player->configPCMCaps(format,rate,channels,layout);
            }
            if (player->getSourceType() == SourceType::WEBSOCKET && parameters.HasLabel("websocketsecparam"))
            {
                SAPLOG_INFO("Configuring security parameters for websocket audio player.");
                player->configWsSecParams(extractSecurityParams(parameters));
            }
            returnResponse(ret);

        }
        returnResponse(false);
    }
    
    Core::hresult SystemAudioPlayerImplementation::GetPlayerSessionId(const string &input, string &output)
    {
        SAPLOG_INFO("SystemAudioPlayerImplementation Got GetPlayerSessionID request :%s\n",input.c_str());
        CONVERT_PARAMETERS_TOJSON();
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("url");
        string url;
        int playerid = -1;
        url = parameters["url"].String();
        CHECK_SAP_PARAMETER_URL_VALID_RETURN_ON_FAIL(url.c_str());
        extractFileProtocol(url); //we do not store file:// for file playback
        _adminLock.Lock();
        if(GetSessionFromUrl(url,playerid))
        {
            response["sessionId"] = (int) playerid;
        }
        else
        {
            response["sessionId"] = (int) -1;
        }
        _adminLock.Unlock();
        returnResponse(true);
    }

    Core::hresult SystemAudioPlayerImplementation::Play(const string &input, string &output)
    {
        SAPLOG_INFO("SystemAudioPlayerImplementation Got Play request :%s\n",input.c_str());
        CONVERT_PARAMETERS_TOJSON();
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("id");
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("url");
        SAPLOG_INFO("SAP: SystemAudioPlayerImplementation Play\n");
        std::lock_guard<std::mutex> playbackLock(_playbackLock);
        if (!_playbackAllowed) {
            SAPLOG_WARNING("SAP: Play rejected while power state disallows playback\n");
            returnResponse(false);
        }
        int id = -1;
        string url;
        AudioPlayer *player;
        url = parameters["url"].String(); 
        getNumberParameter("id", id);
        _adminLock.Lock();
         player = getObjectFromMap(id); 
        _adminLock.Unlock();

         if(player != NULL) {
            string sourceType = sourceTypeToString(player->getSourceType());
            if(!std::regex_match(url, patternMap.at(sourceType))) {
                SAPLOG_ERROR("SAP: SystemAudioPlayerImplementation Source %s and Url %s is different",sourceType.c_str(),url.c_str());
                returnResponse(false);
            }

            if(player->getSourceType() == SourceType::FILESRC) {
                if(!extractFileProtocol(url)) {
                    returnResponse(false);
                }
            }

            _adminLock.Lock();
            if(SameModeNotPlaying(player,id)) {
                _adminLock.Unlock();
                player->Play(url);
                returnResponse(true);
            }
            _adminLock.Unlock();
            response["message"] = "Hardware resource already acquired by session with  id "+ std::to_string(id);
            returnResponse(false);
        }
        returnResponse(false);
    }

    Core::hresult SystemAudioPlayerImplementation::PlayBuffer(const string &input, string &output)
    {
        SAPLOG_INFO("SystemAudioPlayerImplementation Got PlayBuffer request :%s\n",input.c_str());
        CONVERT_PARAMETERS_TOJSON();
        std::lock_guard<std::mutex> playbackLock(_playbackLock);
        if (!_playbackAllowed) {
            SAPLOG_WARNING("SAP: PlayBuffer rejected while power state disallows playback\n");
            returnResponse(false);
        }
        AudioPlayer *player;
        int id = -1;
        std::string data;
        LOGINFO("PlayBuffer request\n");
        getNumberParameter("id", id);
        data = parameters["data"].String();
        LOGINFO("data size %d\n",data.size());
        _adminLock.Lock();
         player = getObjectFromMap(id);
        _adminLock.Unlock();
        if(player != NULL)
        {           
            std::vector<uint8_t> dectokenVec(data.begin(), data.end());
            uint8_t *e = &dectokenVec[0];
            size_t decworkspace_size = b64_get_decoded_buffer_size(dectokenVec.size());
            uint8_t *decworkspace = new uint8_t[decworkspace_size];
            size_t decnum_chars = b64_decode(e, dectokenVec.size(),decworkspace);
            LOGINFO("decode size %d\n", decworkspace_size);
            player->PlayBuffer((const char*)decworkspace,decnum_chars);
            delete []decworkspace;
            
            returnResponse(true);
        }
        returnResponse(false);
    }

    Core::hresult SystemAudioPlayerImplementation::Stop(const string &input, string &output)
    {
        SAPLOG_INFO("SystemAudioPlayerImplementation Got Stop request :%s\n",input.c_str());
        CONVERT_PARAMETERS_TOJSON();
        AudioPlayer *player;
        int id = -1;
        getNumberParameter("id", id);
        SAPLOG_INFO("SAP: SystemAudioPlayerImplementation Stop\n");
        _adminLock.Lock();
         player = getObjectFromMap(id);
        _adminLock.Unlock();
         if(player != NULL)
        {
            player->Stop();;
            returnResponse(true);
        }
        returnResponse(false);
    }

    Core::hresult SystemAudioPlayerImplementation::Close(const string &input, string &output)
    {
        SAPLOG_INFO("SystemAudioPlayerImplementation Got Close request :%s\n",input.c_str());
        CONVERT_PARAMETERS_TOJSON();
        int id = -1;
        getNumberParameter("id", id);
        SAPLOG_INFO("SAP: SystemAudioPlayerImplementation Close\n");
        std::lock_guard<std::mutex> playbackLock(_playbackLock);
        _adminLock.Lock();
        
          if(CloseMapping(id))
        {
             _adminLock.Unlock();
            returnResponse(true);
        }
        _adminLock.Unlock();
        returnResponse(false);
    }

    uint32_t nextId() {
        static uint32_t counter = 0;

        if(counter >= 0xFFFFFFFF)
            counter = 0;

        return ++counter;
    }

    Core::hresult SystemAudioPlayerImplementation::SetMixerLevels(const string &input, string &output)
    {
        SAPLOG_INFO("SystemAudioPlayerImplementation Got SetMixerLevels request :%s\n",input.c_str());
        CONVERT_PARAMETERS_TOJSON();
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("id");
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("primaryVolume");
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("playerVolume");
        bool result = false;
        int primVol = -1;
        int thisVol = -1;
        AudioPlayer *player;
        int playerId = -1;
        getNumberParameter("id", playerId);
        getNumberParameter("primaryVolume", primVol);
        getNumberParameter("playerVolume", thisVol);
        _adminLock.Lock();
        player = getObjectFromMap(playerId);
        _adminLock.Unlock();
        if (player != NULL &&  ( primVol >= 0 && primVol <= 100 && thisVol >= 0 && thisVol <=100) )
        {
            player->SetMixerLevels(primVol, thisVol);
            result = true;
        }
        else
        {
            SAPLOG_ERROR("SAP: setMixerLevels failed Player Obj=%p primvol=%d thisVol=%d", player,primVol,thisVol);
            result = false;
        }
        returnResponse(result);
    }

    Core::hresult SystemAudioPlayerImplementation::SetSmartVolControl(const string &input, string &output)
    {
        SAPLOG_INFO("SystemAudioPlayerImplementation Got SetSmartVolControl request :%s\n",input.c_str());
        CONVERT_PARAMETERS_TOJSON();
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("id");
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("enable");
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("playerAudioLevelThreshold");
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("playerDetectTimeMs");
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("playerHoldTimeMs");
        CHECK_SAP_PARAMETER_RETURN_ON_FAIL("primaryDuckingPercent");
        bool result = false;
        double thresHold = 0.0;
        int detectTimeMs = -1;
        int holdTimeMs = -1;
        int duckPercent = -1;
        bool smartVolumeEnable = false;
        AudioPlayer *player = nullptr;
        int playerId = -1;
        getNumberParameter("id", playerId);
        getBoolParameter("enable",smartVolumeEnable);
        getFloatParameter("playerAudioLevelThreshold", thresHold);
        getNumberParameter("playerDetectTimeMs", detectTimeMs);
        getNumberParameter("playerHoldTimeMs", holdTimeMs);
        getNumberParameter("primaryDuckingPercent", duckPercent);

        SAPLOG_INFO("SAP: SetSmartVolControl Player Obj=%p isEnable=%d thresHold=%f detectTimeMs=%d holdTimeMs=%d duckPercent=%d", player,smartVolumeEnable,thresHold, detectTimeMs,holdTimeMs,duckPercent);

        _adminLock.Lock();
        player = getObjectFromMap(playerId);
        _adminLock.Unlock();
        if (player != NULL &&  ( thresHold >= 0.0 && detectTimeMs >= 0 && holdTimeMs >= 0 && duckPercent >= 0 && duckPercent <=100) )
        {
            player->SetSmartVolControl( smartVolumeEnable, thresHold, detectTimeMs, holdTimeMs, duckPercent);
            result = true;
        }
        else
        {
            SAPLOG_ERROR("SAP: SetSmartVolControl failed Player Obj=%p isActive=%d thresHold=%f detectTimeMs=%d holdTimeMs=%d duckPercent=%d", player,smartVolumeEnable, thresHold, detectTimeMs,holdTimeMs,duckPercent);
            result = false;
        }
        returnResponse(result);

    }

    Core::hresult SystemAudioPlayerImplementation::Pause(const string &input, string &output)
    {
        SAPLOG_INFO("SystemAudioPlayerImplementation Got Pause request :%s\n",input.c_str());
        CONVERT_PARAMETERS_TOJSON();
        AudioPlayer *player;
        int id = -1;
        bool ret = false;
        getNumberParameter("id", id);
        _adminLock.Lock();
        player = getObjectFromMap(id);
        _adminLock.Unlock();
        if(player != NULL)
        {
            ret = player->Pause();
        }
        returnResponse(ret);
    }

    Core::hresult SystemAudioPlayerImplementation::Resume(const string &input, string &output)
    {
        SAPLOG_INFO("SystemAudioPlayerImplementation Got Resume request :%s\n",input.c_str());
        CONVERT_PARAMETERS_TOJSON();
        std::lock_guard<std::mutex> playbackLock(_playbackLock);
        if (!_playbackAllowed) {
            SAPLOG_WARNING("SAP: Resume rejected while power state disallows playback\n");
            returnResponse(false);
        }
        AudioPlayer *player;
        int id = -1;
        bool ret = false;
        getNumberParameter("id", id);
        SAPLOG_INFO("SAP: SystemAudioPlayerImplementation Resume\n");
        _adminLock.Lock();
        player = getObjectFromMap(id);
        _adminLock.Unlock();
        if(player != NULL)
        {
            ret = player->Resume();;
        }
        returnResponse(ret);
    }

    Core::hresult SystemAudioPlayerImplementation::IsPlaying(const string &input, string &output)
    {
        SAPLOG_INFO("SystemAudioPlayerImplementation Got IsPlayingrequest :%s\n",input.c_str());
        CONVERT_PARAMETERS_TOJSON();
        AudioPlayer *player;
        int id = -1;
        bool ret = false;
        getNumberParameter("id", id);
        SAPLOG_INFO("SAP: SystemAudioPlayerImplementation IsPlaying\n");
        _adminLock.Lock();
        player = getObjectFromMap(id);
        _adminLock.Unlock();
        if(player != NULL) {
            ret = player->isPlaying();;
        }
        returnResponse(ret);
    }

    impl::SecurityParameters SystemAudioPlayerImplementation::extractSecurityParams(const JsonObject& params) const
    {
        impl::SecurityParameters output;
        const auto& secParams = params.Get("websocketsecparam").Object();
        if (secParams.HasLabel("cafilenames"))
        {
            const auto& CAFileNames = secParams.Get("cafilenames").Array();
            for (int i = 0; i < CAFileNames.Length(); ++i)
            {
                if (CAFileNames[i].Object().HasLabel("cafilename"))
                {
                    output.CAFileNames.push_back(CAFileNames[i].Object().Get("cafilename").String());
                }
                else
                {
                    SAPLOG_ERROR("Malformed CA file name entry detected at index: %d.", i);
                }
            }
        }
        if (secParams.HasLabel("certfilename"))
        {
            output.certFileName = secParams.Get("certfilename").String();
        }
        if (secParams.HasLabel("keyfilename"))
        {
            output.keyFileName = secParams.Get("keyfilename").String();
        }
        return output;
    }

    void SystemAudioPlayerImplementation::dispatchEvent(Event event, JsonObject &params)
    {
        string data;
        params.ToString(data);
        Core::IWorkerPool::Instance().Submit(Job::Create(this, event, data));
    }

    void SystemAudioPlayerImplementation::Dispatch(Event event, string data, int transactionId, uint64_t powerRequestToken)
    {
        if (event == ONPOWERSTANDBY) {
            // Wait for the full start operation, not just its individual AudioPlayer API locks.
            std::lock_guard<std::mutex> playbackLock(_playbackLock);
            {
                Core::SafeSyncType<Core::CriticalSection> lock(_powerLock);
                if ((_powerManager == nullptr) || (_powerRequestToken != powerRequestToken)) {
                    SAPLOG_WARNING("SAP: Discarding stale queued power pre-change transaction %d\n", transactionId);
                    return;
                }
            }
            SAPLOG_INFO("SAP: Handling power state %s, stopping active players\n", data.c_str());
            StopActivePlayers();
            CompletePowerModePreChange(transactionId, powerRequestToken);
            return;
        }

        _adminLock.Lock();
        std::list<Exchange::ISystemAudioPlayer::INotification*>::iterator index(_notificationClients.begin());

        while (index != _notificationClients.end()) {
            (*index)->OnSAPEvents(data);
            ++index;
        }
        _adminLock.Unlock();

    }

    void SystemAudioPlayerImplementation::onSAPEvent(uint32_t id,std::string message) 
    {
        JsonObject params;
        params["id"]  = JsonValue((int)id);
        params["event"] = message;
        dispatchEvent(ONSAPEVENT, params);
    }
    
    void SystemAudioPlayerImplementation::OpenMapping(AudioType audioType,SourceType sourceType,PlayMode mode,int &playerid)
    {
        playerid= nextId();
        AudioPlayer *obj=new AudioPlayer(audioType,sourceType,mode,playerid);
        objectMap[playerid] = obj;
        SAPLOG_INFO("SAP: SystemAudioPlayerImplementation New player created\n");
    }
   
    bool SystemAudioPlayerImplementation::GetSessionFromUrl(string url,int &playerid)
    {
        std::map<int,AudioPlayer*>::iterator it = objectMap.begin();
        while (it != objectMap.end())
        {
            AudioPlayer *iplayer = it->second;
            if((url.compare(iplayer->getUrl())) == 0)
            {
               playerid = iplayer->getObjectIdentifier();
               SAPLOG_INFO("SAP: GetSessionFromUrl url %s found in list id: %d \n",url.c_str(),playerid);
               return true;
            }
            it++;
        }
        SAPLOG_INFO("SAP: GetSessionFromUrl url %s Not found in list \n",url.c_str());
        return false;
    }

    /*
    If the same mode( app/system) player is playing already, do not allow play back for this player. Otherwise cleanup previous player's state and allow this player.
    */
    bool SystemAudioPlayerImplementation::SameModeNotPlaying(AudioPlayer *player,int &playerid)
    {
        std::map<int,AudioPlayer*>::iterator it = objectMap.begin();
        while (it != objectMap.end())
        {
            AudioPlayer *iplayer = it->second;
            if((player->getObjectIdentifier() != iplayer->getObjectIdentifier()) &&
               (player->getPlayMode() == iplayer->getPlayMode()))        
            {
                if(iplayer->isPlaying())
                {
                    SAPLOG_INFO("SystemAudioPlayerImplementation play request rejected access for id %d",player->getObjectIdentifier());
                    playerid = iplayer->getObjectIdentifier();
                    return false;
                }
                iplayer->Stop();
            }
            it++;        
        }
        SAPLOG_INFO("SystemAudioPlayerImplementation play request granded access for id %d",player->getObjectIdentifier());
        return true;
    }

    bool SystemAudioPlayerImplementation::CloseMapping(int key)
    {
        AudioPlayer *player;
        player = getObjectFromMap(key);
        if(player != NULL)
        {
            objectMap.erase(key);
            player->Stop();
            delete player;
            SAPLOG_INFO("SystemAudioPlayerImplementation closemapping success for key %d\n",key);
            return true;
        }
        SAPLOG_INFO("SystemAudioPlayerImplementation closemapping failed for key %d\n",key);
        return false;
    }

    AudioPlayer* SystemAudioPlayerImplementation::getObjectFromMap(int key)
    {
        std::map<int,AudioPlayer*>::iterator it;
        it = objectMap.find(key);
        if (it != objectMap.end())
        {
            AudioPlayer *player = it->second;
            SAPLOG_INFO("SystemAudioPlayerImplementation getObjectFromMap success for key %d\n",key);
            return player;
        }
        SAPLOG_INFO("SystemAudioPlayerImplementation getObjectFromMap failed for key %d\n",key);
        return NULL;
    }     
} // namespace Plugin
} // namespace WPEFramework
