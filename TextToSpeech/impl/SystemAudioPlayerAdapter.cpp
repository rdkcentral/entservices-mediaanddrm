#include "SystemAudioPlayerAdapter.h"

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
#include "SystemAudioPlayerAdapter.h"

#if defined(SECURITY_TOKEN_ENABLED) && ((SECURITY_TOKEN_ENABLED == 0) || (SECURITY_TOKEN_ENABLED == false))
#define GetSecurityToken(a, b) 0
#define GetToken(a, b, c) 0
#else
#include <WPEFramework/securityagent/securityagent.h>
#include <WPEFramework/securityagent/SecurityTokenUtil.h>
#endif

#define MAX_SECURITY_TOKEN_SIZE 1024
#define SAP_CALLSIGN "org.rdk.SystemAudioPlayer"
#define SAP_CALLSIGN_VER SAP_CALLSIGN ".1"

namespace WPEFramework
{
    namespace Plugin
    {
        namespace TTS
        {
            using namespace ::TTS;


            SystemAudioPlayerAdapter *SystemAudioPlayerAdapter::getInstance()
            {
                static SystemAudioPlayerAdapter instance;
                return &instance;
            }

            string SystemAudioPlayerAdapter::getSecurityToken()
            {
                std::string token = "token=";
                int tokenLength = 0;
                unsigned char buffer[MAX_SECURITY_TOKEN_SIZE] = {0};
                static std::string endpoint;

                if (endpoint.empty())
                {
                    Core::SystemInfo::GetEnvironment(_T("THUNDER_ACCESS"), endpoint);
                    TTSLOG_INFO("Thunder RPC Endpoint read from env - %s", endpoint.c_str());
                }

                if (endpoint.empty())
                {
                    Core::File file("/etc/WPEFramework/config.json");
                    if (file.Open(true))
                    {
                        JsonObject config;
                        if (config.IElement::FromFile(file))
                        {
                            Core::JSON::String port = config.Get("port");
                            Core::JSON::String binding = config.Get("binding");
                            if (!binding.Value().empty() && !port.Value().empty())
                                endpoint = binding.Value() + ":" + port.Value();
                        }
                        file.Close();
                    }
                    if (endpoint.empty())
                        endpoint = _T("127.0.0.1:9998");

                    TTSLOG_INFO("Thunder RPC Endpoint read from config file - %s", endpoint.c_str());
                    Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), endpoint);
                }

                string payload = "http://localhost";
                if (payload.empty())
                {
                    tokenLength = GetSecurityToken(sizeof(buffer), buffer);
                }
                else
                {
                    int buffLength = std::min(sizeof(buffer), payload.length());
                    ::memcpy(buffer, payload.c_str(), buffLength);
                    tokenLength = GetToken(sizeof(buffer), buffLength, buffer);
                }

                if (tokenLength > 0)
                {
                    token.append((char *)buffer);
                }
                else
                {
                    token.clear();
                }

                TTSLOG_INFO("Thunder token - %s", token.empty() ? "" : token.c_str());
                return token;
            }
            void SystemAudioPlayerAdapter::onSAPEventHandler(const JsonObject &parameters)
            {
                int playerId = -1;
                if (parameters.HasLabel("id")) 
                {
                    playerId = parameters["id"].Number();
                }

                std::string event;
                if (parameters.HasLabel("event")) 
                {
                    event = parameters["event"].String();
                }

                if (playerId == currentPlayerId)
                {
                    if (event == "PLAYBACK_STARTED")
                    {
                        // handle PLAYBACK_STARTED
                    }
                    else if (event == "PLAYBACK_FINISHED")
                    {
                        // handle PLAYBACK_FINISHED
                    }
                    else if (event == "PLAYBACK_PAUSED")
                    {
                        // handle PLAYBACK_PAUSED
                    }
                    else if (event == "NETWORK_ERROR")
                    {
                        // handle NETWORK_ERROR
                    }
                    else if (event == "PLAYBACK_ERROR")
                    {
                        // handle PLAYBACK_ERROR
                    }
                    else if (event == "NEED_DATA")
                    {
                        // handle NEED_DATA
                    }
                    else
                    {
                        // Unknown event
                    }
                }

                TTSLOG_INFO("SAP Event Received: %s", event.c_str());
            }
            SystemAudioPlayerAdapter::SystemAudioPlayerAdapter()
            {
                if (m_systemAudioPlayerService == nullptr)
                {
                    string token = getSecurityToken();
                    if (token.empty())
                    {
                        m_systemAudioPlayerService = new WPEFramework::JSONRPC::LinkType<Core::JSON::IElement>(_T(SAP_CALLSIGN_VER), "");
                    }
                    else
                    {
                        m_systemAudioPlayerService = new WPEFramework::JSONRPC::LinkType<Core::JSON::IElement>(_T(SAP_CALLSIGN_VER), "", false, token);
                    }
                }

                if (!m_eventRegistered)
                {
                    if (m_systemAudioPlayerService->Subscribe<JsonObject>(3000, "onsapevents",
                                                                          &SystemAudioPlayerAdapter::onSAPEventHandler, this) == Core::ERROR_NONE)
                    {
                        m_eventRegistered = true;
                        TTSLOG_INFO("Subscribed to notification handler : onsapevents");
                    }
                    else
                    {
                        TTSLOG_ERROR("Failed to Subscribe notification handler : onsapevents");
                    }
                }
            }

