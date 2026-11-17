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


#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include "SystemAudioPlayer.h"
#include "SystemAudioPlayerImplementation.h"

#include "ServiceMock.h"
#include "COMLinkMock.h"
#include "FactoriesImplementation.h"
#include "WorkerPoolImplementation.h"
#include "ThunderPortability.h"
#include "systemaudioplatformmock.h"
#include "PowerManagerMock.h"

using namespace WPEFramework;
using ::testing::Test;
using ::testing::NiceMock;

class SAPTest : public Test {
protected:
    Core::ProxyType<Plugin::SystemAudioPlayer> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;

    SystemAudioPlatformAPIMock *p_systemAudioPlatformMock = nullptr;
    Core::ProxyType<Plugin::SystemAudioPlayerImplementation> SystemAudioPlayerImplementation;
    NiceMock<ServiceMock> service;
    NiceMock<COMLinkMock> comLinkMock;
    PLUGINHOST_DISPATCHER* dispatcher;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    NiceMock<FactoriesImplementation> factoriesImplementation;
    PluginHost::IPlugin::INotification* pluginNotification{nullptr};
    bool pluginInitialized{false};

    SAPTest()
        : plugin(Core::ProxyType<Plugin::SystemAudioPlayer>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
    p_systemAudioPlatformMock = new testing::NiceMock<SystemAudioPlatformAPIMock>;
    SystemAudioPlatformMockImpl::setImpl(p_systemAudioPlatformMock);

        ON_CALL(service, COMLink())
            .WillByDefault(::testing::Invoke(
                  [this]() {
                        return &comLinkMock;
                    }));

#ifdef USE_THUNDER_R4
        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
			.WillByDefault(::testing::Invoke(
                  [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                        SystemAudioPlayerImplementation = Core::ProxyType<Plugin::SystemAudioPlayerImplementation>::Create();
                        return &SystemAudioPlayerImplementation;
                    }));
#else
        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(SystemAudioPlayerImplementation));
#endif /*USE_THUNDER_R4 */

        PluginHost::IFactories::Assign(&factoriesImplementation);

        ON_CALL(service, Register(::testing::Matcher<PluginHost::IPlugin::INotification*>(::testing::_)))
            .WillByDefault(::testing::SaveArg<0>(&pluginNotification));

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
        plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&service);

        EXPECT_EQ(string(""), plugin->Initialize(&service));
        pluginInitialized = true;

    }

    virtual ~SAPTest() override
    {
        DeinitializePlugin();

        dispatcher->Deactivate();
        dispatcher->Release();

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();

        SystemAudioPlatformMockImpl::setImpl(nullptr);
        if (p_systemAudioPlatformMock != nullptr)
        {
            delete p_systemAudioPlatformMock;
            p_systemAudioPlatformMock = nullptr;
        }

        PluginHost::IFactories::Assign(nullptr);
    }

    void DeinitializePlugin()
    {
        if (pluginInitialized) {
            plugin->Deinitialize(&service);
            pluginInitialized = false;
        }
    }
};

class SAPInitializedTest : public SAPTest {
protected:

    SAPInitializedTest() : SAPTest() {
   gst_init(nullptr, nullptr);

    ON_CALL(*p_systemAudioPlatformMock, systemAudioInitialize())
        .WillByDefault(::testing::Return());
    ON_CALL(*p_systemAudioPlatformMock, systemAudioDeinitialize())
        .WillByDefault(::testing::Return());

    ON_CALL(*p_systemAudioPlatformMock, systemAudioChangePrimaryVol(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return());

    ON_CALL(*p_systemAudioPlatformMock, systemAudioSetDetectTime(::testing::_))
        .WillByDefault(::testing::Return());

    ON_CALL(*p_systemAudioPlatformMock, systemAudioSetHoldTime(::testing::_))
        .WillByDefault(::testing::Return());

    ON_CALL(*p_systemAudioPlatformMock, systemAudioSetThreshold(::testing::_))
        .WillByDefault(::testing::Return());

    ON_CALL(*p_systemAudioPlatformMock, systemAudioSetVolume(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return());

    ON_CALL(*p_systemAudioPlatformMock, systemAudioGeneratePipeline(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](GstElement** pipeline, GstElement** source, GstElement* capsfilter,
                             GstElement** audioSink, GstElement** audioVolume,
                             AudioType type, PlayMode mode, SourceType sourceType, bool smartVolumeEnable) {

            bool result = false;
            return result;
        }));

    }

    virtual ~SAPInitializedTest() override = default;
};

TEST_F(SAPInitializedTest,RegisteredMethods) {

    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("close")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("config")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPlayerSessionId")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("isspeaking")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("open")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("pause")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("play")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("playbuffer")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("resume")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setMixerLevels")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setSmartVolControl")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("stop")));
}

/*******************************************************************************************************************
 * Test function for open
 * Open                    :
 *                Opens a player instance and assigns it a unique ID
 *
 *                @return Response object contains id and success status
 * Use case coverage:
 *                @Success : 24
 *                @Failure : 3
 ********************************************************************************************************************/

/**
 * @name  : SAPOpenPCMWebsocketSystem
 * @brief : Open a player instance with pcm, websocket, system 
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:1, success: true}
 */

TEST_F(SAPInitializedTest, SAPOpenPCMWebsocketSystem) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":1,\"success\":true}"));
}

/**
 * @name  : SAPOpenPCMDataSystem
 * @brief : Open a player instance with pcm, websocket, app
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:3, success: true}
 */

TEST_F(SAPInitializedTest,SAPOpenPCMDataSystem) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"data\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":2,\"success\":true}"));
}

/**
 * @name  : SAPOpenPCMWebsocketApp
 * @brief : Open a player instance with pcm, websocket, app
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:3, success: true}
 */

TEST_F(SAPInitializedTest,SAPOpenPCMWebsocketApp) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"app\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":3,\"success\":true}"));
}

/**
 * @name  : SAPOpenPCMDataApp
 * @brief : Open a player instance with pcm, data, app
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:4, success: true}
 */

TEST_F(SAPInitializedTest,SAPOpenPCMDataApp) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"data\",\"playmode\": \"app\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":4,\"success\":true}"));
}

/**
 * @name  : SAPOpenPCMFilesrcSystem
 * @brief : Open a player instance with pcm, filesrc, systen
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:5, success: true}
 */

TEST_F(SAPInitializedTest,SAPOpenPCMFilesrcSystem) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"filesrc\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":5,\"success\":true}"));
}

/**
 * @name  : SAPOpenPCMFilesrcApp
 * @brief : Open a player instance with pcm, filesrc, app
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:6, success: true}
 */

TEST_F(SAPInitializedTest,SAPOpenPCMFilesrcApp) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"filesrc\",\"playmode\": \"app\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":6,\"success\":true}"));
}

/**
 * @name  : SAPOpenPCMHttpsrcSystem
 * @brief : Open a player instance with pcm, httpsrc, system
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:7, success: true}
 */

TEST_F(SAPInitializedTest,SAPOpenPCMHttpsrcSystem) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"httpsrc\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":7,\"success\":true}"));
}

/**
 * @name  : SAPOpenPCMHttpsrcApp
 * @brief : Open a player instance with pcm, httpsrc, app
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:8, success: true}
 */

TEST_F(SAPInitializedTest,SAPOpenPCMHttpsrcApp) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"httpsrc\",\"playmode\": \"app\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":8,\"success\":true}"));
}

/**
 * @name  : SAPMp3WebsocketSystem
 * @brief : Open a player instance with mp3, websocket, system
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:9, success: true}
 */

TEST_F(SAPInitializedTest,SAPMp3WebsocketSystem) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"mp3\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":9,\"success\":true}"));
}

