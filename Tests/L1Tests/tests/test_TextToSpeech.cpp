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
#include "TextToSpeech.h"
#include "TextToSpeechImplementation.h"

#include "ServiceMock.h"
#include "COMLinkMock.h"
#include "FactoriesImplementation.h"
#include "WorkerPoolImplementation.h"
#include "ThunderPortability.h"
#include "systemaudioplatformmock.h"
#include "RfcApiMock.h"
#include "WPEFramework/interfaces/IAuthService.h"
#include "mockauthservices.h"
#include "NetworkManagerMock.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace WPEFramework;
using ::testing::Test;
using ::testing::NiceMock;

#define TTS_CONFIG_FILE_PATH "/etc/entservices/ttsConfig.json"

class TTSTest : public Test{
protected:
    Core::ProxyType<Plugin::TextToSpeech> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;

    SystemAudioPlatformAPIMock *p_systemAudioPlatformMock = nullptr;
    Core::ProxyType<Plugin::TextToSpeechImplementation> TextToSpeechImplementation;
    NiceMock<COMLinkMock> comLinkMock;
    NiceMock<ServiceMock> service;
    RfcApiImplMock  *p_rfcApiImplMock = nullptr;
    PLUGINHOST_DISPATCHER* dispatcher;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    NiceMock<FactoriesImplementation> factoriesImplementation;
    NiceMock<MockAuthService> authserviceMock;
    NiceMock<MockINetworkManager> networkManagerMock;
    GstElement* sourceMock;

    void mockTTSConfigure()
    {
        std::ofstream file(TTS_CONFIG_FILE_PATH, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            printf("kykumar open %s fail\n", TTS_CONFIG_FILE_PATH);
        }

        std::string json ="{\"endpoint\":\"http://example-tts-dummy.net/tts/v1/cdn/location?\","
                "\"secureendpoint\":\"https://example-tts-dummy.net/tts/v1/cdn/location?\","
                "\"localendpoint\":\"http://example-tts-dummy.net/nuanceEvetest/tts?\","
                "\"speechrate\":\"medium\","
                "\"language\":\"en-US\","
                "\"volume\":100,"
                "\"rate\":50,"
                "\"voices\":{\"en-US\":\"en\",\"es-MX\":\"es\",\"fr-CA\":\"fr\",\"en-GB\":\"en-GB\",\"de-DE\":\"de-DE\",\"it-IT\":\"it-IT\"},"
                "\"local_voices\":{\"en-US\":\"en\",\"es-MX\":\"es\",\"fr-CA\":\"fr\",\"en-GB\":\"en-GB\",\"de-DE\":\"de-DE\",\"it-IT\":\"it-IT\"}"
                "}";

        file << json;
        file.close();
        printf("TTS config written successfully.\n");
    }

    void cleanupTTSConfigFile()
    {
        std::ofstream file(TTS_CONFIG_FILE_PATH, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            printf("kykumar open %s fail\n", TTS_CONFIG_FILE_PATH);
        }
        file.close();
    }

    void mockTTSConfigureTTS2()
    {
        std::ofstream file(TTS_CONFIG_FILE_PATH, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            printf("kykumar open %s fail\n", TTS_CONFIG_FILE_PATH);
        }

        std::string json ="{\"endpoint\":\"http://example-tts-dummy.net/tts/v1/cdn/location?\","
                "\"secureendpoint\":\"https://example-tts-dummy.net/tts/v1/cdn/location?\","
                "\"localendpoint\":\"http://example-tts-dummy.net/nuanceEvetest/tts?\","
                "\"endpoint_type\":\"TTS2\","
                "\"speechrate\":\"medium\","
                "\"satplugincallsign\":\"org.rdk.AuthService\","
                "\"language\":\"en-US\","
                "\"volume\":100,"
                "\"rate\":50,"
                "\"voices\":{\"en-US\":\"en\",\"es-MX\":\"es\",\"fr-CA\":\"fr\",\"en-GB\":\"en-GB\",\"de-DE\":\"de-DE\",\"it-IT\":\"it-IT\"},"
                "\"local_voices\":{\"en-US\":\"en\",\"es-MX\":\"es\",\"fr-CA\":\"fr\",\"en-GB\":\"en-GB\",\"de-DE\":\"de-DE\",\"it-IT\":\"it-IT\"}"
                "}";

        file << json;
        file.close();
        printf("TTS config tts2 written successfully.\n");
    }
    TTSTest()
        : plugin(Core::ProxyType<Plugin::TextToSpeech>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
    p_systemAudioPlatformMock = new testing::NiceMock<SystemAudioPlatformAPIMock>;
    SystemAudioPlatformMockImpl::setImpl(p_systemAudioPlatformMock);
    p_rfcApiImplMock = new NiceMock<RfcApiImplMock>();
    RfcApi::setImpl(p_rfcApiImplMock);

        ON_CALL(service, COMLink())
            .WillByDefault(::testing::Invoke(
                  [this]() {
                        return &comLinkMock;
                    }));

#ifdef USE_THUNDER_R4
        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
			.WillByDefault(::testing::Invoke(
                  [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                        TextToSpeechImplementation = Core::ProxyType<Plugin::TextToSpeechImplementation>::Create();
                        return &TextToSpeechImplementation;
                    }));
#else
        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(TextToSpeechImplementation));
#endif /*USE_THUNDER_R4 */

        PluginHost::IFactories::Assign(&factoriesImplementation);

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
        plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&service);

        EXPECT_EQ(string(""), plugin->Initialize(&service));

    }

    virtual ~TTSTest() override
    {
        printf("kykumar destructor deinitialize\n");
        plugin->Deinitialize(&service);
        sleep(3);
        dispatcher->Deactivate();
        dispatcher->Release();

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();

        RfcApi::setImpl(nullptr);
        if (p_rfcApiImplMock != nullptr)
        {
            delete p_rfcApiImplMock;
            p_rfcApiImplMock = nullptr;
        }

        SystemAudioPlatformMockImpl::setImpl(nullptr);
        if (p_systemAudioPlatformMock != nullptr)
        {
            delete p_systemAudioPlatformMock;
            p_systemAudioPlatformMock = nullptr;
        }

        PluginHost::IFactories::Assign(nullptr);
    }
};

class TTSInitializedTest : public TTSTest {
protected:

    TTSInitializedTest() : TTSTest() {

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

    ON_CALL(authserviceMock, GetServiceAccessToken(::testing::_)) 
        .WillByDefault(::testing::Invoke( [](WPEFramework::Exchange::IAuthService::GetServiceAccessTokenResult& res) {
            printf("kykumar mock token retrieval\n");
            res.token = "mock_token";
            return Core::ERROR_NONE;
        }));

    ON_CALL(networkManagerMock, IsConnectedToInternet(testing::_, testing::_, testing::_))
        .WillByDefault(testing::Invoke([](string& ipversion, string& interface, WPEFramework::Exchange::INetworkManager::InternetStatus& status) {

            ipversion = "IPv4";
            interface = "eth0";
            status = WPEFramework::Exchange::INetworkManager::InternetStatus::INTERNET_FULLY_CONNECTED;

            return Core::ERROR_NONE;
        }));

    ON_CALL(*p_systemAudioPlatformMock, systemAudioGeneratePipeline(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([this](GstElement** pipeline, GstElement** source, GstElement* capsfilter,
                             GstElement** audioSink, GstElement** audioVolume,
                             AudioType type, PlayMode mode, SourceType sourceType, bool smartVolumeEnable) {

            *pipeline = gst_pipeline_new(NULL);
            *source = gst_element_factory_make("appsrc", NULL);
            this->sourceMock = *source;
            GstCaps* caps = gst_caps_new_simple("audio/x-raw", "format", G_TYPE_STRING, "S16LE", "layout", G_TYPE_STRING, "interleaved", "channels", G_TYPE_INT, 2, "rate", G_TYPE_INT, 44100, NULL);

            g_object_set(*source, "caps", caps, "format", GST_FORMAT_TIME, "is-live", TRUE, "block", TRUE, NULL);

            gst_caps_unref(caps);
            GstElement* convert = gst_element_factory_make("audioconvert", NULL);
            GstElement* resample = gst_element_factory_make("audioresample", NULL);
            *audioVolume = gst_element_factory_make("volume", "volume");
            *audioSink = gst_element_factory_make("fakesink", NULL);
            
            // Set sync=true to make fakesink respect audio timing instead of consuming instantly
            g_object_set(*audioSink, "sync", TRUE, NULL);

            bool result = TRUE;
                if (!*pipeline) {
                    printf("kykumar pipeline creation failed\n");
                }
                if (!*source) {
                    printf("kykumar fakesrc creation failed\n");
                }
                if (!convert) {
                    printf("kykumar audioconvert creation failed\n");
                }
                if (!*audioVolume) {
                    printf("kykumar audioVolume creation failed\n");
                }
                if (!*audioSink) {
                    printf("kykumar audioSink creation failed\n");
                }
                g_object_set(*audioSink, "sync", TRUE, NULL);

                gst_bin_add_many(GST_BIN(*pipeline), *source, convert, resample, *audioVolume, *audioSink, NULL);

                result = gst_element_link_many(*source, convert, resample, *audioVolume, *audioSink, NULL);
                if(!result) {
                    printf("kykumar Failed to link elements for  pipeline\n");
                }
                else {
                    printf("kykumar Successfully linked elements for  pipeline\n");
                }

            return result;
        }));

    }

    static gboolean push_data(gpointer data)
    {
        GstElement* appsrc = GST_ELEMENT(data);

        const int num_samples = 44100 / 10; // 100ms
        const int num_bytes = num_samples * 2 * 2; // 16-bit stereo

        GstBuffer* buffer = gst_buffer_new_allocate(NULL, num_bytes, NULL);

        GstMapInfo map;
        gst_buffer_map(buffer, &map, GST_MAP_WRITE);
        memset(map.data, 0, num_bytes);
        gst_buffer_unmap(buffer, &map);

        static guint64 timestamp = 0;

        GST_BUFFER_PTS(buffer) = timestamp;
        GST_BUFFER_DTS(buffer) = timestamp;

        GST_BUFFER_DURATION(buffer) =
            gst_util_uint64_scale(num_samples, GST_SECOND, 44100);

        timestamp += GST_BUFFER_DURATION(buffer);

        GstFlowReturn ret;
        g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);

        gst_buffer_unref(buffer);

        return ret == GST_FLOW_OK ? TRUE : FALSE;
    }

    virtual ~TTSInitializedTest() override = default;
};

TEST_F(TTSInitializedTest,RegisteredMethods) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("enabletts")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("cancel")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getapiversion")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getspeechstate")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getttsconfiguration")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("isspeaking")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("isttsenabled")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("listvoices")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("pause")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("resume")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setttsconfiguration")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("speak")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setACL")));
}

/*******************************************************************************************************************
 * Test function for enableTTS
 * enableTTS    :
 *                Enables or disables the TTS conversion processing
 *
 *                @return Response object contains TTS_Status and success
 * Use case coverage:
 *                @Success : 3
 *                @Failure : 0
 ********************************************************************************************************************/
/**
 * @name  : EnableTTSDefault
 * @brief : Enables or disables the TTS conversion processing
 *
 * @param[in]   :  enabletts
 * @return      :  TTS_Status = 0 and success = true
 */

TEST_F(TTSInitializedTest,EnableTTSDefault) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("enabletts"), _T("{\"enabletts\": \"true\"}"), response));
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"TTS_Status\":0")));
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"success\":true")));
}

/**
 * @name  : EnableTTSTrue
 * @brief : Enable with enabletts = true and checked if isttsenabled == true
 *
 * @param[in]   :  enabletts = true
 * @return      :  isttsenabled = true; TTS_Status = 0 and success = true
 */

TEST_F(TTSInitializedTest,EnableTTSTrue) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("enabletts"), _T("{\"enabletts\": true }"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isttsenabled"), _T(""), response));
    EXPECT_EQ(response, _T("{\"isenabled\":true,\"TTS_Status\":0,\"success\":true}"));
}

/**
 * @name  : EnableTTSFalse
 * @brief : Enable with enabletts = false and checked if isttsenabled == false
 *
 * @param[in]   :  enabletts = false
 * @return      :  isttsenabled = false; TTS_Status = 0 and success = true
 */

TEST_F(TTSInitializedTest,EnableTTSFalse) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("enabletts"), _T("{\"enabletts\": false }"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isttsenabled"), _T(""), response));
    EXPECT_EQ(response, _T("{\"isenabled\":false,\"TTS_Status\":0,\"success\":true}"));
}

/*******************************************************************************************************************
 * Test function for getAPIVersion
 * getAPIVersion :
 *                Gets the API Version.
 *
 *                @return Response object contains version and success
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 0
 ********************************************************************************************************************/
/**
 * @name  : GetAPIVersion
 * @brief : Gets the API Version.
 *
 * @param[in]   :  NONE
 * @return      :  version = 1 and success = true
 */

TEST_F(TTSInitializedTest,GetAPIVersion) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getapiversion"), _T(""), response));
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"version\":1")));
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"success\":true")));
}