            SystemAudioPlayerAdapter::~SystemAudioPlayerAdapter()
            {
                if(!m_systemAudioPlayerService)
                {
                    delete m_systemAudioPlayerService;
                    m_systemAudioPlayerService =  nullptr;
                }
                m_eventRegistered = false;
            }

            bool SystemAudioPlayerAdapter::close(int playerId)
            {
                if(playerId != currentPlayerId)
                    return false;

                JsonObject joGetParams, joGetResult;
                joGetParams["id"] = currentPlayerId;


                auto status = m_systemAudioPlayerService->Invoke<JsonObject, JsonObject>(3000, "close", joGetParams, joGetResult);
                if (status == Core::ERROR_NONE && joGetResult.HasLabel("success") && joGetResult["success"].Boolean())
                {
                    currentPlayerId = -1;
                    return true;
                }
                else
                {
                    TTSLOG_ERROR("%s call failed %d", SAP_CALLSIGN_VER, status);
                    return false;
                }
            }

            void SystemAudioPlayerAdapter::config()
            {
                JsonObject joGetParams, joGetResult;
                // Set the player ID
                joGetParams["id"] = currentPlayerId;
                // Create PCM config sub-object
                joGetParams["format"] = "S16LE";
                joGetParams["rate"] = "22050";
                joGetParams["channels"] = "1";
                joGetParams["layout"] = "interleaved";

                auto status = m_systemAudioPlayerService->Invoke<JsonObject, JsonObject>(3000, "config", joGetParams, joGetResult);
                if (status == Core::ERROR_NONE && joGetResult.HasLabel("success") && joGetResult["success"].Boolean())
                {
                    TTSLOG_INFO("SAP configuration Successful");
                }
                else
                {
                    TTSLOG_ERROR("%s call failed %d", SAP_CALLSIGN_VER, status);
                }
            }
            int SystemAudioPlayerAdapter::getPlayerSessionId()
            {
                JsonObject joGetParams, joGetResult;
                joGetParams["url"] = currentUrl;
                auto status = m_systemAudioPlayerService->Invoke<JsonObject, JsonObject>(3000, "getPlayerSessionId", joGetParams, joGetResult);
                if (status == Core::ERROR_NONE && joGetResult.HasLabel("success") && joGetResult["success"].Boolean())
                {
                    return joGetResult["sessionId"].Number();
                    
                }
                else
                {
                    TTSLOG_ERROR("%s call failed %d", SAP_CALLSIGN_VER, status);
                    return -1;
                }
            }

            bool SystemAudioPlayerAdapter::isSpeaking()
            {
                JsonObject joGetParams, joGetResult;
                joGetParams["id"] = currentPlayerId;


                auto status = m_systemAudioPlayerService->Invoke<JsonObject, JsonObject>(3000, "isspeaking", joGetParams, joGetResult);
                if (status == Core::ERROR_NONE && joGetResult.HasLabel("success") && joGetResult["success"].Boolean())
                {
                    return true;
                }
                else
                {
                    TTSLOG_ERROR("%s call failed %d", SAP_CALLSIGN_VER, status);
                    return false;
                }
            }

            int SystemAudioPlayerAdapter::open(AudioType audioType)
            {
                JsonObject joGetParams, joGetResult;
                std::string audioTypeRequired = audioType == (AudioType::Audio_PCM) ? "pcm" : "mp3";

                joGetParams["audiotype"] = "mp3";
                joGetParams["sourcetype"] = "httpsrc";
                joGetParams["playmode"] = "app";

                auto status = m_systemAudioPlayerService->Invoke<JsonObject, JsonObject>(3000, "open", joGetParams, joGetResult);
                if (status == Core::ERROR_NONE && joGetResult.HasLabel("success") && joGetResult["success"].Boolean())
                {
                    currentPlayerId = joGetResult["id"].Number();
                    //config();
                    return currentPlayerId;
                }
                else
                {
                    TTSLOG_ERROR("%s call failed %d", SAP_CALLSIGN_VER, status);
                    return -1;
                }
            }

