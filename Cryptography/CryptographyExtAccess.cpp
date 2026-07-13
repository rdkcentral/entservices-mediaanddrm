/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 Metrological
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

#include "CryptographyExtAccess.h"
#include "UtilsLogging.h"

extern "C" {
    void vault_processor_release(void);
}

namespace WPEFramework {
namespace Plugin {

    namespace {

        static Metadata<CryptographyExtAccess> metadata(
            // Version
            1, 0, 0,
            // Preconditions
            { subsystem::PLATFORM },
            // Terminations
            {},
            // Controls
            {}
        );
    }


    const string CryptographyExtAccess::Initialize(PluginHost::IShell* service) /* override */
    {
        string message;

        ASSERT(service != nullptr);
        ASSERT(_service == nullptr);
        ASSERT(_implementation == nullptr);
        ASSERT(_connectionId == 0);

        _service = service;
        _service->AddRef();

        _service->Register(&_notification);

        if (!createImplementation()) {
            message = _T("CryptographyExtAccess could not be instantiated.");
        } else {
            InitializePowerManager();
            registerPowerEventHandlers();
        }

        return message;
    }

    void CryptographyExtAccess::Deinitialize(PluginHost::IShell* service)  /* override */
    {
        if (_service != nullptr) {
            ASSERT(_service == service);

            unregisterPowerEventHandlers();
            DeinitializePowerManager();

            _service->Unregister(&_notification);

            // Serialize teardown with any in-flight onPowerModeChanged().
            // unregisterPowerEventHandlers() stops new notifications but
            // does not wait for ones already mid-flight, which could
            // otherwise race destroyImplementation() or use _service
            // after we release it.
            std::lock_guard<std::mutex> lock(_implMutex);

            destroyImplementation();

            _connectionId = 0;
            _service->Release();
            _service = nullptr;
        }

    }

    string CryptographyExtAccess::Information() const /* override */
    {
        return string();
    }

    void CryptographyExtAccess::Deactivated(RPC::IRemoteConnection* connection)
    {
        // Deep-sleep teardown intentionally drops the remote connection while
        // _notification is still registered; ignore the resulting Deactivated
        // so we don't submit a DEACTIVATED/FAILURE job against ourselves.
        if (_inDeepSleep) {
            return;
        }
        if (connection->Id() == _connectionId) {

            ASSERT(_service != nullptr);

            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service,
                PluginHost::IShell::DEACTIVATED,
                PluginHost::IShell::FAILURE));
        }
    }

    bool CryptographyExtAccess::createImplementation()
    {
        ASSERT(_service != nullptr);
        ASSERT(_implementation == nullptr);

        _implementation = _service->Root<Exchange::IConfiguration>(_connectionId, Core::infinite, _T("CryptographyImplementation"));
        if (_implementation == nullptr) {
            LOGERR("CryptographyExtAccess could not instantiate CryptographyImplementation");
            return false;
        }

        LOGINFO("CryptographyExtAccess - Connection Id - %u", _connectionId);
        _implementation->Configure(_service);
        return true;
    }

    void CryptographyExtAccess::destroyImplementation()
    {
        if (_implementation != nullptr) {

            VARIABLE_IS_NOT_USED uint32_t result = _implementation->Release();
            _implementation = nullptr;
            ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);

            // Query after Release so a non-null result means the remote
            // genuinely failed to tear down (true error case), instead of
            // tripping on every clean teardown including deep-sleep entry.
            RPC::IRemoteConnection* connection(_service->RemoteConnection(_connectionId));
            if (connection != nullptr) {
                TRACE(Trace::Error, (_T("CryptographyExtAccess is not properly destructed. %d"), _connectionId));

                connection->Terminate();
                connection->Release();
            }
        }
    }

    void CryptographyExtAccess::InitializePowerManager()
    {
        LOGINFO("InitializePowerManager CryptographyExtAccess");
        _powerManagerPlugin = PowerManagerInterfaceBuilder(_T("org.rdk.PowerManager"))
            .withIShell(_service)
            .createInterface();
    }

    void CryptographyExtAccess::DeinitializePowerManager()
    {
        LOGINFO("DeinitializePowerManager CryptographyExtAccess");
        if (_powerManagerPlugin) {
            _powerManagerPlugin.Reset();
        }
    }

    void CryptographyExtAccess::registerPowerEventHandlers()
    {
        if (!_registeredPowerEventHandlers && _powerManagerPlugin) {
            _registeredPowerEventHandlers = true;
            _powerManagerPlugin->Register(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IModeChangedNotification>());
            LOGINFO("CryptographyExtAccess registered PowerManager mode change handler");
        } else {
            LOGWARN("CryptographyExtAccess registerPowerEventHandlers failed");
        }
    }

    void CryptographyExtAccess::unregisterPowerEventHandlers()
    {
        if (_registeredPowerEventHandlers && _powerManagerPlugin) {
            _registeredPowerEventHandlers = false;
            _powerManagerPlugin->Unregister(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IModeChangedNotification>());
            LOGINFO("CryptographyExtAccess unregistered PowerManager mode change handler");
        }
    }

    void CryptographyExtAccess::onPowerModeChanged(const PowerState currentState, const PowerState newState)
    {
        LOGINFO("CryptographyExtAccess::onPowerModeChanged: Old State %d, New State: %d", static_cast<int>(currentState), static_cast<int>(newState));

        std::lock_guard<std::mutex> lock(_implMutex);

        if (newState == WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP) {
            if (!_inDeepSleep) {
                LOGINFO("Releasing SecAPI handle before deep sleep");
                // Set the flag before tearing down so Deactivated(), which
                // can fire on a worker thread when destroyImplementation()
                // drops the remote connection, observes _inDeepSleep == true
                // and skips the FAILURE submission.
                _inDeepSleep = true;
                vault_processor_release();
                destroyImplementation();
            }
        } else if (_inDeepSleep) {
            LOGINFO("Re-acquiring SecAPI handle after deep sleep");
            if (createImplementation()) {
                _inDeepSleep = false;
            } else {
                // Keep the flag set so the next non-deep-sleep transition
                // retries createImplementation() naturally, rather than
                // clearing state and waiting for a full sleep+wake cycle.
                LOGERR("CryptographyExtAccess could not re-create implementation after deep sleep; will retry on next wake transition");
            }
        }
    }
} // namespace Plugin
} // namespace WPEFramework