/*******************************************************************************************************************
 * Test function for isTTSEnabled
 * isTTSEnabled    :
 *                Returns whether the TTS engine is enabled or disabled. By default the TTS engine is disabled.
 *
 *                @return Response object contains isenabled, TTS_Status and success
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 0
 ********************************************************************************************************************/
/**
 * @name  : IsTTSEnabledDefault
 * @brief : Returns whether the TTS engine is enabled or disabled. By default the TTS engine is disabled.
 *
 * @param[in]   :  NONE
 * @return      :  isenabled = false; TTS_Status = 0; success = true
 */

TEST_F(TTSInitializedTest,IsTTSEnabledDefault) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isttsenabled"), _T(""), response));
    EXPECT_EQ(response, _T("{\"isenabled\":false,\"TTS_Status\":0,\"success\":true}"));
}

/*******************************************************************************************************************
 * Test function for listVoices
 * SpeechState          :
 *                Returns voice based on language
 *
 *                @return Response object contains speaking and success
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 4
 ********************************************************************************************************************/
/**
 * @name  : IsListVoicesEmpty
 * @brief : Returns speaking(true,false) of the given speechid
 *
 * @param[in]   :  language
 * @return      :  ERROR_NONE
 */

TEST_F(TTSInitializedTest,IsListVoicesEmpty) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("listvoices"), _T("{\"language\":\"en-US\"}"), response));
    EXPECT_EQ(response, _T("{\"voices\":[],\"TTS_Status\":0,\"success\":true}"));
}

/**
 * @name  : ListVoicesSetEmptyLanguage
 * @brief : Set language as empty and check whether it return error
 *
 * @param[in]   :  Set "" in language
 * @expected    :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, ListVoicesSetEmptyLanguage) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("listvoices"), _T("{\"language\": \"\"}"), response));
}

/**
 * @name  : ListVoicesSetWhiteSpaceLanguage
 * @brief : Set language as " " and check whether it return error
 *
 * @param[in]   :  Set " " in language
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, ListVoicesSetWhiteSpaceAsLanguage) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("listvoices"), _T("{\"language\": \"  \"}"), response));
}

/**
 * @name  : ListVoicesSetNumberAsLanguage
 * @brief : Set language as Number and check whether it return error
 *
 * @param[in]   :  Set "" in language
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, ListVoicesSetNumberAsLanguage) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("listvoices"), _T("{\"language\": 01}"), response));
}

/**
 * @name  : ListVoicesSetNullLanguage
 * @brief : Set language as NULL and check whether it return error
 *
 * @param[in]   :  Set NULL language
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, ListVoicesSetNullLanguage) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("listvoices"), _T("{\"language\": NULL}"), response));
}

/*******************************************************************************************************************
 * Test function for speak
 * speak          :
 *                Converts the input text to speech when TTS is enabled. Any ongoing speech is interrupted and the newly requested speech is processed.
 *
 *                @return Response speechid, TTS_Status and success
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 0
 ********************************************************************************************************************/
/**
 * @name  : Speak
 * @brief : Converts the input text to speech when TTS is enabled. Any ongoing speech is interrupted and the newly requested speech is processed.
 *
 * @param[in]   :  text
 * @return      :  ERROR_NONE
 */

TEST_F(TTSInitializedTest,Speak) {
    mockTTSConfigure();
    EXPECT_EQ(string(""), plugin->Initialize(&service));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("speak"), _T("{\"text\": \"speech_123\"}"), response));
    sleep(3);

    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"speechid\"")));
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"TTS_Status\":0")));
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"success\":true")));
    cleanupTTSConfigFile();
}

/*******************************************************************************************************************
 * Test function for IsSpeaking
 * SpeechState          :
 *                Returns whether current speechid is speaking or not
 *
 *                @return Response object contains speaking and success
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 2
 ********************************************************************************************************************/
/**
 * @name  : IsSpeaking
 * @brief : Returns speaking(true,false) of the given speechid
 *
 * @param[in]   :  speechid
 * @return      :  ERROR_NONE
 */

TEST_F(TTSInitializedTest,IsSpeaking) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isspeaking"), _T("{\"speechid\": 1}"), response));
    EXPECT_EQ(response, _T("{\"speaking\":false,\"TTS_Status\":0,\"success\":true}"));
}

/**
 * @name  : IsSpeakingEmptySpeechId
 * @brief : Given a empty speechid it should return error
 *
 * @param[in]   :  "" in speechId
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest,IsSpeakingEmptySpeechId) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("isspeaking"), _T("{\"speechid\": \"\"}"), response));
}

/**
 * @name  : IsSpeakingStringAsSpeechId
 * @brief : Given a string speechid it should return error
 *
 * @param[in]   :  "hello" in speechId
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest,IsSpeakingStringAsSpeechId) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("isspeaking"), _T("{\"speechid\": \"hello\"}"), response));
}

/*******************************************************************************************************************
 * Test function for GetSpeechState
 * SpeechState          :
 *                Returns the SpeechState of the given speechid
 *
 *                @return Response object contains speechstate and success
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 2
 ********************************************************************************************************************/
/**
 * @name  : SpeechState
 * @brief : Returns the SpeechState of the given speechid
 *
 * @param[in]   :  speechid
 * @return      :  ERROR_NONE
 */

TEST_F(TTSInitializedTest,SpeechState) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getspeechstate"), _T("{\"speechid\": 1}"), response));
    EXPECT_EQ(response, _T("{\"speechstate\":3,\"TTS_Status\":0,\"success\":true}"));
}

/**
 * @name  : GetSpeechStateEmptySpeechId
 * @brief : Given a empty speechid it should return error
 *
 * @param[in]   :  "" in speechId
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest,GetSpeechStateEmptySpeechId) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getspeechstate"), _T("{\"speechid\": \"\"}"), response));
}

/**
 * @name  : GetSpeechStateStringAsSpeechId
 * @brief : Given a string speechid it should return error
 *
 * @param[in]   :  "hello" in speechId
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest,GetSpeechStateStringAsSpeechId) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getspeechstate"), _T("{\"speechid\": \"hello\"}"), response));
}

/*******************************************************************************************************************
 * Test function for Cancel
 * cancel          :
 *                Cancel the speech currently happening
 *
 *                @return Response object contains TTS_Status and success
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 2
 ********************************************************************************************************************/
/**
 * @name  : Cancel
 * @brief : Given a speechid it should cancel it
 *
 * @param[in]   :  Valid configuration
 * @return      :  ERROR_NONE
 */

TEST_F(TTSInitializedTest,Cancel) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("cancel"), _T("{\"speechid\": 1}"), response));
    EXPECT_EQ(response, _T("{\"TTS_Status\":0,\"success\":true}"));
}