            bool SystemAudioPlayerAdapter::pause(int playerId)
            {
                if(playerId != currentPlayerId)
                    return false;
                JsonObject joGetParams, joGetResult;
                joGetParams["id"] = currentPlayerId;

                auto status = m_systemAudioPlayerService->Invoke<JsonObject, JsonObject>(3000, "pause", joGetParams, joGetResult);
                if (status == Core::ERROR_NONE && joGetResult.HasLabel("success") && joGetResult["success"].Boolean())
                {
                    return true;
                }
                else
                {
                    TTSLOG_ERROR("%s call failed %d", SAP_CALLSIGN_VER, status);
                    return false;
                }
            }

            bool SystemAudioPlayerAdapter::play(std::string url)
            {
                JsonObject joGetParams,joGetResult;
                currentUrl = url;

                joGetParams["id"] = currentPlayerId;
                joGetParams["url"] = currentUrl;

                auto status = m_systemAudioPlayerService->Invoke<JsonObject, JsonObject>(3000, "play", joGetParams, joGetResult);
                if (status == Core::ERROR_NONE && joGetResult.HasLabel("success") && joGetResult["success"].Boolean())
                {
                    return true;
                }
                else
                {
                    TTSLOG_ERROR("%s call failed %d", SAP_CALLSIGN_VER, status);
                    return false;
                }
            }

            void SystemAudioPlayerAdapter::playBuffer()
            {
            }

            bool SystemAudioPlayerAdapter::resume(int playerId)
            {
                if(playerId != currentPlayerId)
                    return false;
                JsonObject joGetParams, joGetResult;
                joGetParams["id"] = currentPlayerId;
                auto status = m_systemAudioPlayerService->Invoke<JsonObject, JsonObject>(3000, "resume", joGetParams, joGetResult);
                if (status == Core::ERROR_NONE && joGetResult.HasLabel("success") && joGetResult["success"].Boolean())
                {
                    return true;
                }
                else
                {
                    TTSLOG_ERROR("%s call failed %d", SAP_CALLSIGN_VER, status);
                    return false;
                }
            }

            bool SystemAudioPlayerAdapter::setMixerLevels(int newPrimaryVolume, int newPlayerVolume)
            {
                JsonObject joGetParams, joGetResult;
                std::string primaryVolume = std::to_string(newPrimaryVolume);
                std::string playerVolume = std::to_string(newPlayerVolume); 

                JsonObject paramsIn;
                joGetParams["id"] = currentPlayerId;
                joGetParams["primaryVolume"] = primaryVolume;
                joGetParams["playerVolume"] = playerVolume;

                auto status = m_systemAudioPlayerService->Invoke<JsonObject, JsonObject>(3000, "setMixerLevels", joGetParams, joGetResult);
                if (status == Core::ERROR_NONE && joGetResult.HasLabel("success")  && joGetResult["success"].Boolean())
                {
                    return true;
                }
                else
                {
                    TTSLOG_ERROR("%s call failed %d", SAP_CALLSIGN_VER, status);
                    return false;
                }
            }

            void SystemAudioPlayerAdapter::setSmartVolControl()
            {
                /*JsonObject joGetParams, joGetResult;
                int playerId;
                bool enable;
                float playerAudioLevelThreashold;
                int playerDetectTimeMs;
                int playerHoldTimeMs;
                int primaryDuckingPercent;
                std::string url;
                joGetParams["params"] = {
                    {"id", playerId},
                    {"enable", true},
                    {"playerAudioLevelThreshold", playerAudioLevelThreashold},
                    {"playerDetectTimeMs", playerDetectTimeMs},
                    {"playerHoldTimeMs", playerHoldTimeMs},
                    {"primaryDuckingPercent", primaryDuckingPercent}};
                auto status = m_systemAudioPlayerService->Invoke<JsonObject, JsonObject>(3000, "setSmartVolControl", joGetParams, joGetResult);
                if (status == Core::ERROR_NONE && joGetResult.HasLabel("success"))
                {
                }
                else
                {
                    TTSLOG_ERROR("%s call failed %d", SAP_CALLSIGN_VER, status);
                }*/
            }
            bool SystemAudioPlayerAdapter::stop(int playerId)
            {
                if(playerId != currentPlayerId)
                    return false;
                JsonObject joGetParams, joGetResult;
                JsonObject id;
                joGetParams["id"] = currentPlayerId;

                auto status = m_systemAudioPlayerService->Invoke<JsonObject, JsonObject>(3000, "stop", joGetParams, joGetResult);
                if (status == Core::ERROR_NONE && joGetResult.HasLabel("success") && joGetResult["success"].Boolean())
                {
                    return true;
                }
                else
                {
                    TTSLOG_ERROR("%s call failed %d", SAP_CALLSIGN_VER, status);
                    return false;
                }
            }


        }
    }
}