/**
 * @name  : SAPMp3WebsocketApp
 * @brief : Open a player instance with mp3, websocket, app
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:10, success: true}
 */

TEST_F(SAPInitializedTest,SAPMp3WebsocketApp) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"mp3\",\"sourcetype\": \"websocket\",\"playmode\": \"app\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":10,\"success\":true}"));
}

/**
 * @name  : SAPMp3DataSystem
 * @brief : Open a player instance with mp3, data, system
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:11, success: true}
 */

TEST_F(SAPInitializedTest,SAPMp3DataSystem) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"mp3\",\"sourcetype\": \"data\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":11,\"success\":true}"));
}

/**
 * @name  : SAPMp3DataApp
 * @brief : Open a player instance with mp3, data, app
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:12, success: true}
 */

TEST_F(SAPInitializedTest,SAPMp3DataApp) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"mp3\",\"sourcetype\": \"data\",\"playmode\": \"app\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":12,\"success\":true}"));
}

/**
 * @name  : SAPMp3FilesrcSystem
 * @brief : Open a player instance with mp3, filesrc, system
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:13, success: true}
 */

TEST_F(SAPInitializedTest,SAPMp3FilesrcSystem) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"mp3\",\"sourcetype\": \"filesrc\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":13,\"success\":true}"));
}

/**
 * @name  : SAPMp3FilesrcApp
 * @brief : Open a player instance with mp3, filesrc, app
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:14, success: true}
 */

TEST_F(SAPInitializedTest,SAPMp3FilesrcApp) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"mp3\",\"sourcetype\": \"filesrc\",\"playmode\": \"app\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":14,\"success\":true}"));
}

/**
 * @name  : SAPMp3HttpsrcSystem
 * @brief : Open a player instance with mp3, httpsrc, system
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:15, success: true}
 */

TEST_F(SAPInitializedTest,SAPMp3HttpsrcSystem) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"mp3\",\"sourcetype\": \"httpsrc\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":15,\"success\":true}"));
}

/**
 * @name  : SAPMp3HttpsrcApp
 * @brief : Open a player instance with mp3, httpsrc, app
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:16, success: true}
 */

TEST_F(SAPInitializedTest,SAPMp3HttpsrcApp) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"mp3\",\"sourcetype\": \"httpsrc\",\"playmode\": \"app\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":16,\"success\":true}"));
}

/**
 * @name  : SAPWavWebsocketSystem
 * @brief : Open a player instance with wav, websocket, system
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:17, success: true}
 */

TEST_F(SAPInitializedTest,SAPWavWebsocketSystem) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"wav\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":17,\"success\":true}"));
}

/**
 * @name  : SAPWavWebsocketApp
 * @brief : Open a player instance with wav, websocket, app
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:18, success: true}
 */

TEST_F(SAPInitializedTest,SAPWavWebsocketApp) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"wav\",\"sourcetype\": \"websocket\",\"playmode\": \"app\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":18,\"success\":true}"));
}

/**
 * @name  : SAPWavDataSystem
 * @brief : Open a player instance with wav, data, system
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:19, success: true}
 */

TEST_F(SAPInitializedTest,SAPWavDataSystem) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"wav\",\"sourcetype\": \"data\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":19,\"success\":true}"));
}

/**
 * @name  : SAPWavDataApp
 * @brief : Open a player instance with wav, data, app
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:20, success: true}
 */

TEST_F(SAPInitializedTest,SAPWavDataApp) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"wav\",\"sourcetype\": \"data\",\"playmode\": \"app\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":20,\"success\":true}"));
}

/**
 * @name  : SAPWavFileSrcSystem
 * @brief : Open a player instance with wav, filesrc, system
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:21, success: true}
 */

TEST_F(SAPInitializedTest,SAPWavFileSrcSystem) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"wav\",\"sourcetype\": \"filesrc\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":21,\"success\":true}"));
}

/**
 * @name  : SAPWavFileSrcApp
 * @brief : Open a player instance with wav, filesrc, app
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:22, success: true}
 */

TEST_F(SAPInitializedTest,SAPWavFileSrcApp) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"wav\",\"sourcetype\": \"filesrc\",\"playmode\": \"app\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":22,\"success\":true}"));
}

/**
 * @name  : SAPWavHttpSrcSystem
 * @brief : Open a player instance with wav, httpsrc, system
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:23, success: true}
 */

TEST_F(SAPInitializedTest,SAPWavHttpSrcSystem) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"wav\",\"sourcetype\": \"httpsrc\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":23,\"success\":true}"));
}

/**
 * @name  : SAPWavHttpSrcApp
 * @brief : Open a player instance with wav, httpsrc, app
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {id:24, success: true}
 */

TEST_F(SAPInitializedTest,SAPWavHttpSrcApp) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"wav\",\"sourcetype\": \"httpsrc\",\"playmode\": \"app\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"id\":24,\"success\":true}"));
}

/**
 * @name  : SAPOpenInvalidAudioType
 * @brief : Given InvalidAudioType It should return false
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest,SAPOpenInvalidAudioType) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"invalid\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

/**
 * @name  : SAPOpenInvalidSourceType
 * @brief : Given InvalidSourceType It should return false
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest,SAPOpenInvalidSourceType) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"invalid\",\"playmode\": \"system\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

/**
 * @name  : SAPOpenInvalidPlayMode
 * @brief : Given InvalidPlayMode It should return false
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest,SAPOpenInvalidPlayMode) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"invalid\" }"),
         response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}
/*******************************************************************************************************************
 * Test function for config
 * Open                    :
 *                Configures playback for a PCM audio source (audio/x-raw) on the specified player
 *
 *                @return Response object success status
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 2
 ********************************************************************************************************************/
/**
 * @name  : SAPConfig
 * @brief : Sets a config for PCM audio playback
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {success: true}
 */

TEST_F(SAPInitializedTest, SAPConfig) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"filesrc\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("config"),
        _T("{\"id\": ") +
            std::to_string(speechId) +
            _T(", \"pcmconfig\": {\"format\": \"S16LE\",\"rate\": \"22050\",\"channels\": \"1\",\"layout\": \"interleaved\"},")
            _T("\"websocketsecparam\": {\"cafilenames\": [{\"cafilename\": \"/etc/ssl/certs/Xfinity_Subscriber_ECC_Root.pem\"}],")
            _T("\"certfilename\": \"...\",\"keyfilename\": \"...\"}}"),
        response
        ));

        EXPECT_EQ(response, _T("{\"success\":true}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPConfigForOtherAudioType
 * @brief : Sets a config for MP3 audio playback
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPConfigForOtherAudioType) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"mp3\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("config"),
        _T("{\"id\": ") +
            std::to_string(speechId) +
            _T(", \"pcmconfig\": {\"format\": \"S16LE\",\"rate\": \"22050\",\"channels\": \"1\",\"layout\": \"interleaved\"},")
            _T("\"websocketsecparam\": {\"cafilenames\": [{\"cafilename\": \"/etc/ssl/certs/Xfinity_Subscriber_ECC_Root.pem\"}],")
            _T("\"certfilename\": \"...\",\"keyfilename\": \"...\"}}"),
        response
        ));

        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPConfigWithoutOpen
 * @brief : Sets a config without player instance
 * 
 * @param[in]   :  audiotype ,sourcetype, playmode
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest,SAPConfigWithoutOpen) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
    _T("config"),
    _T("{\"id\": 1, "
       "\"pcmconfig\": {\"format\": \"S16LE\", \"rate\": \"22050\", \"channels\": \"1\", \"layout\": \"interleaved\"}, "
       "\"websocketsecparam\": {\"cafilenames\": [{\"cafilename\": \"/etc/ssl/certs/Xfinity_Subscriber_ECC_Root.pem\"}], "
       "\"certfilename\": \"...\", \"keyfilename\": \"...\"}}"),
    response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