/**
 * @name  : CancelEmptySpeechId
 * @brief : Given a speechid it should return error
 *
 * @param[in]   :  "" in speechId
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest,CancelEmptySpeechId) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("cancel"), _T("{\"speechid\": \"\"}"), response));
}

/**
 * @name  : CancelStringAsSpeechId
 * @brief : Given a string speechid it should return error
 *
 * @param[in]   :  "hello" in speechId
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest,CancelStringAsSpeechId) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("cancel"), _T("{\"speechid\": \"hello\"}"), response));
}

/*******************************************************************************************************************
 * Test function for Pause
 * cancel          :
 *                Cancel the speech currently happening
 *
 *                @return Response object contains TTS_Status and success
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 2
 ********************************************************************************************************************/
/**
 * @name  : Pause
 * @brief : Given a speechid it should cancel it
 *
 * @param[in]   :  Valid configuration
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest,Pause) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("pause"), _T("{\"speechid\": 1}"), response));
}

/**
 * @name  : PauseEmptySpeechId
 * @brief : Given a speechid it should return error
 *
 * @param[in]   :  "" in speechId
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest,PauseEmptySpeechId) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("pause"), _T("{\"speechid\":\"\"}"), response));
}

/**
 * @name  : PauseStringAsSpeechId
 * @brief : Given a string speechid it should return error
 *
 * @param[in]   :  "hello" in speechId
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest,PauseStringAsSpeechId) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("pause"), _T("{\"speechid\": \"hello\"}"), response));
}

/*******************************************************************************************************************
 * Test function for resume
 * resume          :
 *                resume the speech which is resumed
 *
 *                @return Response object contains TTS_Status and success
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 2
 ********************************************************************************************************************/
/**
 * @name  : Resume
 * @brief : Given a speechid it should resume it
 *
 * @param[in]   :  Valid configuration
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest,Resume) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("resume"), _T("{\"speechid\": 1}"), response));
}

/**
 * @name  : ResumeEmptySpeechId
 * @brief : Given a speechid it should return error
 *
 * @param[in]   :  "" in speechId
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest,ResumeEmptySpeechId) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("resume"), _T("{\"speechid\":\"\"}"), response));
}

/**
 * @name  : ResumeStringAsSpeechId
 * @brief : Given a string speechid it should return error
 *
 * @param[in]   :  "hello" in speechId
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest,ResumeStringAsSpeechId) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("resume"), _T("{\"speechid\": \"hello\"}"), response));
}

/*******************************************************************************************************************
 * Test function for :setttsconfiguration
 * setttsconfiguration :
 *                Set The TTS configurations based on params
 *
 *                @return Response object contains TTS_Status and success
 * Use case coverage:
 *                @Success : 2
 *                @Failure : 34
 ********************************************************************************************************************/
/**
 * @name  : SetTTSConfiguration
 * @brief : Given a valid JSON request check whether response is success
 *
 * @param[in]   :  Valid configuration
 * @return      :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetTTSConfiguration) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(
        connection,
        _T("setttsconfiguration"),
        _T("{\"language\": \"en-US\",\"voice\": \"carol\","
            "\"ttsendpoint\":\"http://example-tts-dummy.net/tts/v1/cdn/location?\","
            "\"ttsendpointsecured\":\"https://example-tts-dummy.net/tts/v1/cdn/location?\"}"
        ),
        response
    ));

    EXPECT_EQ(response, _T("{\"TTS_Status\":0,\"success\":true}"));
}

/**
 * @name  : DefaultTTSConfiguration
 * @brief : Update the value and check the value using gettsconfiguration
 *
 * @param[in]   :  Valid configuration
 * @expected    :  getttsconfiguration should return the update value
 */

TEST_F(TTSInitializedTest, DefaultTTSConfiguration) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(
        connection,
        _T("setttsconfiguration"),
        _T("{\"language\": \"en-US\",\"voice\": \"carol\","
            "\"ttsendpoint\":\"http://example-tts-dummy.net/tts/v1/cdn/location?\","
            "\"ttsendpointsecured\":\"https://example-tts-dummy.net/tts/v1/cdn/location?\","
            "\"volume\": \"95\",\"primvolduckpercent\": \"50\",\"rate\": \"40\",\"speechrate\":\"medium\"}"
        ),
        response
    ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));

    EXPECT_EQ(response,
        _T("{\"ttsendpoint\":\"http:\\/\\/example-tts-dummy.net\\/tts\\/v1\\/cdn\\/location?\","
            "\"ttsendpointsecured\":\"https:\\/\\/example-tts-dummy.net\\/tts\\/v1\\/cdn\\/location?\","
            "\"language\":\"en-US\",\"voice\":\"carol\",\"speechrate\":\"medium\",\"rate\":40,\"volume\":\"95\","
            "\"TTS_Status\":0,\"success\":true}"
        ));
}

