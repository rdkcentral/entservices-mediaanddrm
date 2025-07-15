#pragma once

#include "SystemAudioPlayerAdapter.h"
#include <string>

#define LOOPBACK_ENDPOINT "http://127.0.0.1:50050/"
#define LOCALHOST_ENDPOINT "http://localhost:50050/"


namespace TTS
{

    class SpeechPlayerMap
    {
        public:
            SpeechPlayerMap() = default;
            virtual ~SpeechPlayerMap() = default;
            void associateSpeechWithPlayer(int playerId, int speechId);

        private:

    };

    class TTSSystemAudioPlayer
    {
        public:
            ~TTSSystemAudioPlayer();
            static TTSSystemAudioPlayer* createPlayerInstance();
            TTSSAP_STATUS openPlayer();
            TTSSAP_STATUS resetPlayer();
            TTSSAP_STATUS destroyPlayer();

            TTSSAP_STATUS play(std::string url, int speechId);
            TTSSAP_STATUS pause(int speechId);
            TTSSAP_STATUS resume(int speechId);
            TTSSAP_STATUS stop(int speechId);

            TTSSAP_STATUS adjustPrimaryVolume(int newPrimaryVolume, int newPlayerVolume);
            void updatePlayerMetaDataFromURL(std::string URL);
            
        private:
            TTSSystemAudioPlayer();
            AudioType detectAudioTypefromURL(std::string url);
            void setPlayerConfiguration();

            WPEFramework::Plugin::TTS::SystemAudioPlayerAdapter* m_playerAdapter;
            bool isPlayerOpened;
            bool isPlayerClosed;
            bool isPlayerPaused;
            AudioType requiredAudioType;
            AudioType activeAudioType;
            int activePlayerId;

    };
}