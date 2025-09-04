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

#pragma once

#include "TTSCommon.h"

enum AudioType
{
    Audio_MP3,
    Audio_PCM
};

enum TTSSAP_STATUS
{
    SAP_Error,
    SAP_Success,
    SAP_Unknown
};

namespace WPEFramework
{
    namespace Plugin
    {
        namespace TTS
        {
            using namespace ::TTS;
            class SystemAudioPlayerAdapter
            {
            public:
                virtual ~SystemAudioPlayerAdapter();
                static SystemAudioPlayerAdapter *getInstance();

                bool close(int playerId);
                void config();
                int getPlayerSessionId();
                bool isSpeaking();
                int open(AudioType audioType);
                bool pause(int playerId);
                bool play(std::string url);
                void playBuffer();
                bool resume(int playerId);
                bool setMixerLevels(int newPrimaryVolume, int newPlayerVolume);
                void setSmartVolControl();
                bool stop(int playerId);

            private:
                SystemAudioPlayerAdapter();
                SystemAudioPlayerAdapter(const SystemAudioPlayerAdapter &) = delete;
                SystemAudioPlayerAdapter &operator=(const SystemAudioPlayerAdapter &) = delete;

                string getSecurityToken();
                void onSAPEventHandler(const JsonObject &parameters);

                WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement> *m_systemAudioPlayerService{nullptr};
                bool m_eventRegistered{false};
                int currentPlayerId{0};
                std::string currentUrl;
            };

        }
    }
}