/**
 * @name  : SetInvalidTTSEndpoint
 * @brief : Set Invalid URL in ttsendpoint and check it return error
 *
 * @param[in]   :  Set invalid url in ttsendpoint
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetInvalidTTSEndpoint) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection,
        _T("setttsconfiguration"),
        _T("{\"language\": \"en-US\",\"voice\": \"carol\",\"ttsendpoint\":\"invalid1@#\","
           "\"ttsendpointsecured\":\"https://localhost:50050/nuanceEve/tts?\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t ttsEndPointPos = response.find("\"ttsendpoint\"");

    if (ttsEndPointPos  != string::npos) {
        size_t ttsEndpointStart = response.find(':', ttsEndPointPos ) + 1;
        size_t ttsEndpointEnd = response.find(',', ttsEndPointPos );
        std::string ttsSubstring = response.substr(ttsEndpointStart ,ttsEndpointEnd  - ttsEndpointStart );
        EXPECT_EQ(ttsSubstring ,"\"\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'ttsendpoint' not found in the response.";
    }
}

/**
 * @name  : SetInvalidSecuredTTSEndpoint
 * @brief : Set Invalid URL in ttssecuredendpoint and check it return error
 *
 * @param[in]   :  Set invalid url in ttsendpoint
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetInvalidtSecuredTTSEndpoint) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection,
        _T("setttsconfiguration"),
        _T("{\"language\": \"en-US\",\"voice\": \"carol\","
           "\"ttsendpoint\":\"http://localhost:50050/nuanceEve/tts?\",\"ttsendpointsecured\":\"invalid1@#\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t ttsendpointsecuredPos = response.find("\"ttsendpointsecured\"");

    if (ttsendpointsecuredPos  != string::npos) {
        size_t ttsendpointsecuredStart = response.find(':', ttsendpointsecuredPos ) + 1;
        size_t ttsendpointsecuredEnd = response.find(',', ttsendpointsecuredPos );
        std::string ttsendpointsecuredSubstring = response.substr(ttsendpointsecuredStart ,ttsendpointsecuredEnd  - ttsendpointsecuredStart );
        EXPECT_EQ(ttsendpointsecuredSubstring ,"\"\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'ttsEndpointSecured' not found in the response.";
    }
}

/**
 * @name  : SetEmptyTTSEndpoint
 * @brief : Set Empty URL in ttsendpoint and check it returns error
 *
 * @param[in]   :  Set "" in ttsendpoint
 * @expected    :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetEmptyTTSEndpoint) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"ttsendpoint\":\"\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t ttsEndPointPos = response.find("\"ttsendpoint\"");

    if (ttsEndPointPos  != string::npos) {
        size_t ttsEndpointStart = response.find(':', ttsEndPointPos ) + 1;
        size_t ttsEndpointEnd = response.find(',', ttsEndPointPos );
        std::string ttsSubstring = response.substr(ttsEndpointStart ,ttsEndpointEnd  - ttsEndpointStart );
        EXPECT_EQ(ttsSubstring ,"\"\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'ttsendpoint' not found in the response.";
    }
}

/**
 * @name  : SetEmptySecuredTTSEndpoint
 * @brief : Set Empty URL in ttssecuredendpoint and check it returns error
 *
 * @param[in]   :  Set "" in ttsendpoint
 * @expected    :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetEmptySecuredTTSEndpoint) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"ttssecuredendpoint\":\"\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t ttsEndPointPos = response.find("\"ttsendpointsecured\"");

    if (ttsEndPointPos  != string::npos) {
        size_t ttsEndpointStart = response.find(':', ttsEndPointPos ) + 1;
        size_t ttsEndpointEnd = response.find(',', ttsEndPointPos );
        std::string ttsSubstring = response.substr(ttsEndpointStart ,ttsEndpointEnd  - ttsEndpointStart );
        EXPECT_EQ(ttsSubstring ,"\"\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'ttssecuredendpoint' not found in the response.";
    }
}

/**
 * @name  : SetNullTTSEndpoint
 * @brief : Set NULL  in ttsendpoint and check it returns error
 *
 * @param[in]   :  Set NULL in ttsendpoint
 * @expected    :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetNULLTTSEndpoint) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"ttsendpoint\": null}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t ttsEndPointPos = response.find("\"ttsendpoint\"");

    if (ttsEndPointPos  != string::npos) {
        size_t ttsEndpointStart = response.find(':', ttsEndPointPos ) + 1;
        size_t ttsEndpointEnd = response.find(',', ttsEndPointPos );
        std::string ttsSubstring = response.substr(ttsEndpointStart ,ttsEndpointEnd  - ttsEndpointStart );
        EXPECT_EQ(ttsSubstring ,"\"\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'ttsendpoint' not found in the response.";
    }
}

/**
 * @name  : SetNULLSecuredTTSEndpoint
 * @brief : Set NULL in ttssecuredendpoint and check it returns error
 *
 * @param[in]   :  Set NULL in ttsendpoint
 * @expected    :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetNULLSecuredTTSEndpoint) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"ttssecuredendpoint\": null}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t ttsendpointsecuredPos = response.find("\"ttsendpointsecured\"");

    if (ttsendpointsecuredPos  != string::npos) {
        size_t ttsendpointsecuredStart = response.find(':', ttsendpointsecuredPos ) + 1;
        size_t ttsendpointsecuredEnd = response.find(',', ttsendpointsecuredPos );
        std::string ttsendpointsecuredSubstring = response.substr(ttsendpointsecuredStart ,ttsendpointsecuredEnd  - ttsendpointsecuredStart );
        EXPECT_EQ(ttsendpointsecuredSubstring ,"\"\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'ttsendpointsecured' not found in the response.";
    }
}

/**
 * @name  : SetStringAsVolume
 * @brief : Set string in Volume and check it returns error
 *
 * @param[in]   :  Set "invalid" in Volume
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetStringAsVolume) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"volume\": \"invalid\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t volumePos = response.find("\"volume\"");

    if (volumePos  != string::npos) {
        size_t volumeStart = response.find(':', volumePos ) + 1;
        size_t volumeEnd = response.find(',', volumePos );
        std::string volumeSubstring = response.substr(volumeStart ,volumeEnd  - volumeStart );
        EXPECT_EQ(volumeSubstring ,"\"95\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'volume' not found in the response.";
    }
}

/**
 * @name  : SetVolumeLessThanMinValue
 * @brief : Set volume as -1 and check whether it return error
 *
 * @param[in]   :  Set -1 in Volume
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetVolumeLessThanMinValue) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"volume\": -1}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t volumePos = response.find("\"volume\"");

    if (volumePos  != string::npos) {
        size_t volumeStart = response.find(':', volumePos ) + 1;
        size_t volumeEnd = response.find(',', volumePos );
        std::string volumeSubstring = response.substr(volumeStart ,volumeEnd  - volumeStart );
        EXPECT_EQ(volumeSubstring ,"\"95\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'volume' not found in the response.";
    }
}

/**
 * @name  : SetVolumeLessThanMaxValue
 * @brief : Set volume as 101 and check whether it return error
 *
 * @param[in]   :  Set 101 in Volume
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetVolumeGreaterThanMaxValue) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"volume\": 101}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t volumePos = response.find("\"volume\"");

    if (volumePos  != string::npos) {
        size_t volumeStart = response.find(':', volumePos ) + 1;
        size_t volumeEnd = response.find(',', volumePos );
        std::string volumeSubstring = response.substr(volumeStart ,volumeEnd  - volumeStart );
        EXPECT_EQ(volumeSubstring ,"\"95\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'volume' not found in the response.";
    }
}

/**
 * @name  : SetEmptyVolume
 * @brief : Set volume as empty and check whether it return error
 *
 * @param[in]   :  Set  in Volume
 * @expected    :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetEmptyVolume) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"volume\":\"\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t volumePos = response.find("\"volume\"");

    if (volumePos  != string::npos) {
        size_t volumeStart = response.find(':', volumePos ) + 1;
        size_t volumeEnd = response.find(',', volumePos );
        std::string volumeSubstring = response.substr(volumeStart ,volumeEnd  - volumeStart );
        EXPECT_EQ(volumeSubstring ,"\"95\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'ttsendpoint' not found in the response.";
    }
}

/**
 * @name  : SetStringAsRate
 * @brief : Set string in rate and check it returns error
 *
 * @param[in]   :  Set "invalid" in rate
 * @expected    :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetStringAsRate) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"rate\": \"invalid\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t ratePos = response.find("\"rate\"");

    if (ratePos  != string::npos) {
        size_t rateStart = response.find(':', ratePos ) + 1;
        size_t rateEnd = response.find(',', ratePos );
        std::string rateSubstring = response.substr(rateStart ,rateEnd  - rateStart );
        EXPECT_EQ(rateSubstring ,"40");
     } else {
        EXPECT_TRUE(false) << "Error: 'rate' not found in the response.";
    }
}

/**
 * @name  : SetRateLessThanMinValue
 * @brief : Set rate as -1 and check whether it return error
 *
 * @param[in]   :  Set -1 in rate
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetRateLessThanMinValue) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"rate\": -1}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t ratePos = response.find("\"rate\"");

    if (ratePos  != string::npos) {
        size_t rateStart = response.find(':', ratePos ) + 1;
        size_t rateEnd = response.find(',', ratePos );
        std::string rateSubstring = response.substr(rateStart ,rateEnd  - rateStart );
        EXPECT_EQ(rateSubstring ,"40");
     } else {
        EXPECT_TRUE(false) << "Error: 'rate' not found in the response.";
    }
}

/**
 * @name  : SetRateGreaterThanMaxValue
 * @brief : Set rate as 101 and check whether it return error
 *
 * @param[in]   :  Set 101 in rate
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetRateGreaterThanMaxValue) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"rate\": 101}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t ratePos = response.find("\"rate\"");

    if (ratePos  != string::npos) {
        size_t rateStart = response.find(':', ratePos ) + 1;
        size_t rateEnd = response.find(',', ratePos );
        std::string rateSubstring = response.substr(rateStart ,rateEnd  - rateStart );
        EXPECT_EQ(rateSubstring ,"40");
     } else {
        EXPECT_TRUE(false) << "Error: 'rate' not found in the response.";
    }
}

/**
 * @name  : SetEmptyRate
 * @brief : Set rate as empty and check whether it return error
 *
 * @param[in]   :  Set  in rate
 * @expected    :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetEmptyRate) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"rate\":\"\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t ratePos = response.find("\"rate\"");

    if (ratePos  != string::npos) {
        size_t rateStart = response.find(':', ratePos ) + 1;
        size_t rateEnd = response.find(',', ratePos );
        std::string rateSubstring = response.substr(rateStart ,rateEnd  - rateStart );
        EXPECT_EQ(rateSubstring ,"40");
     } else {
        EXPECT_TRUE(false) << "Error: 'rate' not found in the response.";
    }
}

/**
 * @name  : SetStringAsPrimvolduckpercent
 * @brief : Set string in primvolduckpercent and check it returns error
 *
 * @param[in]   :  Set "invalid" in primvolduckpercent
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetStringAsprimvolduckpercent) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"primvolduckpercent\": \"invalid\"}"), response));
}

/**
 * @name  : SetPrimvolduckpercentLessThanMinValue
 * @brief : Set primvolduckpercent as -1 and check whether it return error
 *
 * @param[in]   :  Set -1 in primvolduckpercent
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetPrimvolduckpercentLessThanMinValue) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"primvolduckpercent\": -1}"), response));
}

/**
 * @name  : SetPrimvolduckpercentGreaterThanMaxValue
 * @brief : Set primvolduckpercent as 101 and check whether it return error
 *
 * @param[in]   :  Set 101 in primvolduckpercent
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetPrimvolduckpercentGreaterThanMaxValue) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"primvolduckpercent\": 101}"), response));
}

/**
 * @name  : SetEmptyPrimvolduckpercent
 * @brief : Set primvolduckpercent as empty and check whether it return error
 *
 * @param[in]   :  Set  in primvolduckpercent
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetEmptyPrimvolduckpercent) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"primvolduckpercent\": \"\"}"), response));
}

/**
 * @name  : SetEmptySpeechRate
 * @brief : Set speechrate as empty and check whether it return error
 *
 * @param[in]   :  Set "" in speechrate
 * @expected    :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetEmptySpeechRate) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"speechrate\": \"\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t speechRatePos = response.find("\"speechrate\"");

    if (speechRatePos  != string::npos) {
        size_t speechRatePosStart = response.find(':', speechRatePos ) + 1;
        size_t speechRatePosEnd = response.find(',', speechRatePos );
        std::string speechRateSubstring = response.substr(speechRatePosStart ,speechRatePosEnd  - speechRatePosStart);
        EXPECT_EQ(speechRateSubstring ,"\"medium\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'speechrate' not found in the response.";
    }
}

/**
 * @name  : SetWhiteSpaceSpeechRate
 * @brief : Set speechrate as " " and check whether it return error
 *
 * @param[in]   :  Set " " in speechrate
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetWhiteSpaceAsSpeechRate) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"speechrate\": \"  \"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t speechRatePos = response.find("\"speechrate\"");

    if (speechRatePos  != string::npos) {
        size_t speechRatePosStart = response.find(':', speechRatePos ) + 1;
        size_t speechRatePosEnd = response.find(',', speechRatePos );
        std::string speechRateSubstring = response.substr(speechRatePosStart ,speechRatePosEnd  - speechRatePosStart);
        EXPECT_EQ(speechRateSubstring ,"\"medium\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'speechrate' not found in the response.";
    }
}

/**
 * @name  : SetNumberAsSpeechRate
 * @brief : Set speechrate as Number and check whether it return error
 *
 * @param[in]   :  Set "" in speechrate
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetNumberAsSpeechRate) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"speechrate\": 01}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t speechRatePos = response.find("\"speechrate\"");

    if (speechRatePos  != string::npos) {
        size_t speechRatePosStart = response.find(':', speechRatePos ) + 1;
        size_t speechRatePosEnd = response.find(',', speechRatePos );
        std::string speechRateSubstring = response.substr(speechRatePosStart ,speechRatePosEnd  - speechRatePosStart);
        EXPECT_EQ(speechRateSubstring ,"\"medium\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'speechrate' not found in the response.";
    }
}

/**
 * @name  : SetNullSpeechRate
 * @brief : Set speechrate as NULL and check whether it return error
 *
 * @param[in]   :  Set NULL speechrate
 * @expected    :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetNullSpeechRate) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"speechrate\": null}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t speechRatePos = response.find("\"speechrate\"");

    if (speechRatePos  != string::npos) {
        size_t speechRatePosStart = response.find(':', speechRatePos ) + 1;
        size_t speechRatePosEnd = response.find(',', speechRatePos );
        std::string speechRateSubstring = response.substr(speechRatePosStart ,speechRatePosEnd  - speechRatePosStart);
        EXPECT_EQ(speechRateSubstring ,"\"medium\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'speechrate' not found in the response.";
    }
}

/**
 * @name  : SetInvalidSpeechRate
 * @brief : Set speechrate as Invalid It should be one of the following [slow,medium,fast,faster]
 *
 * @param[in]   :  Set invalid  speechrate
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetInvalidSpeechRate) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"speechrate\": \"invalid\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t speechRatePos = response.find("\"speechrate\"");

    if (speechRatePos  != string::npos) {
        size_t speechRatePosStart = response.find(':', speechRatePos ) + 1;
        size_t speechRatePosEnd = response.find(',', speechRatePos );
        std::string speechRateSubstring = response.substr(speechRatePosStart ,speechRatePosEnd  - speechRatePosStart);
        EXPECT_EQ(speechRateSubstring ,"\"medium\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'speechrate' not found in the response.";
    }
}

/**
 * @name  : SetEmptyLanguage
 * @brief : Set language as empty and check whether it return error
 *
 * @param[in]   :  Set "" in language
 * @expected    :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetEmptyLanguage) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"language\": \"\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t languagePos = response.find("\"language\"");

    if (languagePos  != string::npos) {
        size_t languagePosStart = response.find(':', languagePos ) + 1;
        size_t languagePosEnd = response.find(',', languagePos );
        std::string languageSubstring = response.substr(languagePosStart ,languagePosEnd  - languagePosStart);
        EXPECT_EQ(languageSubstring ,"\"en-US\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'language' not found in the response.";
    }
}

/**
 * @name  : SetWhiteSpaceLanguage
 * @brief : Set language as " " and check whether it return error
 *
 * @param[in]   :  Set " " in language
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetWhiteSpaceAsLanguage) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"language\": \"  \"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t languagePos = response.find("\"language\"");

    if (languagePos  != string::npos) {
        size_t languagePosStart = response.find(':', languagePos ) + 1;
        size_t languagePosEnd = response.find(',', languagePos );
        std::string languageSubstring = response.substr(languagePosStart ,languagePosEnd  - languagePosStart);
        EXPECT_EQ(languageSubstring ,"\"en-US\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'language' not found in the response.";
    }
}

/**
 * @name  : SetNumberAsLanguage
 * @brief : Set language as Number and check whether it return error
 *
 * @param[in]   :  Set "" in language
 * @expected    :  ERRERROR_GENERALOR_NONE
 */