/*******************************************************************************************************************
 * Test function for play
 * Open                    :
 *                Plays audio on the specified player.
 *
 *                @return Response object success status
 * Use case coverage:
 *                @Success : 4
 *                @Failure : 3
 ********************************************************************************************************************/

/**
 * @name  : SAPPlayWebsocket
 * @brief : plays a player instance
 * 
 * @param[in]   :  id ,url 
 * @return      :  {success: true}
 */

TEST_F(SAPInitializedTest, SAPPlayWebsocket) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart); 
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("play"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(", \"url\": \"ws://example.com/socket\" }"),
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":true}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPPlayDataSrc
 * @brief : plays a player instance
 * 
 * @param[in]   :  id ,url 
 * @return      :  {success: true}
 */

TEST_F(SAPInitializedTest, SAPPlayDataSrc) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"data\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("play"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(", \"url\": \"data://example/data\" }"),
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":true}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPPlayFilesrc
 * @brief : Opens a player instance with source:filesrc but given http url
 * 
 * @param[in]   :  id ,url 
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPPlayFilesrc) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"wav\",\"sourcetype\": \"filesrc\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("play"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(", \"url\": \"file:///example/data\" }"),
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":true}"));
        sleep(1);
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPPlayHttpsrc
 * @brief : Opens a player instance with source:httpsrc but given http url
 * 
 * @param[in]   :  id ,url 
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPPlayHttpsrc) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"httpsrc\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("play"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(", \"url\": \"http://example/data\" }"),
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":true}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPPlayDifferentSrcUrl
 * @brief : Opens a player instance with source:httpsrc but given http url
 * 
 * @param[in]   :  id ,url 
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPPlayDifferentSrcUrl) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"httpsrc\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("play"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(", \"url\": \"file:///example/data\" }"),
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPPlayWithoutOpen
 * @brief : Sets a play without player instance
 * 
 * @param[in]   :  id ,url
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest,SAPPlayWithoutOpen) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, \
        _T("play"),
        _T("{\"id\": 1, \"url\": \"https://example/data\"}"), 
        response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

/**
 * @name  : SAPPlayInvalidUrl
 * @brief : Play with invalid url
 * 
 * @param[in]   :  id , url
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPPlayInvalidUrl) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, 
        _T("play"),
        _T("{\"id\": 1,\"url\": \"invalid\" }"), 
        response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}")); 
}

/*******************************************************************************************************************
 * Test function for close
 * Close                    :
 *                Closes the specified Player
 *
 *                @return Response object success status
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 1
 ********************************************************************************************************************/

/**
 * @name  : SAPClose
 * @brief : Closes the player instance
 * 
 * @param[in]   :  id
 * @return      :  {success: true}
 */

TEST_F(SAPInitializedTest, SAPClose) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, 
            _T("close"),
            _T("{\"id\": ") + std::to_string(speechId) + _T("}"), 
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":true}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPCloseWithoutOpen
 * @brief : Close the player Which is not opened
 * 
 * @param[in]   :  id
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest,SAPCloseWithoutOpen) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("close"),
        _T("{\"id\": 1}"),
        response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

/*******************************************************************************************************************
 * Test function for resume
 * Resume                    :
 *                Resumes the specified Player
 *
 *                @return Response object success status
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 1
 ********************************************************************************************************************/

/**
 * @name  : SAPResume
 * @brief : Resume the player instance
 * 
 * @param[in]   :  id
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPResume) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, 
            _T("resume"),
            _T("{\"id\": ") + std::to_string(speechId) + _T("}"), 
            response\
        ));
        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPResumeWithOutOpen
 * @brief : Resume the invalid player
 * 
 * @param[in]   :  id
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest,SAPResumeWithOutOpen) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("resume"),
        _T("{\"id\": 1}"),
        response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

/*******************************************************************************************************************
 * Test function for pause
 * Pause                   :
 *                Closes the specified Player
 *
 *                @return Response object success status
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 1
 ********************************************************************************************************************/

/**
 * @name  : SAPPause
 * @brief : Pause the player instance
 * 
 * @param[in]   :  id
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPPause) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, 
            _T("pause"),
            _T("{\"id\": ") + std::to_string(speechId) + _T("}"), 
            response\
        ));
        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPPauseWithOutOpen
 * @brief : Pause the invalid player
 * 
 * @param[in]   :  id
 * @return      :  {success: false}
 */
TEST_F(SAPInitializedTest,SAPPauseWithoutOpen) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("pause"),
        _T("{\"id\": 1}"),
        response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

/*******************************************************************************************************************
 * Test function for stop
 * Stop                  :
 *                stop the specified Players connection with source
 *
 *                @return Response object success status
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 1
 ********************************************************************************************************************/

/**
 * @name  : SAPStop
 * @brief : Stop the connection between websocket
 * 
 * @param[in]   :  id
 * @return      :  {success: true}
 */

TEST_F(SAPInitializedTest, SAPStop) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, 
            _T("stop"),
            _T("{\"id\": ") + std::to_string(speechId) + _T("}"), 
            response\
        ));
        EXPECT_EQ(response, _T("{\"success\":true}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPStopWithOutOpen
 * @brief : Stop the invalid player
 * 
 * @param[in]   :  id
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest,SAPStopWithoutOpen) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("stop"),
        _T("{\"id\": 1}"),
        response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

/*******************************************************************************************************************
 * Test function for isSpeaking
 * isSpeaking                 :
 *                Checks the player instance is speaking or not
 *
 *                @return Response object success status
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 1
 ********************************************************************************************************************/

/**
 * @name  : SAPIsSpeaking
 * @brief : checks the player instance is speaking or not
 * 
 * @param[in]   :  id
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPIsSpeaking) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("isspeaking"),
            _T("{\"id\": ") + std::to_string(speechId) + _T("}"), 
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPisSpeakingWithoutOpen
 * @brief : Check isSpeaking the invalid player
 * 
 * @param[in]   :  id
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest,SAPisSpeakingWithoutOpen) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("isspeaking"),
        _T("{\"id\": 1}"),
        response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

/*******************************************************************************************************************
 * Test function for playbuffer
 * PlayBuffer                 :
 *                Buffers the audio playback on the specified player
 *
 *                @return Response object success status
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 1
 ********************************************************************************************************************/
/**
 * @name  : SAPPlayBuffer
 * @brief : Buffers the audio playback on the specified player.
 * 
 * @param[in]   :  id , data
 * @return      :  {success: true}
 */

TEST_F(SAPInitializedTest, SAPPlayBuffer) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("playbuffer"), 
            _T("{\"id\": ") + std::to_string(speechId) + _T(",\"data\": \"180\"}"),
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":true}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPPlayBufferWithoutOpen
 * @brief : Buffer on invalid player
 * 
 * @param[in]   :  id , data
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest,SAPPlayBufferWithoutOpen) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("playbuffer"),
        _T("{\"id\": 1 , \"data\": \"180\"}"),
        response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

/*******************************************************************************************************************
 * Test function for setMixerLevel
 * setMixerLevel                 :
 *                Sets the audio level on the specified player. The SystemAudioPlayer plugin can control the volume of the content being played back as well as the primary program audio; 
 *                thus, an application can duck down the volume on the primary program
 *                audio when system audio is played and then restore it back when the system audio playback is complete.
 *
 *                @return Response object success status
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 5
 ********************************************************************************************************************/

/**
 * @name  : SAPSetMixerLevel
 * @brief : SettingMixerlevels for Player
 * 
 * @param[in]   :  id , primaryVolume , player volume
 * @return      :  {success: true}
 */

