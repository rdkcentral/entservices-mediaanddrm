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

#include "TTSSpeaker.h"
#include "TTSURLConstructer.h"
#include "NetworkStatusObserver.h"
#include "SatToken.h"
#include <unistd.h>
#include <regex>

#define INT_FROM_ENV(env, default_value) ((getenv(env) ? atoi(getenv(env)) : 0) > 0 ? atoi(getenv(env)) : default_value)
#define TTS_CONFIGURATION_STORE "/opt/persistent/tts.setting.ini"
#define UPDATE_AND_RETURN(o, n) if(o != n) { o = n; return true; }

namespace TTS {

using namespace ::TTS;

std::map<std::string, std::string> TTSConfiguration::m_others;

TTSConfiguration::TTSConfiguration() :
    m_ttsEndPoint(""),
    m_ttsEndPointSecured(""),
    m_ttsEndPointLocal(""),
    m_apiKey(""),
    m_endpointType(""),
    m_speechRate(""),
    m_satPluginCallsign(""),
    m_language("en-US"),
    m_voice(""),
    m_localVoice(""),
    m_volume(MAX_VOLUME),
    m_rate(DEFAULT_RATE),
    m_primVolDuck(25),
    m_preemptiveSpeaking(true),
    m_enabled(false),
    m_ttsRFCEnabled(false),
    m_fallbackenabled(false),
    m_validLocalEndpoint(false) { }

TTSConfiguration::~TTSConfiguration() {}

TTSConfiguration::TTSConfiguration(TTSConfiguration &config)
{
    m_ttsEndPoint = config.m_ttsEndPoint;
    m_ttsEndPointSecured = config.m_ttsEndPointSecured;
    m_ttsEndPointLocal = config.m_ttsEndPointLocal;
    m_localVoice = config.m_localVoice;
    m_language = config.m_language;
    m_apiKey = config.m_apiKey;
    m_satPluginCallsign = config.m_satPluginCallsign;
    m_endpointType = config.m_endpointType;
    m_speechRate = config.m_speechRate;
    m_voice = config.m_voice;
    m_volume = config.m_volume;
    m_rate = config.m_rate;
    m_primVolDuck = config.m_primVolDuck;
    m_enabled = config.m_enabled;
    m_ttsRFCEnabled = config.m_ttsRFCEnabled;
    m_validLocalEndpoint = config.m_validLocalEndpoint;
    m_preemptiveSpeaking = config.m_preemptiveSpeaking;
    m_data.scenario = config.m_data.scenario;
    m_data.value = config.m_data.value;
    m_data.path = config.m_data.path;
    m_fallbackenabled = config.m_fallbackenabled;
}
TTSConfiguration& TTSConfiguration::operator = (const TTSConfiguration &config)
{
    m_ttsEndPoint = config.m_ttsEndPoint;
    m_ttsEndPointSecured = config.m_ttsEndPointSecured;
    m_ttsEndPointLocal = config.m_ttsEndPointLocal;
    m_localVoice = config.m_localVoice;
    m_language = config.m_language;
    m_apiKey = config.m_apiKey;
    m_satPluginCallsign = config.m_satPluginCallsign;
    m_endpointType = config.m_endpointType;
    m_speechRate = config.m_speechRate;
    m_voice = config.m_voice;
    m_volume = config.m_volume;
    m_rate = config.m_rate;
    m_primVolDuck = config.m_primVolDuck;
    m_enabled = config.m_enabled;
    m_ttsRFCEnabled =  config.m_ttsRFCEnabled;
    m_validLocalEndpoint = config.m_validLocalEndpoint;
    m_preemptiveSpeaking = config.m_preemptiveSpeaking;
    m_data.scenario = config.m_data.scenario;
    m_data.value = config.m_data.value;
    m_data.path = config.m_data.path;
    m_fallbackenabled = config.m_fallbackenabled;
    return *this;
}

bool TTSConfiguration::setEndPoint(const std::string endpoint) {
    if(!endpoint.empty())
    {
        UPDATE_AND_RETURN(m_ttsEndPoint, endpoint);
    }
    else
        TTSLOG_VERBOSE("Invalid TTSEndPoint input \"%s\"", endpoint.c_str());
    return false;
}

bool TTSConfiguration::setSecureEndPoint(const std::string endpoint) {
    if(!endpoint.empty())
    {
        UPDATE_AND_RETURN(m_ttsEndPointSecured, endpoint);
    }
    else
        TTSLOG_VERBOSE("Invalid Secured TTSEndPoint input \"%s\"", endpoint.c_str());
    return false;
}

bool TTSConfiguration::setRFCEndPoint(const std::string endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!endpoint.empty() && endpoint.find_first_not_of(' ') != std::string::npos) {
        m_ttsRFCEnabled = true;
        setEndpointType("TTS2");
        UPDATE_AND_RETURN(m_ttsRFCEndpoint, endpoint);
    } else {
        m_ttsRFCEnabled = false;
         if (!apiKey().empty()) {
            setEndpointType("TTS1");
        }
        TTSLOG_VERBOSE("Invalid RFC TTSEndPoint input \"%s\"", endpoint.c_str());
    }
    return false;
}