TEST_F(TTSInitializedTest, SetNumberAsLanguage) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"language\": 01}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t languagePos = response.find("\"language\"");

    if (languagePos  != string::npos) {
        size_t languagePosStart = response.find(':', languagePos ) + 1;
        size_t languagePosEnd = response.find(',', languagePos );
        std::string languageSubstring = response.substr(languagePosStart ,languagePosEnd  - languagePosStart);
        EXPECT_EQ(languageSubstring ,"\"en-US\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'language' not found in the response.";
    }
}

/**
 * @name  : SetNullLanguage
 * @brief : Set language as NULL and check whether it return error
 *
 * @param[in]   :  Set NULL language
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetNullLanguage) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"language\": NULL}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t languagePos = response.find("\"language\"");

    if (languagePos  != string::npos) {
        size_t languagePosStart = response.find(':', languagePos ) + 1;
        size_t languagePosEnd = response.find(',', languagePos );
        std::string languageSubstring = response.substr(languagePosStart ,languagePosEnd  - languagePosStart);
        EXPECT_EQ(languageSubstring ,"\"en-US\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'language' not found in the response.";
    }
}

/**
 * @name  : SetEmptyVoice
 * @brief : Set voice as empty and check whether it return error
 *
 * @param[in]   :  Set "" in voice
 * @expected    :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetEmptyVoice) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"voice\": \"\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t voicePos = response.find("\"voice\"");

    if (voicePos  != string::npos) {
        size_t voicePosStart = response.find(':', voicePos ) + 1;
        size_t voicePosEnd = response.find(',', voicePos );
        std::string voiceSubstring = response.substr(voicePosStart ,voicePosEnd  - voicePosStart );
        EXPECT_EQ(voiceSubstring ,"\"carol\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'voice' not found in the response.";
    }
}

/**
 * @name  : SetWhiteSpaceVoice
 * @brief : Set voice as " " and check whether it return error
 *
 * @param[in]   :  Set " " in voice
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetWhiteSpaceAsVoice) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"voice\": \"  \"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t voicePos = response.find("\"voice\"");

    if (voicePos  != string::npos) {
        size_t voicePosStart = response.find(':', voicePos ) + 1;
        size_t voicePosEnd = response.find(',', voicePos );
        std::string voiceSubstring = response.substr(voicePosStart ,voicePosEnd  - voicePosStart );
        EXPECT_EQ(voiceSubstring ,"\"carol\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'voice' not found in the response.";
    }
}

/**
 * @name  : SetNumberAsVoice
 * @brief : Set voice as Number and check whether it return error
 *
 * @param[in]   :  Set "" in voice
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetNumberAsVoice) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"voice\": 01}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t voicePos = response.find("\"voice\"");

    if (voicePos  != string::npos) {
        size_t voicePosStart = response.find(':', voicePos ) + 1;
        size_t voicePosEnd = response.find(',', voicePos );
        std::string voiceSubstring = response.substr(voicePosStart ,voicePosEnd  - voicePosStart );
        EXPECT_EQ(voiceSubstring ,"\"carol\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'voice' not found in the response.";
    }
}

/**
 * @name  : SetNullVoice
 * @brief : Set voice as NULL and check whether it return error
 *
 * @param[in]   :  Set NULL voice
 * @expected    :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetNullVoice) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setttsconfiguration"), _T("{\"voice\": null}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getttsconfiguration"), _T(""), response));
    size_t voicePos = response.find("\"voice\"");

    if (voicePos  != string::npos) {
        size_t voicePosStart = response.find(':', voicePos ) + 1;
        size_t voicePosEnd = response.find(',', voicePos );
        std::string voiceSubstring = response.substr(voicePosStart ,voicePosEnd  - voicePosStart );
        EXPECT_EQ(voiceSubstring ,"\"carol\"");
     } else {
        EXPECT_TRUE(false) << "Error: 'voice' not found in the response.";
    }
}

/**
 * @name  : SetAuthInfoTypeEmpty
 * @brief : Set AuthInfo type Empty
 *
 * @param[in]   :  Set type as invalid
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetAuthInfoTypeEmpty) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"),
        _T("{\"authinfo\": {\"type\":\"\",\"value\":\"speak text\"}}"), response));
}

/**
 * @name  : SetAuthInfoTypeWhiteSpace
 * @brief : Set AuthInfo type " "
 *
 * @param[in]   :  Set type as " "
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetAuthInfoTypeWhiteSpace) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"),
        _T("{\"authinfo\": {\"type\":\"  \",\"value\":\"speak text\"}}"), response));
}

/**
 * @name  : SetAuthInfoTypeNull
 * @brief : Set AuthInfo type NULL
 *
 * @param[in]   :  Set type as NULL
 * @expected    :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetAuthInfoTypeNull) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setttsconfiguration"),
        _T("{\"authinfo\": {\"type\":null,\"value\":\"speak text\"}}"), response));
}

/*******************************************************************************************************************
 * Test function for SetACL
 * resume          :
 *                Gives speak access for applications
 *
 *                @return Response object contains bool success
 * Use case coverage:
 *                @Success : 1
 *                @Failure : 8
 ********************************************************************************************************************/
 /**
 * @name  : SetACL
 * @brief : Gives speak access for applications
 *
 * @param[in]   :  method of tts and app name
 * @return      :  ERROR_NONE
 */

 TEST_F(TTSInitializedTest,SetACL) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(
        connection,
        _T("setACL"),
        _T("{\"accesslist\": [{\"method\":\"speak\",\"apps\":[\"WebAPP1\"]}]}"),
        response
    ));

    EXPECT_EQ(response, _T("{\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(
        connection,
        _T("setttsconfiguration"),
        _T("{\"language\": \"en-US\",\"voice\": \"carol\","
            "\"ttsendpoint\":\"http://example-tts-dummy.net/tts/v1/cdn/location?\","
            "\"ttsendpointsecured\":\"https://example-tts-dummy.net/tts/v1/cdn/location?\"}"
        ),
        response
    ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("speak"),
        _T("{\"text\": \"speech_123\",\"callsign\":\"WebAPP1\"}"), response));
    sleep(1);
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"speechid\"")));
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"TTS_Status\":0")));
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"success\":true")));
}

 /**
 * @name  : SetACLInvalidAccess
 * @brief : Gives speak access for applications
 *
 * @param[in]   :  method of tts and app name
 * @return      :  ERROR_NONE
 */