TEST_F(SAPInitializedTest, SAPSetMixerLevel) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("setMixerLevels"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(",\"primaryVolume\": \"80\" ,\"playerVolume\": \"70\"}"),
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":true}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPSetMixerLevelPriVolLessThanZero
 * @brief : Set Primary Volume less than zero and expects failure
 * 
 * @param[in]   :  id , primaryVolume , player volume
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPSetMixerLevelPriVolLessThanZero) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("setMixerLevels"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(",\"primaryVolume\": \"-1\" ,\"playerVolume\": \"70\"}"),
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPSetMixerLevelPlayerVolLessThanZero
 * @brief : Set Player Volume less than zero and expects failure
 * 
 * @param[in]   :  id , primaryVolume , player volume
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPSetMixerLevelPlayerVolLessThanZero) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

  size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("setMixerLevels"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(",\"primaryVolume\": \"80\" ,\"playerVolume\": \"-1\"}"),
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPSetMixerLevelPriVolGreaterThan100
 * @brief : Set Primary Volume Greater than 100 and expects failure
 * 
 * @param[in]   :  id , primaryVolume , player volume
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPSetMixerLevelPriVolGreaterThan100) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

  size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("setMixerLevels"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(",\"primaryVolume\": \"101\" ,\"playerVolume\": \"70\"}"),
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPSetMixerLevelPlayerVolGreaterThan100
 * @brief : Set Player Volume Greater than 100 and expects failure
 * 
 * @param[in]   :  id , primaryVolume , player volume
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPSetMixerLevelPlayerVolGreaterThan100) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

  size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("setMixerLevels"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(",\"primaryVolume\": \"80\" ,\"playerVolume\": \"101\"}"),
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPSetMixerLevelsWithoutOpen
 * @brief : SettingMixerlevels for invalidPlayer
 * 
 * @param[in]   :  id , primaryVolume , player volume
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest,SAPSetMixerLevelsWithoutOpen) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("setMixerLevels"),
        _T("{\"id\": 1 , \"primaryVolume\": \"180\" ,\"playerVolume\": \"7\" }"), 
        response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

/*******************************************************************************************************************
 * Test function for SetSmartVolControl
 * SetSmartVolControl                 :
 *                Sets the smart volume audio control on the specified player. 
 *                The plugin can control the smart volume of the content being played back as well as the primary program audio; 
 *                thus, an application can duck down the volume on the primary program audio when system audio is played 
 *                and then restore it back when the system audio playback is complete.
 *
 *                @return Response object success status
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 5
 ********************************************************************************************************************/
/**
 * @name  : SAPSetSmartVolControl
 * @brief : SettingSAPSetSmartVolControl for Player
 * 
 * @param[in]   :  id, enable, playerAudioLevelThreshold, playerDetectTimeMs, playerHoldTimeMs, primaryDuckingPercent
 * @return      :  {success: true}
 */

TEST_F(SAPInitializedTest, SAPSetSmartVolControl) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("setSmartVolControl"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(",\"enable\": true, \"playerAudioLevelThreshold\": 0.1, \"playerDetectTimeMs\": 200, \"playerHoldTimeMs\": 1000, \"primaryDuckingPercent\": 1 }"), 
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":true}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPSetSmartVolControlThresholdLessThanZero
 * @brief : Set Thresholdvaluse < 0 which result in failure
 * 
 * @param[in]   :  threshold = -1 
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPSetSmartVolControlThresholdLessThanZero) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("setSmartVolControl"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(",\"enable\": true, \"playerAudioLevelThreshold\": -1, \"playerDetectTimeMs\": 200, \"playerHoldTimeMs\": 1000, \"primaryDuckingPercent\": 1 }"), 
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPSetSmartVolControlDetectTimeMsLessThanZero
 * @brief : Set detectTimeMs < 0 which result in failure
 * 
 * @param[in]   :   detectTimeMs = -1 
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPSetSmartVolControlDetectTimeMsLessThanZero) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("setSmartVolControl"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(",\"enable\": true, \"playerAudioLevelThreshold\": 0.1, \"playerDetectTimeMs\": -1, \"playerHoldTimeMs\": 1000, \"primaryDuckingPercent\": 1 }"), 
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPSetSmartVolControlHoldTimeMsLessThanZero
 * @brief : Set holdTimeMs < 0 which result in failure
 * 
 * @param[in]   :  holdTimeMs = -1 
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPSetSmartVolControlHoldTimeMsLessThanZero) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("setSmartVolControl"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(",\"enable\": true, \"playerAudioLevelThreshold\": 0.1, \"playerDetectTimeMs\": 200, \"playerHoldTimeMs\": -1, \"primaryDuckingPercent\": 1 }"), 
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPSetSmartVolControlDuckPercentLessThanZero
 * @brief : Set primaryDuckingPercent < 0 which result in failure
 * 
 * @param[in]   :  primaryDuckingPercent = -1 
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPSetSmartVolControlDuckPercentLessThanZero) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("setSmartVolControl"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(",\"enable\": true, \"playerAudioLevelThreshold\": 0.1, \"playerDetectTimeMs\": 200, \"playerHoldTimeMs\": 1000, \"primaryDuckingPercent\": -1 }"), 
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPSetSmartVolControlDuckPercentLessThanZero
 * @brief : Set primaryDuckingPercent < 0 which result in failure
 * 
 * @param[in]   :  primaryDuckingPercent = -1 
 * @return      :  {success: false}
 */

TEST_F(SAPInitializedTest, SAPSetSmartVolControlDuckPercentGreaterThan100) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("setSmartVolControl"),
            _T("{\"id\": ") + std::to_string(speechId) + _T(",\"enable\": true, \"playerAudioLevelThreshold\": 0.1, \"playerDetectTimeMs\": 200, \"playerHoldTimeMs\": 1000, \"primaryDuckingPercent\": 101 }"), 
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":false}"));
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}

/**
 * @name  : SAPSetSmartVolControlWithoutOpen
 * @brief : SettingSmartVolControl for invalidPlayer
 * 
 * @param[in]   :  id, enable, playerAudioLevelThreshold, playerDetectTimeMs, playerHoldTimeMs, primaryDuckingPercent
 * @return      :  {success: false}
 */
TEST_F(SAPInitializedTest,SAPSetSmartVolControlWithoutOpen) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("setSmartVolControl"), 
        _T("{\"id\": 1 , \"enable\": true, \"playerAudioLevelThreshold\": 0.1, \"playerDetectTimeMs\": 200, \"playerHoldTimeMs\": 1000, \"primaryDuckingPercent\": 1 }"), 
        response
    ));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

/*******************************************************************************************************************
 * Test function for getPlayerSessionId
 * getPlayerSessionId                 :
 *                Gets the session ID from the provided the URL. The session is the ID returned in open cal.
 *                
 *                @return Response object contains session id and success status
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 2
 ********************************************************************************************************************/
/**
 * @name  : SAPGetPlayerSessionId
 * @brief : Open the player and play the url and get sessionid for it
 * 
 * @param[in]   :  id , url
 * @return      :  {success: true}
 */
