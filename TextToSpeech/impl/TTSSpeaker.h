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

#ifndef _TTS_SPEAKER_H_
#define _TTS_SPEAKER_H_

#include <map>
#include <list>
#include <mutex>
#include <thread>
#include <vector>
#include <condition_variable>

#include "TTSConfiguration.h"
#include "TTSSystemAudioPlayer.h"

namespace TTS {

#define DEFAULT_RATE  50
#define DEFAULT_WPM 200
#define MAX_VOLUME 100

class TTSSpeakerClient {
public:
    virtual TTSConfiguration* configuration() = 0;
    virtual void willSpeak(uint32_t speech_id, std::string callsign, std::string text) = 0;
    virtual void started(uint32_t speech_id, std::string callsign, std::string text) = 0;
    virtual void spoke(uint32_t speech_id, std::string callsign, std::string text) = 0;
    virtual void paused(uint32_t speech_id, std::string callsign) = 0;
    virtual void resumed(uint32_t speech_id, std::string callsign) = 0;
    virtual void cancelled(std::vector<uint32_t> &speeches, std::string callsign) = 0;
    virtual void interrupted(uint32_t speech_id, std::string callsign) = 0;
    virtual void networkerror(uint32_t speech_id, std::string callsign) = 0;
    virtual void playbackerror(uint32_t speech_id, std::string callsign) = 0;
};

struct SpeechData {
    public:
        SpeechData() : client(NULL), secure(false), id(0), callsign(), text(), primVolDuck(25) {}
        SpeechData(TTSSpeakerClient *c, uint32_t i, std::string callsign,std::string t, bool s=false,int8_t vol=25) : client(c), secure(s), id(i), callsign(callsign), text(t), primVolDuck(vol) {}
        SpeechData(const SpeechData &n) {
            client = n.client;
            id = n.id;
            callsign =n.callsign;
            text = n.text;
            secure = n.secure;
            primVolDuck = n.primVolDuck;
        }
        ~SpeechData() {}

        TTSSpeakerClient *client;
        bool secure;
        uint32_t id;
        std::string callsign;
        std::string text;
        int8_t primVolDuck;
};

class TTSSpeaker {
public:
    TTSSpeaker(TTSConfiguration &config);
    ~TTSSpeaker();

    void ensurePlayer(bool flag=true);

    // Speak Functions
    int speak(TTSSpeakerClient* client, uint32_t id, std::string callsign, std::string text, bool secure,int8_t primVolDuck); // Formalize data to speak API
    bool isSpeaking(uint32_t id);
    SpeechState getSpeechState(uint32_t id);
    bool cancelSpeech(uint32_t id=0);
    bool reset();

    bool pause(uint32_t id = 0);
    bool resume(uint32_t id = 0);
private:

    // Private Data
    void queueData(SpeechData);
    void flushQueue();
    SpeechData dequeueData();

    // Private functions
    inline void setSpeakingState(bool state, TTSSpeakerClient *client=NULL);
    static void GStreamerThreadFunc(void *ctx);
    std::string constructURL(TTSConfiguration &config, SpeechData &d);
    void speakText(TTSConfiguration &config, SpeechData &data);
    bool shouldUseLocalEndpoint();
    void play(string url, SpeechData &data, bool authrequired, string token);

    bool m_playerError;
    bool m_networkError;
    bool m_ensurePlayer;
    bool m_remoteError;
    bool m_runThread;
    bool m_flushed;
    bool m_isSpeaking;
    bool m_isPaused;
    uint8_t m_playerConstructionFailures;
    const uint8_t m_maxPlayerConstructionFailures;
    TTSConfiguration &m_defaultConfig;
    TTSSpeakerClient *m_clientSpeaking;
    SpeechData *m_currentSpeech;
    TTSSystemAudioPlayer *m_player;


    std::mutex m_stateMutex;
    std::condition_variable m_condition;
    
    std::mutex m_queueMutex;
    std::list<SpeechData> m_queue;

    std::thread *m_gstThread;

};

} // namespace TTS

namespace WPEFramework {
namespace Plugin {
bool _readFromFile(std::string filename, ::TTS::TTSConfiguration &ttsConfig);
bool _writeToFile(std::string filename, ::TTS::TTSConfiguration &ttsConfig);
}//namespace Plugin
}//namespace WPEFramework

#endif