bool TTSConfiguration::isRFCEnabled() {
    return m_ttsRFCEnabled;
}

bool TTSConfiguration::setLocalEndPoint(const std::string endpoint) {
    if(!endpoint.empty() && endpoint.find_first_not_of(' ') != std::string::npos) {
        m_validLocalEndpoint = true;
        UPDATE_AND_RETURN(m_ttsEndPointLocal, endpoint);
    } else {
        m_validLocalEndpoint = false;
        TTSLOG_VERBOSE("Invalid Local TTSEndPoint input \"%s\"", endpoint.c_str());
    }
    return false;
}

bool TTSConfiguration::setApiKey(const std::string apikey) {
    if(!apikey.empty())
    {
        UPDATE_AND_RETURN(m_apiKey, apikey);
    }
    else
        TTSLOG_VERBOSE("Invalid api key input \"%s\"", apikey.c_str());
    return false;
}

bool TTSConfiguration::setEndpointType(const std::string type) {
    if(!type.empty())
    {
        UPDATE_AND_RETURN(m_endpointType, type);
    }
    else
        TTSLOG_VERBOSE("Invalid EndPoint type input \"%s\"", type.c_str());
    return false;
}

bool TTSConfiguration::setSpeechRate(const std::string rate) {
    if(!rate.empty() && rate.find_first_not_of(' ') != std::string::npos)
    {
        UPDATE_AND_RETURN(m_speechRate, rate);
    }
    else
        TTSLOG_VERBOSE("Invalid Speechrate input \"%s\"", rate.c_str());
    return false;
}

bool TTSConfiguration::setLanguage(const std::string language) {
    if(!language.empty())
    {
        UPDATE_AND_RETURN(m_language, language);
    }
    else
        TTSLOG_VERBOSE("Empty Language input");
    return false;
}

bool TTSConfiguration::setVoice(const std::string voice) {
    if(!voice.empty())
    {
        UPDATE_AND_RETURN(m_voice, voice);  
    }
    else
        TTSLOG_VERBOSE("Empty Voice input");
    return false;
}

bool TTSConfiguration::setLocalVoice(const std::string voice) {
    if(!voice.empty()) {
        UPDATE_AND_RETURN(m_localVoice, voice);
    } else
        TTSLOG_VERBOSE("Empty Voice input");
    return false;
}

bool TTSConfiguration::setVolume(const double volume) {
    if(volume >= 1 && volume <= 100)
    {
        UPDATE_AND_RETURN(m_volume, volume);    
    }
    else
        TTSLOG_VERBOSE("Invalid Volume input \"%lf\"", volume);
    return false;
}

bool TTSConfiguration::setRate(const uint8_t rate) {
    if(rate >= 1 && rate <= 100)
    {
        UPDATE_AND_RETURN(m_rate, rate);    
    }
    else
        TTSLOG_VERBOSE("Invalid Rate input \"%u\"", rate);
    return false;
}

bool TTSConfiguration::setPrimVolDuck(const int8_t primvolduck) {
    if(primvolduck >= 0 && primvolduck <= 100)
    {
        UPDATE_AND_RETURN(m_primVolDuck, primvolduck);
    }
    else
        TTSLOG_VERBOSE("Invalid PrimVolDuck input \"%d\"", primvolduck);
    return false;
}

bool TTSConfiguration::setSATPluginCallsign(const std::string callsign) {
    if(!callsign.empty())
    {
        UPDATE_AND_RETURN(m_satPluginCallsign, callsign);
    }
    return false;
}

bool TTSConfiguration::setEnabled(const bool enabled) {
    UPDATE_AND_RETURN(m_enabled, enabled);
    return false;
}