TEST_F(SAPInitializedTest, SAPGetPlayerSessionId) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
        _T("open"), 
        _T("{\"audiotype\": \"pcm\",\"sourcetype\": \"websocket\",\"playmode\": \"system\" }"),
         response
    ));

    size_t idPos = response.find("\"id\"");
    if (idPos != string::npos) {
        size_t idStart = response.find(':', idPos) + 1;
        size_t idEnd = response.find(',', idPos);
        std::string idSubstring = response.substr(idStart, idEnd - idStart);
        int speechId = std::stoi(idSubstring);
        std::cout << "Speech ID: " << speechId << std::endl;

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, 
            _T("play"), 
            _T("{\"id\": ") + std::to_string(speechId) + _T(", \"url\": \"ws://example.com/socket\" }"), 
            response
        ));
        EXPECT_EQ(response, _T("{\"success\":true}"));
        
        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection,
            _T("getPlayerSessionId"), 
            _T("{\"url\": \"ws://example.com/socket\" }"), 
            response
        ));
        size_t sesPos = response.find("\"sessionId\"");
        if (sesPos != string::npos) {
            size_t sesStart = response.find(':', sesPos) + 1;
            size_t sesEnd = response.find(',', sesPos);
            std::string sesSubstring = response.substr(sesStart, sesEnd - sesStart);
            int sessionId = std::stoi(sesSubstring);
            std::cout << "Session ID: " << sessionId << std::endl;
            EXPECT_EQ(speechId,sessionId);
        } else {
            EXPECT_TRUE(false) << "Error: 'sessionid' not found in the response.";
        }
    } else {
        EXPECT_TRUE(false) << "Error: 'id' not found in the response.";
    }
}
/**
 * @name  : SAPGetPlayerSessionIdInvalid
 * @brief : GetSessionId for non-existing url
 * 
 * @param[in]   :  id , url
 * @return      :  {success: true}
 */
TEST_F(SAPInitializedTest, SAPGetPlayerSessionIdInvalid) {
        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, 
            _T("getPlayerSessionId"), 
            _T("{\"url\": \"https://www.yahoo.com\" }"), 
            response
        ));
    EXPECT_EQ(response, _T("{\"sessionId\":-1,\"success\":true}")); 
}
/**
 * @name  : SAPGetPlayerSessionIdInvalidUrl
 * @brief : GetSessionId for invalid url
 * 
 * @param[in]   :  id , url
 * @return      :  {success: false}
 */
TEST_F(SAPInitializedTest, SAPGetPlayerSessionIdInvalidUrl) {
        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, 
            _T("getPlayerSessionId"), 
            _T("{\"url\": \"invalid\" }"), 
            response
        ));
    EXPECT_EQ(response, _T("{\"success\":false}")); 
}

class SAPWorkerBlocker {
private:
    struct State {
        std::mutex mutex;
        std::condition_variable cv;
        bool started{false};
        bool released{false};
    };

    class BlockingJob : public Core::IDispatch {
    public:
        explicit BlockingJob(const std::shared_ptr<State>& state) : _state(state) {}
        void Dispatch() override {
            std::unique_lock<std::mutex> lock(_state->mutex);
            _state->started = true;
            _state->cv.notify_all();
            _state->cv.wait(lock, [this] { return _state->released; });
        }
    private:
        std::shared_ptr<State> _state;
    };

    static void Submit(const std::shared_ptr<State>& state) {
#ifdef USE_THUNDER_R4
        Core::IWorkerPool::Instance().Submit(Core::ProxyType<Core::IDispatch>(Core::ProxyType<BlockingJob>::Create(state)));
#else
        Core::IWorkerPool::Instance().Submit(Core::proxy_cast<Core::IDispatch>(Core::ProxyType<BlockingJob>::Create(state)));
#endif
    }

public:
    SAPWorkerBlocker() : _state(std::make_shared<State>()) {
        // The fixture runs one internal worker; the second slot requires an external Join().
        Submit(_state);
    }

    ~SAPWorkerBlocker() { Release(); }

    bool WaitUntilBlocked() {
        std::unique_lock<std::mutex> lock(_state->mutex);
        return _state->cv.wait_for(lock, std::chrono::seconds(2), [this] { return _state->started; });
    }

    void Release() {
        {
            std::lock_guard<std::mutex> lock(_state->mutex);
            _state->released = true;
        }
        _state->cv.notify_all();
    }

    bool ReleaseAndDrain() {
        // A FIFO barrier proves queued power work ran; stopping the pool could discard it.
        auto barrier = std::make_shared<State>();
        barrier->released = true;
        Submit(barrier);
        Release();
        std::unique_lock<std::mutex> lock(barrier->mutex);
        return barrier->cv.wait_for(lock, std::chrono::seconds(2), [barrier] { return barrier->started; });
    }

private:
    std::shared_ptr<State> _state;
};

class SAPPowerManagerTest : public SAPInitializedTest {
protected:
    struct InterfaceRelease {
        void operator()(Exchange::ISystemAudioPlayer* interface) const { interface->Release(); }
    };

    Core::ProxyType<PowerManagerMock> powerManagerMock = Core::ProxyType<PowerManagerMock>::Create();
    std::unique_ptr<Exchange::ISystemAudioPlayer, InterfaceRelease> sap;
    Exchange::IPowerManager::IModePreChangeNotification* preChangeNotification{nullptr};
    const uint32_t clientId{42};

    void SetUp() override {
        sap.reset(static_cast<Exchange::ISystemAudioPlayer*>(plugin->QueryInterface(Exchange::ISystemAudioPlayer::ID)));
        ASSERT_NE(sap, nullptr);
        ON_CALL(service, QueryInterfaceByCallsign(Exchange::IPowerManager::ID, ::testing::StrEq("org.rdk.PowerManager")))
            .WillByDefault(::testing::Invoke(
                [this](const uint32_t, const string&) -> void* {
                    powerManagerMock->AddRef();
                    return static_cast<void*>(static_cast<Exchange::IPowerManager*>(&(*powerManagerMock)));
                }));
    }

    void TearDown() override {
        if (sap != nullptr) {
            SAPWorkerBlocker barrier;
            EXPECT_TRUE(barrier.WaitUntilBlocked());
            EXPECT_TRUE(barrier.ReleaseAndDrain());
            workerPool->Stop();
            sap.reset();
            DeinitializePlugin();
        }
    }

    void ActivatePowerManager() {
        ASSERT_NE(pluginNotification, nullptr);
#ifdef USE_THUNDER_R4
        pluginNotification->Activated("org.rdk.PowerManager", &service);
#else
        ON_CALL(service, Callsign()).WillByDefault(::testing::Return("org.rdk.PowerManager"));
        ON_CALL(service, State()).WillByDefault(::testing::Return(PluginHost::IShell::ACTIVATED));
        pluginNotification->StateChange(&service);
#endif
    }

    void DeactivatePowerManager() {
        ASSERT_NE(pluginNotification, nullptr);
#ifdef USE_THUNDER_R4
        pluginNotification->Deactivated("org.rdk.PowerManager", &service);
#else
        ON_CALL(service, Callsign()).WillByDefault(::testing::Return("org.rdk.PowerManager"));
        ON_CALL(service, State()).WillByDefault(::testing::Return(PluginHost::IShell::DEACTIVATION));
        pluginNotification->StateChange(&service);
#endif
    }

    void ConfigurePowerManager() {
        ::testing::InSequence sequence;
        EXPECT_CALL(*powerManagerMock, Register(::testing::Matcher<Exchange::IPowerManager::IModePreChangeNotification*>(::testing::_)))
            .WillOnce(::testing::Invoke([this](Exchange::IPowerManager::IModePreChangeNotification* notification) {
                preChangeNotification = notification;
                return Core::ERROR_NONE;
            }));
        EXPECT_CALL(*powerManagerMock, AddPowerModePreChangeClient("SystemAudioPlayer", ::testing::_))
            .WillOnce(::testing::DoAll(::testing::SetArgReferee<1>(clientId), ::testing::Return(Core::ERROR_NONE)));
        EXPECT_CALL(*powerManagerMock, RemovePowerModePreChangeClient(clientId))
            .WillOnce(::testing::Return(Core::ERROR_NONE));
        EXPECT_CALL(*powerManagerMock, Unregister(::testing::Matcher<const Exchange::IPowerManager::IModePreChangeNotification*>(::testing::_)))
            .WillOnce(::testing::Return(Core::ERROR_NONE));
        ActivatePowerManager();
    }
};

