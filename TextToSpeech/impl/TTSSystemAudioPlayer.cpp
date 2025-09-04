#include "TTSSystemAudioPlayer.h"

namespace TTS
{
    using namespace ::TTS;

    TTSSystemAudioPlayer *TTSSystemAudioPlayer::createPlayerInstance()
    {
        static TTSSystemAudioPlayer instance;
        return &instance;
    }

    TTSSystemAudioPlayer::TTSSystemAudioPlayer() :
        isPlayerOpened{false},
        isPlayerClosed{true},
        isPlayerPaused{false},
        requiredAudioType{AudioType::Audio_MP3},
        activeAudioType{AudioType::Audio_MP3},
        activePlayerId{0}
    {
        m_playerAdapter = WPEFramework::Plugin::TTS::SystemAudioPlayerAdapter::getInstance();
    }


    TTSSystemAudioPlayer::~TTSSystemAudioPlayer() 
    {
        if(m_playerAdapter)
        {
            delete m_playerAdapter;
            m_playerAdapter = nullptr;
        }
        isPlayerOpened = false;
        isPlayerClosed = true;
        activePlayerId = 0;
    }


    TTSSAP_STATUS TTSSystemAudioPlayer::openPlayer()
    {
        TTSSAP_STATUS status = TTSSAP_STATUS::SAP_Unknown;
        if(!isPlayerOpened && isPlayerClosed)
        {
            if(!activePlayerId)
                activePlayerId = m_playerAdapter->open(requiredAudioType);
            if(true)
            {
                isPlayerClosed = false;
                isPlayerOpened = true;
                isPlayerPaused = true;
                status = TTSSAP_STATUS::SAP_Success;
                TTSLOG_INFO("AudioPlayer is created with activePlayerId = %d\n",activePlayerId);

            }
            else
            {
                status = TTSSAP_STATUS::SAP_Error;
            }
        }
        return status;
    }

    TTSSAP_STATUS TTSSystemAudioPlayer::resetPlayer()
    {
        TTSSAP_STATUS status = TTSSAP_STATUS::SAP_Unknown;
        if(activePlayerId > 0 && isPlayerOpened)
        {
            bool resetStatus = m_playerAdapter->stop(activePlayerId);
            if(resetStatus)
            {
                    status = TTSSAP_STATUS::SAP_Success;
                    isPlayerPaused = true;
            }
            else
            {
                    status = TTSSAP_STATUS::SAP_Error;              
            }
        }
        return status;
    }

    TTSSAP_STATUS TTSSystemAudioPlayer::destroyPlayer()
    {
        TTSSAP_STATUS status = TTSSAP_STATUS::SAP_Unknown;
        if(isPlayerOpened && !isPlayerClosed)
        {
            if(activePlayerId > 0)
            {
                bool closeStatus = m_playerAdapter->close(activePlayerId);
                if(closeStatus)
                {
                    status = TTSSAP_STATUS::SAP_Success;
                    isPlayerClosed = true;
                    isPlayerOpened = false;
                    activePlayerId = 0;
                }
                else
                {
                    status = TTSSAP_STATUS::SAP_Error;
                }
            }
        }
        return status;
    }

    TTSSAP_STATUS TTSSystemAudioPlayer::play(std::string url, int speechId)
    {
        TTSSAP_STATUS status = TTSSAP_STATUS::SAP_Unknown;
        if(!activePlayerId)
            activePlayerId = m_playerAdapter->open(requiredAudioType);
        if(true)
        {
            //adjustPrimaryVolume(25, 100);
            bool playStatus = m_playerAdapter->play(url);
            if(playStatus)
            {
                isPlayerPaused = false;
                status = TTSSAP_STATUS::SAP_Success;
            }
            else
            {
                status = TTSSAP_STATUS::SAP_Error;
            }
        }
        //adjustPrimaryVolume(100,0);
        return status;
    }

    TTSSAP_STATUS TTSSystemAudioPlayer::pause(int speechId)
    {
        TTSSAP_STATUS status = TTSSAP_STATUS::SAP_Unknown;
        if(isPlayerOpened && activePlayerId > 0 && !isPlayerPaused)
        {
            bool pauseStatus = m_playerAdapter->pause(activePlayerId);
            if(pauseStatus)
            {
                status = TTSSAP_STATUS::SAP_Success;
                isPlayerPaused = true;
            }
            else
            {
                status = TTSSAP_STATUS::SAP_Error;
            }
        }
        return status;
    }
    TTSSAP_STATUS TTSSystemAudioPlayer::resume(int speechId)
    {
        TTSSAP_STATUS status = TTSSAP_STATUS::SAP_Unknown;
        if(isPlayerOpened && activePlayerId > 0 && isPlayerPaused)
        {
            bool resumeStatus = m_playerAdapter->resume(activePlayerId);
            if(resumeStatus)
            {
                status = TTSSAP_STATUS::SAP_Success;
                isPlayerPaused = false;
            }
            else
            {
                status = TTSSAP_STATUS::SAP_Error;
            }
        }
        return status;

    }
    TTSSAP_STATUS TTSSystemAudioPlayer::stop(int speechId)
    {
        TTSSAP_STATUS status = TTSSAP_STATUS::SAP_Unknown;
        if((isPlayerOpened && activePlayerId > 0 && !isPlayerPaused) || speechId == 0)
        {
            bool stopStatus = m_playerAdapter->stop(activePlayerId);
            if(stopStatus)
            {
                status = TTSSAP_STATUS::SAP_Success;
            }
            else
            {
                status = TTSSAP_STATUS::SAP_Error;
            }
        }
        return status;
    }

    TTSSAP_STATUS TTSSystemAudioPlayer::adjustPrimaryVolume(int newPrimaryVolume, int newPlayerVolume)
    {
        TTSSAP_STATUS status = TTSSAP_STATUS::SAP_Unknown;
        if(isPlayerOpened && activePlayerId > 0)
        {
            bool volumeStatus = m_playerAdapter->setMixerLevels(newPrimaryVolume, newPlayerVolume);
            if(volumeStatus)
            {
                status = TTSSAP_STATUS::SAP_Success;
                TTSLOG_INFO("Volume Adjusted as PrimVol = %d & PlayerVol = %d", newPrimaryVolume, newPlayerVolume);
            }
            else
            {
                status = TTSSAP_STATUS::SAP_Error;
                TTSLOG_ERROR("Player Volume Adjust Failed");
            }

        }
        return status;
    }

    void TTSSystemAudioPlayer::updatePlayerMetaDataFromURL(std::string url)
    {
        // Check if url contains endpoint on localhost, enable PCM audio
        if ((url.rfind(LOOPBACK_ENDPOINT, 0) != std::string::npos) || (url.rfind(LOCALHOST_ENDPOINT, 0) != std::string::npos))
        {
            TTSLOG_INFO("PCM audio playback is enabled");
            requiredAudioType = AudioType::Audio_PCM;
        }
        requiredAudioType = AudioType::Audio_MP3;

        if(activeAudioType != requiredAudioType)
        {
            if(isPlayerOpened)
            {
                destroyPlayer();
                openPlayer();
            }
        }
    }

}