void TTSConfiguration::setPreemptiveSpeak(const bool preemptive) {
    m_preemptiveSpeaking = preemptive;
}

bool TTSConfiguration::loadFromConfigStore()
{
    return WPEFramework::Plugin::_readFromFile(TTS_CONFIGURATION_STORE, *this);
}

bool TTSConfiguration::updateConfigStore()
{
    return WPEFramework::Plugin::_writeToFile(TTS_CONFIGURATION_STORE, *this);
}

const std::string TTSConfiguration::voice() {
    std::string str;

    if(!m_voice.empty())
        return m_voice;
    else {
        std::string key = std::string("voice_for_") + m_language;
        auto it = m_others.find(key);
        if(it != m_others.end())
            str = it->second;
        return str;
    }
}

const std::string TTSConfiguration::localVoice() {
    std::string str;

    if(!m_localVoice.empty())
        return m_localVoice;
    else {
        std::string key = std::string("voice_for_local_") + m_language;
        auto it = m_others.find(key);
        if(it != m_others.end())
            str = it->second;
        return str;
    }
}


bool TTSConfiguration::updateWith(TTSConfiguration &nConfig) {
    bool updated = false;
    setEndPoint(nConfig.m_ttsEndPoint);
    setSecureEndPoint(nConfig.m_ttsEndPointSecured);
    updated |= setLanguage(nConfig.m_language);
    updated |= setVoice(nConfig.m_voice);
    updated |= setVolume(nConfig.m_volume);
    updated |= setRate(nConfig.m_rate);
    return updated;
}

bool TTSConfiguration::isValid() {
    if((m_ttsEndPoint.empty() && m_ttsEndPointSecured.empty() && m_ttsRFCEndpoint.empty())) {
        TTSLOG_ERROR("TTSEndPointEmpty=%d, TTSSecuredEndPointEmpty=%d , TTSRFCEndpoint=%d",
                m_ttsEndPoint.empty(), m_ttsEndPointSecured.empty(), m_ttsRFCEndpoint.empty());
        return false;
    }
    return true;
}

bool TTSConfiguration::hasValidLocalEndpoint() {
    return m_validLocalEndpoint;
}

bool TTSConfiguration::isFallbackEnabled() {
    return m_fallbackenabled;
}

void TTSConfiguration::saveFallbackPath(std::string path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.path = path;
}

const std::string TTSConfiguration::getFallbackScenario() {
    return m_data.scenario;
}

const std::string TTSConfiguration::getFallbackPath() {
    return m_data.path;
}

const std::string TTSConfiguration::getFallbackValue() {
    return m_data.value;
}

bool TTSConfiguration::setFallBackText(FallbackData &fd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if((fd.scenario).empty() || (fd.value).empty()) {
        return false;
    } else if(fd.scenario !=  m_data.scenario || fd.value !=  m_data.value) {
        m_data.scenario = fd.scenario;
        m_data.value = fd.value;
        m_data.path = fd.path;
        m_fallbackenabled = true;
        return true;
    }
    return false;
}

TTSSpeaker::TTSSpeaker(TTSConfiguration &config) :
    m_playerError(false),
    m_networkError(false),
    m_ensurePlayer(false),
    m_remoteError(false),
    m_runThread(true),
    m_flushed(false),
    m_isSpeaking(false),
    m_isPaused(false),
    m_playerConstructionFailures(0),
    m_maxPlayerConstructionFailures(INT_FROM_ENV("MAX_PIPELINE_FAILURE_THRESHOLD", 1)),
    m_defaultConfig(config),
    m_clientSpeaking(nullptr),
    m_currentSpeech(nullptr),
    m_player(nullptr) 
    {
        std::cout << "RAJAN-DBG: Speaker Constructed" << std::endl;
        m_player = TTS::TTSSystemAudioPlayer::createPlayerInstance();
        m_gstThread = new std::thread(GStreamerThreadFunc, this);

    }

TTSSpeaker::~TTSSpeaker() 
{
        if (m_isSpeaking)
            m_flushed = true;
        m_runThread = false;
        m_condition.notify_one();

        delete m_player;
        m_player = nullptr;

        if (m_gstThread)
        {
            m_gstThread->join();
            m_gstThread = NULL;
        }
}

void TTSSpeaker::ensurePlayer(bool flag) 
{
        std::unique_lock<std::mutex> mlock(m_queueMutex);
        m_ensurePlayer = flag;
        TTSLOG_WARNING("%s", __FUNCTION__);
        m_condition.notify_one();
}