TEST_F(SAPPowerManagerTest, DeinitializeReleasesPowerMonitoring) {
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    SAPWorkerBlocker barrier;
    ASSERT_TRUE(barrier.WaitUntilBlocked());
    ASSERT_TRUE(barrier.ReleaseAndDrain());
    workerPool->Stop();
    sap.reset();

    DeinitializePlugin();

    EXPECT_TRUE(::testing::Mock::VerifyAndClearExpectations(&(*powerManagerMock)));
}

TEST_F(SAPPowerManagerTest, ConfigureDoesNotReleaseExistingMonitoring) {
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    EXPECT_EQ(Core::ERROR_ILLEGAL_STATE, sap->Configure(&service));

    EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(clientId, 44))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_STANDBY,
                                               Exchange::IPowerManager::POWER_STATE_ON, 44, 1);
}

TEST_F(SAPPowerManagerTest, RejectsStartsBeforeStandbyCleanupAndAllowsPlaybackAfterWakeup) {
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("open"),
        _T("{\"audiotype\":\"pcm\",\"sourcetype\":\"data\",\"playmode\":\"system\"}"), response));
    JsonObject openResponse;
    openResponse.FromString(response);
    const string id = std::to_string(static_cast<int>(openResponse["id"].Number()));
    const string parameters = _T("{\"id\":") + id + _T("}");
    const Exchange::IPowerManager::PowerState states[] = {
        Exchange::IPowerManager::POWER_STATE_STANDBY,
        Exchange::IPowerManager::POWER_STATE_STANDBY_LIGHT_SLEEP,
        Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP,
        Exchange::IPowerManager::POWER_STATE_OFF
    };
    int transactionId = 50;
    for (const auto state : states) {
        SAPWorkerBlocker blocker;
        ASSERT_TRUE(blocker.WaitUntilBlocked());
        const int standbyTransaction = ++transactionId;
        auto completed = std::make_shared<std::promise<void>>();
        auto completion = completed->get_future();
        if (state == Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP ||
            state == Exchange::IPowerManager::POWER_STATE_OFF) {
            EXPECT_CALL(*powerManagerMock, DelayPowerModeChangeBy(clientId, standbyTransaction, 5))
                .WillOnce(::testing::Return(Core::ERROR_NONE));
        }
        EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(clientId, standbyTransaction))
            .WillOnce(::testing::Invoke([completed](const uint32_t, const int) {
                completed->set_value();
                return Core::ERROR_NONE;
            }));
        preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_ON,
                                                   state, standbyTransaction, 1);
        const auto expectStartsRejected = [&] {
            EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("play"),
                _T("{\"id\":") + id + _T(",\"url\":\"data://blocked\"}"), response));
            EXPECT_EQ(response, _T("{\"success\":false}"));
            EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("playbuffer"),
                _T("{\"id\":") + id + _T(",\"data\":\"AAAAAA==\"}"), response));
            EXPECT_EQ(response, _T("{\"success\":false}"));
            EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("resume"), parameters, response));
            EXPECT_EQ(response, _T("{\"success\":false}"));
        };
        expectStartsRejected();
        EXPECT_TRUE(blocker.ReleaseAndDrain());
        EXPECT_EQ(std::future_status::ready, completion.wait_for(std::chrono::seconds(2)));
        expectStartsRejected();

        EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(clientId, ++transactionId))
            .WillOnce(::testing::Return(Core::ERROR_NONE));
        preChangeNotification->OnPowerModePreChange(state, Exchange::IPowerManager::POWER_STATE_ON, transactionId, 1);
        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("play"),
            _T("{\"id\":") + id + _T(",\"url\":\"data://allowed\"}"), response));
        EXPECT_EQ(response, _T("{\"success\":true}"));
        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("playbuffer"),
            _T("{\"id\":") + id + _T(",\"data\":\"AAAAAA==\"}"), response));
        EXPECT_EQ(response, _T("{\"success\":true}"));
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("close"), parameters, response));
}

TEST_F(SAPPowerManagerTest, PowerStopWaitsForInFlightPlayToFinish) {
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    std::shared_ptr<GstElement> pipeline;
    EXPECT_CALL(*p_systemAudioPlatformMock, systemAudioGeneratePipeline(::testing::_, ::testing::_, ::testing::_,
        ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([&pipeline](GstElement** playerPipeline, GstElement** source, GstElement*,
            GstElement** sink, GstElement**, AudioType, PlayMode, SourceType, bool) {
            *sink = gst_element_factory_make("fakesink", nullptr);
            if (*sink == nullptr) {
                ADD_FAILURE() << "GStreamer fakesink is required for the concurrent-start test";
                return false;
            }
            g_object_set(*source, "is-live", TRUE, nullptr);
            g_object_set(*sink, "async", FALSE, "sync", FALSE, nullptr);
            gst_bin_add_many(GST_BIN(*playerPipeline), *source, *sink, nullptr);
            pipeline = std::shared_ptr<GstElement>(GST_ELEMENT(gst_object_ref(*playerPipeline)),
                [](GstElement* element) { gst_object_unref(element); });
            return gst_element_link(*source, *sink) != FALSE;
        }));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("open"),
        _T("{\"audiotype\":\"pcm\",\"sourcetype\":\"data\",\"playmode\":\"system\"}"), response));
    ASSERT_NE(pipeline, nullptr);
    JsonObject openResponse;
    openResponse.FromString(response);
    const string id = std::to_string(static_cast<int>(openResponse["id"].Number()));

    struct StartBarrier {
        GstElement* pipeline;
        std::promise<void> entered;
        std::shared_future<void> release;
        std::atomic<bool> blocked{false};
    };
    std::promise<void> release;
    StartBarrier barrier{pipeline.get(), {}, release.get_future().share()};
    auto entered = barrier.entered.get_future();
    GstBus* bus = gst_element_get_bus(pipeline.get());
    gst_bus_set_sync_handler(bus, [](GstBus*, GstMessage* message, gpointer data) -> GstBusSyncReply {
        auto* barrier = static_cast<StartBarrier*>(data);
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_STATE_CHANGED &&
            GST_MESSAGE_SRC(message) == GST_OBJECT(barrier->pipeline)) {
            GstState oldState, newState, pending;
            gst_message_parse_state_changed(message, &oldState, &newState, &pending);
            if (newState == GST_STATE_READY && !barrier->blocked.exchange(true)) {
                barrier->entered.set_value();
                barrier->release.wait();
            }
        }
        return GST_BUS_PASS;
    }, &barrier, nullptr);
    auto completed = std::make_shared<std::promise<void>>();
    auto completion = completed->get_future();
    EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(clientId, 65))
        .WillOnce(::testing::Invoke([completed](const uint32_t, const int) {
            completed->set_value();
            return Core::ERROR_NONE;
        }));
    auto play = std::async(std::launch::async, [this, id] {
        string output;
        EXPECT_EQ(Core::ERROR_NONE, sap->Play(_T("{\"id\":") + id + _T(",\"url\":\"data://inflight\"}"), output));
        EXPECT_EQ(output, _T("{\"success\":true}"));
    });
    EXPECT_EQ(std::future_status::ready, entered.wait_for(std::chrono::seconds(2)));
    preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_ON,
                                               Exchange::IPowerManager::POWER_STATE_STANDBY, 65, 1);
    EXPECT_EQ(std::future_status::timeout, completion.wait_for(std::chrono::milliseconds(100)));
    release.set_value();
    play.get();
    EXPECT_EQ(std::future_status::ready, completion.wait_for(std::chrono::seconds(2)));
    gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
    gst_object_unref(bus);
    GstState current = GST_STATE_VOID_PENDING;
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_get_state(pipeline.get(), &current, nullptr, 0));
    EXPECT_EQ(GST_STATE_NULL, current);
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("close"), _T("{\"id\":") + id + _T("}"), response));
}