TEST_F(TTSInitializedTest, SetACLInvalidAccess) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(
        connection,
        _T("setACL"),
        _T("{\"accesslist\": [{\"method\":\"speak\",\"apps\":\"WebAPP1\"}]}"),
        response
    ));

    EXPECT_EQ(response, _T("{\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(
        connection,
        _T("setttsconfiguration"),
        _T("{\"language\": \"en-US\",\"voice\": \"carol\","
            "\"ttsendpoint\":\"http://example-tts-dummy.net/tts/v1/cdn/location?\","
            "\"ttsendpointsecured\":\"https://example-tts-dummy.net/tts/v1/cdn/location?\"}"
        ),
        response
    ));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(
        connection,
        _T("speak"),
        _T("{\"text\": \"speech_123\",\"callsign\":\"WebAPP2\"}"),
        response
    ));

    EXPECT_EQ(response, _T(""));
}

/**
 * @name  : SetACLInvalidMethod
 * @brief : Calling speak method with  invalid method other than speak
 *
 * @param[in]   :  method of tts and app name
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetACLInvalidMethod) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(
        connection,
        _T("setACL"),
        _T("{\"accesslist\": [{\"method\":\"invalid\",\"apps\":\"WebAPP1\"}]}"),
        response
    ));
}

/**
 * @name  : SetACLEmptyMethod
 * @brief : Calling speak method with empty method
 *
 * @param[in]   :  method of tts and app name
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetACLEmptyMethod) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(
        connection,
        _T("setACL"),
        _T("{\"accesslist\": [{\"method\":\"\",\"apps\":\"WebAPP1\"}]}"),
        response
    ));
}

/**
 * @name  : SetACLWhiteSpaceMethod
 * @brief : Calling speak method with empty method
 *
 * @param[in]   :  method of tts and app name
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetACLWhitespaceMethod) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(
        connection,
        _T("setACL"),
        _T("{\"accesslist\": [{\"method\":\" \",\"apps\":\"WebAPP1\"}]}"),
        response
    ));
}

/**
 * @name  : SetACLNullMethod
 * @brief : Calling speak method with empty method
 *
 * @param[in]   :  method of tts and app name
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetACLNullMethod) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(
        connection,
        _T("setACL"),
        _T("{\"accesslist\": [{\"method\":NULL,\"apps\":\"WebAPP1\"}]}"),
        response
    ));
}

/***
 * @name  : SetACLEmptyApp
 * @brief : Calling speak method with empty method
 *
 * @param[in]   :  method of tts and app name
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetACLEmptyApp) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(
        connection,
        _T("setACL"),
        _T("{\"accesslist\": [{\"method\":\"speak\",\"apps\":\"\"}]}"),
        response
    ));
}

/**
 * @name  : SetACLWhiteSpaceApp
 * @brief : Calling speak method with empty method
 *
 * @param[in]   :  method of tts and app name
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetACLWhitespaceApp) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(
        connection,
        _T("setACL"),
        _T("{\"accesslist\": [{\"method\":\"speak\",\"apps\":\" \"}]}"),
        response
    ));
}

/**
 * @name  : SetACLNullApp
 * @brief : Calling speak method with empty method
 *
 * @param[in]   :  method of tts and app name
 * @return      :  ERROR_GENERAL
 */