int TTSSpeaker::speak(TTSSpeakerClient *client, uint32_t id, std::string callsign, std::string text, bool secure,int8_t primVolDuck) {
    TTSLOG_TRACE("id=%d, text=\"%s\"", id, text.c_str());

    // If force speak is set, clear old queued data & stop speaking
    if(client->configuration()->isPreemptive())
        reset();

    SpeechData data(client, id, callsign, text, secure,primVolDuck);
    queueData(data);

    return 0;
}

bool TTSSpeaker::shouldUseLocalEndpoint() {
   if(m_defaultConfig.hasValidLocalEndpoint())
       return !WPEFramework::Plugin::TTS::NetworkStatusObserver::getInstance()->isConnected() || m_remoteError;
   return false;
}

SpeechState TTSSpeaker::getSpeechState(uint32_t id) {
    // See if the speech is in progress i.e Speaking / Paused
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if(m_currentSpeech && id == m_currentSpeech->id) {
            if(m_isPaused)
                return SPEECH_PAUSED;
            else
                return SPEECH_IN_PROGRESS;
        }
    }

    // Or in queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        for(auto it = m_queue.begin(); it != m_queue.end(); ++it) {
            if(it->id == id)
                return SPEECH_PENDING;
        }
    }

    return SPEECH_NOT_FOUND;
}

bool TTSSpeaker::isSpeaking(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    if(m_currentSpeech) {
        if(id == m_currentSpeech->id)
            return m_isSpeaking;
    }

    return false;
}

bool TTSSpeaker::cancelSpeech(uint32_t id) {
    TTSLOG_VERBOSE("Cancelling current speech");
    bool status = false;
    if(m_isSpeaking && m_currentSpeech && ((m_currentSpeech->id == id) || (id == 0))) {
        m_isPaused = false;
        m_flushed = true;
        status = true;
        m_condition.notify_one();
    }
    return status;
}

bool TTSSpeaker::reset() {
    TTSLOG_VERBOSE("Resetting Speaker");
    cancelSpeech();
    flushQueue();

    return true;
}

bool TTSSpeaker::pause(uint32_t id) {
    if(!m_isSpeaking || !m_currentSpeech || (id != m_currentSpeech->id))
        return false;

    if(m_player) {
        if(!m_isPaused) {
            m_isPaused = true;
            m_player->pause(0); // To be set for respective speechId
            TTSLOG_INFO("Set state to PAUSED");
            return true;
        }
    }
    return false;
}

bool TTSSpeaker::resume(uint32_t id) {
    if(!m_isSpeaking || !m_currentSpeech || (id != m_currentSpeech->id))
        return false;

    if(m_player) {
        if(m_isPaused) {
            m_player->pause(0); // To be set for respective speechId
            TTSLOG_INFO("Set state to PLAYING");
            return true;
        }
    }
    return false;
}

void TTSSpeaker::setSpeakingState(bool state, TTSSpeakerClient *client) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    m_isSpeaking = state;
    m_clientSpeaking = client;
    
    // If thread just completes speaking (called only from GStreamerThreadFunc),
    // it will take the next text from queue, no need to keep
    // m_flushed (as nothing is being spoken, which needs bail out)
    if(state == false)
        m_flushed = false;
}

void TTSSpeaker::queueData(SpeechData data) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_queue.push_back(data);
    m_condition.notify_one();
}

void TTSSpeaker::flushQueue() {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_queue.clear();
}

SpeechData TTSSpeaker::dequeueData() {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    SpeechData d;
    d = m_queue.front();
    m_queue.pop_front();
    m_flushed = false;
    return d;
}

std::string TTSSpeaker::constructURL(TTSConfiguration &config, SpeechData &d) {
        if (!config.isValid())
        {
            TTSLOG_ERROR("Invalid configuration");
            return "";
        }

        TTSURLConstructer urlConstructor;
        std::string tts_request = urlConstructor.constructURL(m_defaultConfig, d.text, false, shouldUseLocalEndpoint());
        if (m_defaultConfig.hasValidLocalEndpoint())
        {
            TTSLOG_INFO("Trying to update playermetaData\n");
            m_player->updatePlayerMetaDataFromURL(tts_request);
        }
        return tts_request;
}