TEST_F(SAPPowerManagerTest, StopsActivePipelineBeforeCompletingDeepSleepPreChange) {
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    std::shared_ptr<GstElement> pipeline;
    EXPECT_CALL(*p_systemAudioPlatformMock, systemAudioGeneratePipeline(::testing::_, ::testing::_, ::testing::_,
        ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([&pipeline](GstElement** playerPipeline, GstElement** source, GstElement*,
            GstElement** sink, GstElement**, AudioType, PlayMode, SourceType, bool) {
            *sink = gst_element_factory_make("fakesink", nullptr);
            if (*sink == nullptr) {
                ADD_FAILURE() << "GStreamer fakesink is required for the active-player test";
                return false;
            }
            g_object_set(*source, "is-live", TRUE, nullptr);
            g_object_set(*sink, "async", FALSE, "sync", FALSE, nullptr);
            gst_bin_add_many(GST_BIN(*playerPipeline), *source, *sink, nullptr);
            pipeline = std::shared_ptr<GstElement>(GST_ELEMENT(gst_object_ref(*playerPipeline)),
                [](GstElement* element) { gst_object_unref(element); });
            return gst_element_link(*source, *sink) != FALSE;
        }));
    auto completed = std::make_shared<std::promise<void>>();
    auto completion = completed->get_future();
    ::testing::Sequence powerSequence;
    EXPECT_CALL(*powerManagerMock, DelayPowerModeChangeBy(clientId, 17, 5))
        .InSequence(powerSequence)
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("open"),
        _T("{\"audiotype\": \"pcm\", \"sourcetype\": \"data\", \"playmode\": \"system\"}"), response));
    JsonObject openResponse;
    openResponse.FromString(response);
    const int playerId = static_cast<int>(openResponse["id"].Number());
    const string playerParameters = _T("{\"id\": ") + std::to_string(playerId) + _T("}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("playbuffer"),
        _T("{\"id\": ") + std::to_string(playerId) + _T(",\"data\":\"AAAAAA==\"}"), response));
    const auto playingDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do {
        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isspeaking"), playerParameters, response));
        if (response == _T("{\"success\":true}")) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < playingDeadline);
    EXPECT_EQ(response, _T("{\"success\":true}"));
    EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(clientId, 17))
        .InSequence(powerSequence)
        .WillOnce(::testing::Invoke([completed, pipeline](const uint32_t, const int) {
            EXPECT_NE(pipeline, nullptr);
            if (pipeline != nullptr) {
                GstState state = GST_STATE_VOID_PENDING;
                EXPECT_NE(GST_STATE_CHANGE_FAILURE, gst_element_get_state(pipeline.get(), &state, nullptr, 0));
                EXPECT_EQ(GST_STATE_NULL, state);
            }
            completed->set_value();
            return Core::ERROR_NONE;
        }));

    preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_ON,
                                               Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP, 17, 1);
    EXPECT_EQ(std::future_status::ready, completion.wait_for(std::chrono::seconds(2)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isspeaking"), playerParameters, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("close"),
        _T("{\"id\": ") + std::to_string(playerId) + _T("}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(SAPPowerManagerTest, StopsStartingPipelineBeforeCompletingStandbyPreChange) {
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    std::shared_ptr<GstElement> pipeline;
    EXPECT_CALL(*p_systemAudioPlatformMock, systemAudioGeneratePipeline(::testing::_, ::testing::_, ::testing::_,
        ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([&pipeline](GstElement** playerPipeline, GstElement** source, GstElement*,
            GstElement** sink, GstElement**, AudioType, PlayMode, SourceType, bool) {
            *sink = gst_element_factory_make("fakesink", nullptr);
            if (*sink == nullptr) {
                ADD_FAILURE() << "GStreamer fakesink is required for the starting-player test";
                return false;
            }
            // Without input, a non-live appsrc cannot finish preroll.
            g_object_set(*source, "is-live", FALSE, nullptr);
            g_object_set(*sink, "async", TRUE, "sync", FALSE, nullptr);
            gst_bin_add_many(GST_BIN(*playerPipeline), *source, *sink, nullptr);
            pipeline = std::shared_ptr<GstElement>(GST_ELEMENT(gst_object_ref(*playerPipeline)),
                [](GstElement* element) { gst_object_unref(element); });
            return gst_element_link(*source, *sink) != FALSE;
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("open"),
        _T("{\"audiotype\":\"pcm\",\"sourcetype\":\"data\",\"playmode\":\"system\"}"), response));
    JsonObject openResponse;
    openResponse.FromString(response);
    const string playerId = std::to_string(static_cast<int>(openResponse["id"].Number()));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("play"),
        _T("{\"id\":") + playerId + _T(",\"url\":\"data://pending\"}"), response));
    ASSERT_NE(pipeline, nullptr);
    GstState current = GST_STATE_VOID_PENDING;
    GstState pending = GST_STATE_VOID_PENDING;
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_get_state(pipeline.get(), &current, &pending, 0));
    EXPECT_EQ(GST_STATE_READY, current);
    EXPECT_EQ(GST_STATE_PLAYING, pending);

    auto completed = std::make_shared<std::promise<void>>();
    auto completion = completed->get_future();
    EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(clientId, 43))
        .WillOnce(::testing::Invoke([completed, pipeline](const uint32_t, const int) {
            GstState current = GST_STATE_VOID_PENDING;
            GstState pending = GST_STATE_VOID_PENDING;
            EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_get_state(pipeline.get(), &current, &pending, 0));
            EXPECT_EQ(GST_STATE_NULL, current);
            EXPECT_EQ(GST_STATE_VOID_PENDING, pending);
            completed->set_value();
            return Core::ERROR_NONE;
        }));
    preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_ON,
                                               Exchange::IPowerManager::POWER_STATE_STANDBY, 43, 1);
    EXPECT_EQ(std::future_status::ready, completion.wait_for(std::chrono::seconds(2)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("close"),
        _T("{\"id\":") + playerId + _T("}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(SAPPowerManagerTest, AcknowledgesOtherPowerStates) {
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    const Exchange::IPowerManager::PowerState states[] = {
        Exchange::IPowerManager::POWER_STATE_ON,
        Exchange::IPowerManager::POWER_STATE_STANDBY,
        Exchange::IPowerManager::POWER_STATE_STANDBY_LIGHT_SLEEP,
        Exchange::IPowerManager::POWER_STATE_OFF
    };
    EXPECT_CALL(*powerManagerMock, DelayPowerModeChangeBy(clientId, 4, 5))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    int transactionId = 0;
    for (auto state : states) {
        ++transactionId;
        auto completed = std::make_shared<std::promise<void>>();
        auto completion = completed->get_future();
        EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(clientId, transactionId))
            .WillOnce(::testing::Invoke([completed](const uint32_t, const int) {
                completed->set_value();
                return Core::ERROR_NONE;
            }));
        preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_ON, state, transactionId, 1);
        ASSERT_EQ(std::future_status::ready, completion.wait_for(std::chrono::seconds(2)));
    }
}

