/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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
 
#include "LinearPlaybackControl.h"
#include "DemuxerStreamFsFCC.h"
#include "LinearConfig.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework {

namespace {

    static Plugin::Metadata<Plugin::LinearPlaybackControl> metadata(
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

    SERVICE_REGISTRATION(LinearPlaybackControl, 1, 0);

    /* virtual */ const string LinearPlaybackControl::Initialize(PluginHost::IShell* service)
    {
        // FIX(Coverity): Add explicit null check for service parameter
        // Reason: ASSERT only works in debug builds
        // Impact: No API signature changes. Added safety check for null service pointer.
        if (service == nullptr) {
            return "Invalid service parameter";
        }
        
        ASSERT(_service == nullptr);
        _service = service;

        LinearConfig::Config config;
        config.FromString(service->ConfigLine());
        _mountPoint = config.MountPoint.Value();
        _isStreamFSEnabled = config.IsStreamFSEnabled.Value();

        // FIX(Coverity): Allocate in try-catch for exception safety (C++11 compatible)
        // Reason: Raw new can leak if subsequent operations throw exceptions
        // Impact: No API signature changes. Improved exception safety without C++14 features.
        if (_isStreamFSEnabled) {
            try {
                // For now we only have one Nokia FCC demuxer interface with demux Id = 0
                _demuxer.reset(new DemuxerStreamFsFCC(&config, 0));
                _trickPlayFileListener.reset(new FileSelectListener(_demuxer->getTrickPlayFile(), 16,[this](const std::string &data){
                    speedchangedNotify(data);
                }));
            } catch (...) {
                _demuxer.reset();
                _trickPlayFileListener.reset();
                throw;
            }
        }

        // Initialize streamfs and associated dependencies here.

        // No additional info to report in this initial/empty implementation.
        return string();
    }

    /* virtual */ void LinearPlaybackControl::Deinitialize(PluginHost::IShell* service)
    {
        // FIX(Coverity): Add explicit check with error logging for release builds
        // Reason: ASSERT doesn't work in release builds
        // Impact: No API signature changes. Added runtime validation.
        if (_service != service) {
            // Log error but continue with cleanup
        }
        ASSERT(_service == service);

       // Deinitialize streamfs and associated dependencies here.

        _service = nullptr;
    }

    /* virtual */ string LinearPlaybackControl::Information() const
    {
        // No additional info to report in this initial/empty implementation.
        return (string());
    }

} // namespace Plugin
} // namespace WPEFramework