void TTSSpeaker::play(string url, SpeechData &data, bool authrequired, string token) {
        m_currentSpeech = &data;
        TTSLOG_INFO("Playing the Audio with speech id = %d\n", m_currentSpeech->id);
        m_player->play(url, m_currentSpeech->id);
        //m_player->adjustPrimaryVolume(data.primVolDuck,100);
        TTSLOG_VERBOSE("Speaking.... ( %d, \"%s\")", data.id, data.text.c_str());
        m_currentSpeech = NULL;
}

void TTSSpeaker::speakText(TTSConfiguration &config, SpeechData &data) {

    if ((m_player && !m_flushed))
    {
        string token;
        bool authrequired = (config.endPointType().compare("TTS2") == 0);
        if (authrequired)
            token = WPEFramework::Plugin::TTS::SatToken::getInstance(config.satPluginCallsign())->getSAT();
        TTSLOG_INFO("Constructing URL and Trying to Play\n");
        play(constructURL(config, data), data, authrequired, token);
    }
    else
    {
        TTSLOG_WARNING("m_player=%p, m_playerError=%d", m_player, m_playerError);
    }
}


void TTSSpeaker::GStreamerThreadFunc(void *ctx) {
    TTSLOG_INFO("Starting GStreamerThread");
    TTSSpeaker *speaker = (TTSSpeaker *)ctx;

    //TTSLOG_INFO("Openening Player\n");
    //TTSSAP_STATUS playerStatus = speaker->m_player->openPlayer();
    //if(playerStatus != TTSSAP_STATUS::SAP_Success)
    //    speaker->m_playerError =  true;
    //TTSLOG_INFO("Player Opened\n");

    while (speaker && speaker->m_runThread)
    {
        if (speaker->m_ensurePlayer)
        {
            // If pipeline creation fails, send playbackerror to the client and remove the req from queue
            if (!speaker->m_player && !speaker->m_queue.empty())
            {
                SpeechData data = speaker->dequeueData();
                TTSLOG_ERROR("Pipeline creation failed, sending error for speech=%d from client %p\n", data.id, data.client);
                data.client->playbackerror(data.id, data.callsign);
                speaker->m_playerConstructionFailures = 0;
            }
        }
        else
        {
            TTSLOG_INFO("Player destroyed\n");
            speaker->m_player->destroyPlayer();
        }

        // Take an item from the queue
        TTSLOG_INFO("Waiting for text input");
        while (speaker->m_runThread && speaker->m_queue.empty())
        {
            std::unique_lock<std::mutex> mlock(speaker->m_queueMutex);
            speaker->m_condition.wait(mlock, [speaker]()
                                      { return (!speaker->m_queue.empty() || !speaker->m_runThread); });
        }

        // Stop thread on Speaker's cue
        if (!speaker->m_runThread)
        {
            if (speaker->m_player)
            {
                speaker->m_player->resetPlayer();
            }
            TTSLOG_INFO("Stopping GStreamerThread");
            return;
        }

        TTSLOG_INFO("Got text input, list size=%d", speaker->m_queue.size());
        SpeechData data = speaker->dequeueData();

        speaker->setSpeakingState(true, data.client);
        // Inform the client before speaking
        if (!speaker->m_flushed)
            data.client->willSpeak(data.id, data.callsign, data.text);

        // Push it to gstreamer for speaking
        if (!speaker->m_flushed)
        {
            speaker->speakText(*data.client->configuration(), data);
        }

        // Use Local endpoint for speaking as remote is down
        if (!speaker->m_flushed && speaker->m_remoteError)
        {
            TTSLOG_INFO("Speak with Local endpoint");
            speaker->speakText(*data.client->configuration(), data);
        }

        // when not speaking, set primary mixgain back to default.
        if (speaker->m_flushed || speaker->m_networkError || !speaker->m_player || speaker->m_playerError)
        {
            speaker->m_player->adjustPrimaryVolume(100, 0);    //adjustVolume(PrimaryVolume, PlayerVolume)
        }
        // Inform the client after speaking
        if (speaker->m_flushed)
            data.client->interrupted(data.id, data.callsign);
        else if (speaker->m_networkError)
            data.client->networkerror(data.id, data.callsign);
        else if (!speaker->m_player || speaker->m_playerError)
            data.client->playbackerror(data.id, data.callsign);
        else
        {
            speaker->m_player->adjustPrimaryVolume(100, 0);
            data.client->spoke(data.id, data.callsign, data.text);
        }
        speaker->setSpeakingState(false);
        speaker->m_player->resetPlayer();
    }

    speaker->m_player->destroyPlayer();
}


} // namespace TTS