TEST_F(SAPPowerManagerTest, RenewsPendingShutdownWithoutExceedingThirtySeconds) {
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    SAPWorkerBlocker blocker;
    ASSERT_TRUE(blocker.WaitUntilBlocked());
    auto firstExtension = std::make_shared<std::promise<void>>();
    auto lastExtension = std::make_shared<std::promise<void>>();
    auto completed = std::make_shared<std::promise<void>>();
    auto first = firstExtension->get_future();
    auto last = lastExtension->get_future();
    auto completion = completed->get_future();
    auto extensions = std::make_shared<int>(0);
    const auto start = std::chrono::steady_clock::now();
    EXPECT_CALL(*powerManagerMock, DelayPowerModeChangeBy(clientId, 23, 5))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*powerManagerMock, DelayPowerModeChangeBy(clientId, 23, 3))
        .Times(12)
        .WillRepeatedly(::testing::Invoke([extensions, firstExtension, start](const uint32_t, const int, const int) {
            if (++(*extensions) == 1) {
                EXPECT_GE(std::chrono::steady_clock::now() - start, std::chrono::seconds(4));
                firstExtension->set_value();
            }
            return Core::ERROR_NONE;
        }));
    EXPECT_CALL(*powerManagerMock, DelayPowerModeChangeBy(clientId, 23, 2))
        .WillOnce(::testing::Invoke([lastExtension](const uint32_t, const int, const int) {
            lastExtension->set_value();
            return Core::ERROR_NONE;
        }));
    EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(clientId, 23))
        .WillOnce(::testing::Invoke([completed](const uint32_t, const int) {
            completed->set_value();
            return Core::ERROR_NONE;
        }));

    preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_ON,
                                               Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP, 23, 1);
    EXPECT_EQ(std::future_status::timeout, completion.wait_for(std::chrono::milliseconds(0)));
    EXPECT_EQ(std::future_status::ready, first.wait_for(std::chrono::seconds(10)));
    EXPECT_EQ(std::future_status::ready, last.wait_for(std::chrono::seconds(27)));
    std::this_thread::sleep_until(start + std::chrono::seconds(31));
    EXPECT_EQ(std::future_status::timeout, completion.wait_for(std::chrono::milliseconds(0)));
    blocker.Release();
    EXPECT_EQ(std::future_status::ready, completion.wait_for(std::chrono::seconds(2)));
}

TEST_F(SAPPowerManagerTest, DisconnectCancelsMonitorAndQueuedCompletion) {
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    SAPWorkerBlocker blocker;
    ASSERT_TRUE(blocker.WaitUntilBlocked());
    EXPECT_CALL(*powerManagerMock, DelayPowerModeChangeBy(clientId, 31, 5))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(::testing::_, ::testing::_)).Times(0);

    preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_ON,
                                               Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP, 31, 1);
    DeactivatePowerManager();
    EXPECT_TRUE(blocker.ReleaseAndDrain());
}

TEST_F(SAPPowerManagerTest, ExplicitShutdownCancelsMonitoringWhilePowerJobRetainsImplementation) {
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    SAPWorkerBlocker blocker;
    ASSERT_TRUE(blocker.WaitUntilBlocked());
    EXPECT_CALL(*powerManagerMock, DelayPowerModeChangeBy(clientId, 66, 5))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(::testing::_, ::testing::_)).Times(0);
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("open"),
        _T("{\"audiotype\":\"pcm\",\"sourcetype\":\"data\",\"playmode\":\"system\"}"), response));
    JsonObject openResponse;
    openResponse.FromString(response);
    const string id = std::to_string(static_cast<int>(openResponse["id"].Number()));

    preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_ON,
                                               Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP, 66, 1);
    EXPECT_EQ(Core::ERROR_NONE, sap->Configure(nullptr));
    EXPECT_TRUE(::testing::Mock::VerifyAndClearExpectations(&(*powerManagerMock)));
    EXPECT_CALL(*powerManagerMock, DelayPowerModeChangeBy(::testing::_, ::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*powerManagerMock, RemovePowerModePreChangeClient(::testing::_)).Times(0);
    EXPECT_CALL(*powerManagerMock, Unregister(::testing::Matcher<const Exchange::IPowerManager::IModePreChangeNotification*>(::testing::_))).Times(0);
    EXPECT_EQ(Core::ERROR_NONE, sap->Configure(nullptr));
    // A callback already in transit must not reopen playback or restart monitoring.
    preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP,
                                               Exchange::IPowerManager::POWER_STATE_ON, 67, 1);
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("play"),
        _T("{\"id\":") + id + _T(",\"url\":\"data://blocked\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
    EXPECT_TRUE(blocker.ReleaseAndDrain());
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("close"), _T("{\"id\":") + id + _T("}"), response));
}

TEST_F(SAPPowerManagerTest, RollsBackFailedPreChangeClientRegistration) {
    ::testing::InSequence sequence;
    EXPECT_CALL(*powerManagerMock, Register(::testing::Matcher<Exchange::IPowerManager::IModePreChangeNotification*>(::testing::_)))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*powerManagerMock, AddPowerModePreChangeClient("SystemAudioPlayer", ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_CALL(*powerManagerMock, Unregister(::testing::Matcher<const Exchange::IPowerManager::IModePreChangeNotification*>(::testing::_)))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*powerManagerMock, RemovePowerModePreChangeClient(::testing::_)).Times(0);
    ActivatePowerManager();
}

TEST_F(SAPPowerManagerTest, FailedNotificationRegistrationDoesNotAddClient) {
    EXPECT_CALL(*powerManagerMock, Register(::testing::Matcher<Exchange::IPowerManager::IModePreChangeNotification*>(::testing::_)))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_CALL(*powerManagerMock, AddPowerModePreChangeClient(::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*powerManagerMock, RemovePowerModePreChangeClient(::testing::_)).Times(0);
    EXPECT_CALL(*powerManagerMock, Unregister(::testing::Matcher<const Exchange::IPowerManager::IModePreChangeNotification*>(::testing::_))).Times(0);
    ActivatePowerManager();
}

TEST_F(SAPPowerManagerTest, ReconnectionDiscardsPreviousConnectionsQueuedWork) {
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    SAPWorkerBlocker blocker;
    ASSERT_TRUE(blocker.WaitUntilBlocked());
    EXPECT_CALL(*powerManagerMock, DelayPowerModeChangeBy(clientId, 37, 5))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(clientId, 37)).Times(0);
    preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_ON,
                                               Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP, 37, 1);
    DeactivatePowerManager();
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(clientId, 38))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP,
                                               Exchange::IPowerManager::POWER_STATE_ON, 38, 1);
    EXPECT_TRUE(blocker.ReleaseAndDrain());
}

TEST_F(SAPPowerManagerTest, WakeupCancelsSupersededDeepSleepWork) {
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    SAPWorkerBlocker blocker;
    ASSERT_TRUE(blocker.WaitUntilBlocked());
    EXPECT_CALL(*powerManagerMock, DelayPowerModeChangeBy(clientId, 39, 5))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(clientId, 39)).Times(0);
    EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(clientId, 40))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_ON,
                                               Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP, 39, 1);
    preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP,
                                               Exchange::IPowerManager::POWER_STATE_ON, 40, 1);
    EXPECT_TRUE(blocker.ReleaseAndDrain());
}

TEST_F(SAPPowerManagerTest, CompletesEvenWhenDelayRequestFails) {
    ConfigurePowerManager();
    ASSERT_NE(preChangeNotification, nullptr);
    auto completed = std::make_shared<std::promise<void>>();
    auto completion = completed->get_future();
    EXPECT_CALL(*powerManagerMock, DelayPowerModeChangeBy(clientId, 41, 5))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_CALL(*powerManagerMock, PowerModePreChangeComplete(clientId, 41))
        .WillOnce(::testing::Invoke([completed](const uint32_t, const int) {
            completed->set_value();
            return Core::ERROR_NONE;
        }));
    preChangeNotification->OnPowerModePreChange(Exchange::IPowerManager::POWER_STATE_ON,
                                               Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP, 41, 1);
    EXPECT_EQ(std::future_status::ready, completion.wait_for(std::chrono::seconds(2)));
}
