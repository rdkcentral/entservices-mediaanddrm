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

/**
 * @file GStreamerPlayer.h
 * @brief Thunder Plugin based Implementation for GStreamer Player service API's.
 */

/**
  @mainpage GStreamerPlayer

  <b>GStreamerPlayer</b> Thunder Service provides APIs for media playback using GStreamer
  
  */

#pragma once

#include "Module.h"
#include "tracing/Logging.h"

#include "GStreamerPlayerImplementation.h"

namespace WPEFramework {
namespace Plugin {

    class GStreamerPlayer: public PluginHost::IPlugin, public PluginHost::JSONRPC {
    public:
        class Notification : public RPC::IRemoteConnection::INotification,
                             public Exchange::IGStreamerPlayer::INotification {
            private:
                Notification() = delete;
                Notification(const Notification&) = delete;
                Notification& operator=(const Notification&) = delete;

            public:
                explicit Notification(GStreamerPlayer* parent)
                    : _parent(*parent) {
                    ASSERT(parent != nullptr);
                }

                virtual ~Notification() {
                }

            public:
                
                virtual void OnPipelineCreated(const string &data) {
                    _parent.dispatchJsonEvent("onPipelineCreated", data);
                }

                virtual void OnPlaybackStateChanged(const string &data) {
                    _parent.dispatchJsonEvent("onPlaybackStateChanged", data);
                }

                virtual void Activated(RPC::IRemoteConnection* /* connection */) final
                {
                    TRACE_L1("GStreamerPlayer::Notification::Activated - %p", this);
                }

                virtual void Deactivated(RPC::IRemoteConnection* connection) final
                {
                    TRACE_L1("GStreamerPlayer::Notification::Deactivated - %p", this);
                    _parent.Deactivated(connection);
                }

                BEGIN_INTERFACE_MAP(Notification)
                INTERFACE_ENTRY(Exchange::IGStreamerPlayer::INotification)
                INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                END_INTERFACE_MAP

            private:
                    GStreamerPlayer& _parent;
        };

        BEGIN_INTERFACE_MAP(GStreamerPlayer)
        INTERFACE_ENTRY(PluginHost::IPlugin)
        INTERFACE_ENTRY(PluginHost::IDispatcher)
        INTERFACE_AGGREGATE(Exchange::IGStreamerPlayer, _player)
        END_INTERFACE_MAP

    public:
        GStreamerPlayer();
        virtual ~GStreamerPlayer();
        virtual const string Initialize(PluginHost::IShell* service) override;
        virtual void Deinitialize(PluginHost::IShell* service) override;
        virtual string Information() const override { return {}; }

    private:
        // We do not allow this plugin to be copied !!
        GStreamerPlayer(const GStreamerPlayer&) = delete;
        GStreamerPlayer& operator=(const GStreamerPlayer&) = delete;

        void RegisterAll();

        uint32_t CreatePipeline(const JsonObject& parameters, JsonObject& response);
        uint32_t Play(const JsonObject& parameters, JsonObject& response);
        uint32_t Pause(const JsonObject& parameters, JsonObject& response);

        //version number API's
        uint32_t getapiversion(const JsonObject& parameters, JsonObject& response);

        void dispatchJsonEvent(const char *event, const string &data);
        void Deactivated(RPC::IRemoteConnection* connection);

    private:
        uint8_t _skipURL{};
        uint32_t _connectionId{};
        PluginHost::IShell* _service{};
        Exchange::IGStreamerPlayer* _player{};
        Core::Sink<Notification> _notification;
        uint32_t _apiVersionNumber;
        
        friend class Notification;
    };

} // namespace Plugin
} // namespace WPEFramework