TEST_F(TTSInitializedTest, SetACLNullApp) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(
        connection,
        _T("setACL"),
        _T("{\"accesslist\": [{\"method\":\"speak\",\"apps\":NULL}]}"),
        response
    ));
}

TEST_F(TTSInitializedTest,SetConfigurationWithFallbackText) {
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setttsconfiguration"), 
        _T("{\"language\":\"en-US\", \"voice\":\"carol\",\"fallbacktext\":{\"scenario\":\"error\",\"value\":\"TTS service unavailable\"}}"), response));
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"TTS_Status\":0")));
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"success\":true")));
}

TEST_F(TTSInitializedTest,SpeakWithRFCURL) {
    plugin->Deinitialize(&service);
    printf("kykumar ttsplugin stopped\n");
    sleep(5);
    mockTTSConfigureTTS2();
    printf("kykumar starting ttsplugin\n");
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    printf("kykumar enable tts\n");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("enabletts"), _T("{\"enabletts\": false}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("enabletts"), _T("{\"enabletts\": true}"), response));
    sleep(1);
    printf("kykumar speak rfc\n");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("speak"), _T("{\"text\": \"speech_123\"}"), response));
    sleep(3);
    printf("kykumar push data\n");
    g_timeout_add(100, (GSourceFunc)push_data, this->sourceMock); // every 100ms
    sleep(2);
    printf("kykumar push EOS\n");
    g_signal_emit_by_name(this->sourceMock, "end-of-stream", NULL);
    sleep(2);
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"speechid\"")));
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"TTS_Status\":0")));
    EXPECT_THAT(response, ::testing::ContainsRegex(_T("\"success\":true")));
    cleanupTTSConfigFile();
}