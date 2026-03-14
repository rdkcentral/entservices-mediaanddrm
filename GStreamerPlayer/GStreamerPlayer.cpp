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

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0
#define API_VERSION_NUMBER 1

namespace WPEFramework {

namespace {

    static Plugin::Metadata<Plugin::GStreamerPlayer> metadata(
        // Version (Major, Minor, Patch)
        API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
        // Preconditions
        {},
        // Terminations
        {},
        // Controls
        {}
    );
}

namespace Plugin {

    /*
     *Register GStreamerPlayer module as wpeframework plugin
     **/
    SERVICE_REGISTRATION(GStreamerPlayer, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    const string GStreamerPlayer::Initialize(PluginHost::IShell* service)
    {
        ASSERT(_service == nullptr);

        _connectionId = 0;
        _service = service;
        _skipURL = static_cast<uint8_t>(_service->WebPrefix().length());

        _service->Register(&_notification);

        _player = _service->Root<Exchange::IGStreamerPlayer>(_connectionId, 15000, _T("GStreamerPlayerImplementation"));

        std::string message;
        if(_player != nullptr) {
            #ifndef UNIT_TESTING
                ASSERT(_connectionId != 0);
            #endif

            _player->Configure(_service);
            _player->Register(&_notification);
            RegisterAll();
        } else {
            message = _T("GStreamerPlayer could not be instantiated.");
            _service->Unregister(&_notification);
            _service = nullptr;
        }

        return message;
    }

    void GStreamerPlayer::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);
        ASSERT(_player != nullptr);

        if (!_player)
            return;

        _player->Unregister(&_notification);
        _service->Unregister(&_notification);

        if(_player->Release() != Core::ERROR_DESTRUCTION_SUCCEEDED) {
            ASSERT(_connectionId != 0);
            TRACE_L1("GStreamerPlayer Plugin is not properly destructed. %d", _connectionId);

            RPC::IRemoteConnection* connection(_service->RemoteConnection(_connectionId));

            // The process can disappear in the meantime...
            if (connection != nullptr) {
                // But if it did not dissapear in the meantime, forcefully terminate it. Shoot to kill :-)
                connection->Terminate();
                connection->Release();
            }
        }
        // Deinitialize what we initialized..
        _service = nullptr;
        _player = nullptr;
    }

    GStreamerPlayer::GStreamerPlayer()
            : PluginHost::JSONRPC()
            , _notification(this)
            , _apiVersionNumber(API_VERSION_NUMBER)
    {
    }

    GStreamerPlayer::~GStreamerPlayer()
    {
    }

    void GStreamerPlayer::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == _connectionId) {
            ASSERT(_service != nullptr);
            TRACE_L1("GStreamerPlayer::Deactivated - %p", this);
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

} // namespace Plugin
} // namespace WPEFramework
