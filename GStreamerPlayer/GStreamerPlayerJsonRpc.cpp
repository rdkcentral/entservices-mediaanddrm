/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#include "GStreamerPlayer.h"

namespace WPEFramework {
namespace Plugin {

    void GStreamerPlayer::RegisterAll()
    {
        Register("createPipeline", &GStreamerPlayer::CreatePipeline, this);
        Register("play", &GStreamerPlayer::Play, this);
        Register("pause", &GStreamerPlayer::Pause, this);
        Register("getapiversion", &GStreamerPlayer::getapiversion, this);
    }
    
   
    uint32_t GStreamerPlayer::CreatePipeline(const JsonObject& parameters, JsonObject& response)
    {
        if(_player) {
            string params, result;
            parameters.ToString(params);
            uint32_t ret = _player->CreatePipeline(params, result);
            response.FromString(result);
            return ret;
        }
        return Core::ERROR_NONE;
    }

    uint32_t GStreamerPlayer::Play(const JsonObject& parameters, JsonObject& response)
    {
        if(_player) {
            string params, result;
            parameters.ToString(params);
            uint32_t ret = _player->Play(params, result);
            response.FromString(result);
            return ret;
        }
        return Core::ERROR_NONE;
    }

    uint32_t GStreamerPlayer::Pause(const JsonObject& parameters, JsonObject& response)
    {
        if(_player) {
            string params, result;
            parameters.ToString(params);
            uint32_t ret = _player->Pause(params, result);
            response.FromString(result);
            return ret;
        }
        return Core::ERROR_NONE;
    }

    uint32_t GStreamerPlayer::getapiversion(const JsonObject& parameters, JsonObject& response)
    {
        response["version"] = _apiVersionNumber;
        return Core::ERROR_NONE;
    }

    void GStreamerPlayer::dispatchJsonEvent(const char *event, const string &data)
    {
        Notify(event, JsonObject(data.c_str()));
    }

} // namespace Plugin
} // namespace WPEFramework
