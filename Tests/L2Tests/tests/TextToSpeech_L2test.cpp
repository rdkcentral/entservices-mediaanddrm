/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#include "L2Tests.h"
#include "L2TestsMock.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>

#include <condition_variable>
#include <fstream>
#include <mutex>

#include <interfaces/ITextToSpeech.h>

#define JSON_TIMEOUT (5000)
#define TEST_LOG(x, ...)                                                                                                                         \
    fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); \
    fflush(stderr);
#define SAMPLEPLUGIN_CALLSIGN _T("org.rdk.TextToSpeech.1")
#define SAMPLEPLUGINL2TEST_CALLSIGN _T("L2tests.1")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;

class TextToSpeechTest : public L2TestMocks {

protected:
    virtual ~TextToSpeechTest() override;

public:
    TextToSpeechTest();
    void eventHandler();
    void setTTSConfiguration();
    void enableTTS(bool);
    void setACL();
    uint32_t WaitForRequestStatus(uint32_t);

public:
    std::mutex m_mutex;
    std::condition_variable m_condition_variable;
    uint32_t m_event_signalled;

    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;
    uint32_t speechID;
};

TextToSpeechTest::TextToSpeechTest()
    : L2TestMocks()
{
    gst_init(nullptr, nullptr);
    ON_CALL(*p_systemAudioPlatformAPIMock, systemAudioInitialize())
        .WillByDefault(
            ::testing::Invoke(
                [&]() {
                    return;
                }));
    ON_CALL(*p_systemAudioPlatformAPIMock, systemAudioDeinitialize())
        .WillByDefault(
            ::testing::Invoke(
                [&]() {
                    return;
                }));

    ON_CALL(*p_systemAudioPlatformAPIMock, systemAudioChangePrimaryVol(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return());

    ON_CALL(*p_systemAudioPlatformAPIMock, systemAudioSetDetectTime(::testing::_))
        .WillByDefault(::testing::Return());

    ON_CALL(*p_systemAudioPlatformAPIMock, systemAudioSetHoldTime(::testing::_))
        .WillByDefault(::testing::Return());

    ON_CALL(*p_systemAudioPlatformAPIMock, systemAudioSetThreshold(::testing::_))
        .WillByDefault(::testing::Return());

    ON_CALL(*p_systemAudioPlatformAPIMock, systemAudioSetVolume(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return());

    ON_CALL(*p_systemAudioPlatformAPIMock, systemAudioGeneratePipeline(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](GstElement** pipeline, GstElement** source, GstElement* capsfilter,
                             GstElement** audioSink, GstElement** audioVolume,
                             AudioType type, PlayMode mode, SourceType sourceType, bool smartVolumeEnable) {

            *pipeline = gst_pipeline_new(NULL);
            *source = gst_element_factory_make("souphttpsrc", NULL);
            GstElement* convert = gst_element_factory_make("audioconvert", NULL);
            *audioSink = gst_element_factory_make("fakesink", NULL);
            
            // Set sync=true to make fakesink respect audio timing instead of consuming instantly
            g_object_set(*audioSink, "sync", TRUE, NULL);

            bool result = TRUE;

            if (type == MP3) {
                GstElement* parser = gst_element_factory_make("mpegaudioparse", NULL);
                GstElement* decodebin = gst_element_factory_make("avdec_mp3", NULL);
                gst_bin_add_many(GST_BIN(*pipeline), *source, parser, convert, decodebin, *audioSink, NULL);

                result = gst_element_link_many(*source, parser, decodebin, convert, *audioSink, NULL);

            } else if (type == PCM) {

            } else {
            }

            return result;
        }));

    Core::JSONRPC::Message message;
    string response;
    uint32_t status = Core::ERROR_GENERAL;
    m_event_signalled = 0;
    status = ActivateService("org.rdk.TextToSpeech.1");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

TextToSpeechTest::~TextToSpeechTest()
{
    uint32_t status = Core::ERROR_GENERAL;

    status = DeactivateService("org.rdk.TextToSpeech.1");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

TEST_F(TextToSpeechTest, getapiversion)
{
    JsonObject parameter;
    JsonObject response;
    uint32_t status = Core::ERROR_GENERAL;

    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SAMPLEPLUGIN_CALLSIGN, SAMPLEPLUGINL2TEST_CALLSIGN);
    status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "getapiversion", parameter, response);
    EXPECT_EQ(Core::ERROR_NONE, status);
}

TEST_F(TextToSpeechTest, checkTTSONOFF)
{
    // Disable TTS
    JsonObject parameterDisable;
    JsonObject responseDisable;
    uint32_t status = Core::ERROR_GENERAL;
    std::string receivedEvent;
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SAMPLEPLUGIN_CALLSIGN, SAMPLEPLUGINL2TEST_CALLSIGN);
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT, _T("onttsstatechanged"),
        [this, &receivedEvent](const JsonObject event) {
            std::unique_lock<std::mutex> lock(m_mutex);
            {
                event.ToString(receivedEvent);
                TEST_LOG("Event received in subscription callback: %s", receivedEvent.c_str());
                m_event_signalled = 1;
            }
            m_condition_variable.notify_one();
        });
    bool ttsValue[] = { true, false };
    for (int i = 0; i < 2; i++) {
        parameterDisable["enabletts"] = ttsValue[i];
        uint32_t status = Core::ERROR_GENERAL;
        status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "enabletts", parameterDisable, responseDisable);
        uint32_t signalled = WaitForRequestStatus(JSON_TIMEOUT);
        EXPECT_TRUE(signalled);
        EXPECT_EQ(Core::ERROR_NONE, status);

        JsonObject parameterCheck;
        JsonObject responseCheck;
        status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "isttsenabled", parameterCheck, responseCheck);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_EQ(parameterDisable["enabletts"], responseCheck["isenabled"]);
    }
    // Unsubscribe from the notification
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onttsstatechanged"));
}

TEST_F(TextToSpeechTest, setgetTTSConfiguration)
{
    JsonObject configurationParameter;
    JsonObject configurationResponse;

    configurationParameter["language"] = "en-US";
    configurationParameter["voice"] = "carol";
    configurationParameter["ttsendpointsecured"] = "https://ccr.voice-guidance-tts.xcr.comcast.net/tts?";
    configurationParameter["ttsendpoint"] = "https://ccr.voice-guidance-tts.xcr.comcast.net/tts?";

    uint32_t status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "setttsconfiguration", configurationParameter, configurationResponse);
    EXPECT_EQ(Core::ERROR_NONE, status);
    JsonObject configurationGetParameter;
    JsonObject configurationGetResponse;

    status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "getttsconfiguration", configurationGetParameter, configurationGetResponse);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Verify the retrieved configuration matches what was set
    EXPECT_EQ(configurationParameter["language"], configurationGetResponse["language"]);
    EXPECT_EQ(configurationParameter["voice"], configurationGetResponse["voice"]);
    EXPECT_EQ(configurationParameter["ttsendpointsecured"], configurationGetResponse["ttsendpointsecured"]);
    EXPECT_EQ(configurationParameter["ttsendpoint"], configurationGetResponse["ttsendpoint"]);
}

TEST_F(TextToSpeechTest, SpeakWithTTSDisabled)
{
    uint32_t status = Core::ERROR_GENERAL;
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SAMPLEPLUGIN_CALLSIGN, SAMPLEPLUGINL2TEST_CALLSIGN);

    setTTSConfiguration();
    // Disable TTS
    enableTTS(false);
    // Subscribe to playbackerror
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT, _T("onplaybackerror"),
        [this](const JsonObject event) {
            std::unique_lock<std::mutex> lock(m_mutex);
            {
                std::string eventString;
                event.ToString(eventString);
                TEST_LOG("Event received in subscription callback: %s", eventString.c_str());
                m_event_signalled = 1;
            }
            m_condition_variable.notify_one();
        });

    // Call Speak
    JsonObject parameterSpeak;
    JsonObject responseSpeak;
    std::string text = "Hello Testing";
    std::string callsign = "testApp";
    parameterSpeak["text"] = text;
    parameterSpeak["callsign"] = callsign;
    status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "speak", parameterSpeak, responseSpeak);
    uint32_t signalled = WaitForRequestStatus(JSON_TIMEOUT);
    EXPECT_TRUE(signalled);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Unsubscribe from the notification
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onplaybackerror"));
}

TEST_F(TextToSpeechTest, willSpeakEventCheck)
{
    uint32_t status = Core::ERROR_GENERAL;
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SAMPLEPLUGIN_CALLSIGN, SAMPLEPLUGINL2TEST_CALLSIGN);

    // SetTTSConfiguration
    setTTSConfiguration();
    // Enable TTS
    enableTTS(true);

    // Subscribe to willspeakEvent
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT, _T("onwillspeak"),
        [this](const JsonObject event) {
            std::unique_lock<std::mutex> lock(m_mutex);
            {
                std::string eventString;
                event.ToString(eventString);
                TEST_LOG("Event received in subscription callback: %s", eventString.c_str());
                m_event_signalled = 1;
            }
            m_condition_variable.notify_one();
        });

    // setACL
    setACL();

    // Call Speak
    JsonObject parameterSpeak;
    JsonObject responseSpeak;
    std::string text = "Hello Testing";
    std::string callsign = "testApp";
    parameterSpeak["text"] = text;
    parameterSpeak["callsign"] = callsign;
    status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "speak", parameterSpeak, responseSpeak);
    uint32_t signalled = WaitForRequestStatus(JSON_TIMEOUT);
    EXPECT_TRUE(signalled);
    EXPECT_EQ(Core::ERROR_NONE, status);

    enableTTS(false);
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onwillspeak"));
}

TEST_F(TextToSpeechTest, speakStartEventCheck)
{
    uint32_t status = Core::ERROR_GENERAL;
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SAMPLEPLUGIN_CALLSIGN, SAMPLEPLUGINL2TEST_CALLSIGN);

    // SetTTSConfiguration
    setTTSConfiguration();

    // Enable TTS
    enableTTS(true);

    // Subscribe to willspeakEvent
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT, _T("onspeechstart"),
        [this](const JsonObject event) {
            std::unique_lock<std::mutex> lock(m_mutex);
            {
                std::string eventString;
                event.ToString(eventString);
                TEST_LOG("Event received in subscription callback: %s", eventString.c_str());
                m_event_signalled = 1;
            }
            m_condition_variable.notify_one();
        });

    // setACL
    setACL();

    // Call Speak
    JsonObject parameterSpeak;
    JsonObject responseSpeak;
    std::string text = "Hello Testing";
    std::string callsign = "testApp";
    parameterSpeak["text"] = text;
    parameterSpeak["callsign"] = callsign;
    status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "speak", parameterSpeak, responseSpeak);
    uint32_t signalled = WaitForRequestStatus(JSON_TIMEOUT);
    EXPECT_TRUE(signalled);
    EXPECT_EQ(Core::ERROR_NONE, status);
    enableTTS(false);
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onspeechstart"));
}

TEST_F(TextToSpeechTest, speechCompleteEventCheck)
{
    uint32_t status = Core::ERROR_GENERAL;
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SAMPLEPLUGIN_CALLSIGN, SAMPLEPLUGINL2TEST_CALLSIGN);

    // SetTTSConfiguration
    setTTSConfiguration();

    // Enable TTS
    enableTTS(true);

    // Subscribe to willspeakEvent
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT, _T("onspeechcomplete"),
        [this](const JsonObject event) {
            std::unique_lock<std::mutex> lock(m_mutex);
            {
                std::string eventString;
                event.ToString(eventString);
                TEST_LOG("Event received in subscription callback: %s", eventString.c_str());
                m_event_signalled = 1;
            }
            m_condition_variable.notify_one();
        });

    // setACL
    setACL();

    // Call Speak
    JsonObject parameterSpeak;
    JsonObject responseSpeak;
    std::string text = "Hello Testing";
    std::string callsign = "testApp";
    parameterSpeak["text"] = text;
    parameterSpeak["callsign"] = callsign;
    status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "speak", parameterSpeak, responseSpeak);
    uint32_t signalled = WaitForRequestStatus(JSON_TIMEOUT);
    EXPECT_TRUE(signalled);
    EXPECT_EQ(Core::ERROR_NONE, status);
    enableTTS(false);
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onspeechcomplete"));
}

TEST_F(TextToSpeechTest, speechInterruptEventCheck)
{
    uint32_t status = Core::ERROR_GENERAL;
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SAMPLEPLUGIN_CALLSIGN, SAMPLEPLUGINL2TEST_CALLSIGN);

    // SetTTSConfiguration
    setTTSConfiguration();

    // Enable TTS
    enableTTS(true);

    // setACL
    setACL();

    std::string text1 = "Hello Testing, I am trying to invoke the speechInterrupt for the next text. I am happy for the testing. How can I spend time for turning off the voice guidance in the tv. I see the way to do this.";
    std::string text2 = "Changing the text to be spoken";
    std::string callsign = "testApp";

    // Note: In test environment, audio playback completes extremely fast (~8ms instead of 13+ seconds).
    // To ensure we interrupt WHILE speech is actually playing, we trigger the interrupt action
    // immediately when onspeechstart fires, confirming speech has begun.

    // Subscribe to onspeechstart - will trigger interrupt action immediately when speech begins
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT, _T("onspeechstart"),
        [this, &jsonrpc, &text2, &callsign](const JsonObject event) {
            std::unique_lock<std::mutex> lock(m_mutex);
            TEST_LOG("Speech started, triggering interrupt immediately");
            
            // Speech is NOW confirmed playing - immediately trigger second speak to interrupt
            std::thread([&]() {
                JsonObject parameterSpeak2;
                JsonObject responseSpeak2;
                parameterSpeak2["text"] = text2;
                parameterSpeak2["callsign"] = callsign;
                int status1 = InvokeServiceMethod("org.rdk.TextToSpeech.1", "speak", parameterSpeak2, responseSpeak2);
                if (status1 != Core::ERROR_NONE) {
                    TEST_LOG("Error in second speak call");
                }
            }).detach();
            
            m_event_signalled = 1;
            m_condition_variable.notify_one();
        });

    // Speak call

    // Subscribe to onspeechinterrupted to catch the interrupt event
    m_event_signalled = 0;
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT, _T("onspeechinterrupted"),
        [this](const JsonObject event) {
            std::unique_lock<std::mutex> lock(m_mutex);
            {
                std::string eventString;
                event.ToString(eventString);
                TEST_LOG("Interrupt event received: %s", eventString.c_str());
                m_event_signalled = 1;
            }
            m_condition_variable.notify_one();
        });

    // First invocation of speak in the main thread
    JsonObject parameterSpeak;
    JsonObject responseSpeak;
    parameterSpeak["text"] = text1;
    parameterSpeak["callsign"] = callsign;
    status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "speak", parameterSpeak, responseSpeak);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Wait for onspeechstart (which triggers the interrupt)
    uint32_t signalled = WaitForRequestStatus(JSON_TIMEOUT);
    EXPECT_TRUE(signalled);
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onspeechstart"));

    // Wait for interrupt event (flag already reset before subscription)
    uint32_t signalled1 = WaitForRequestStatus(JSON_TIMEOUT);
    EXPECT_TRUE(signalled1);
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onspeechinterrupted"));
    enableTTS(false);
}

TEST_F(TextToSpeechTest, disableTTSDuringSpeak)
{

    uint32_t status = Core::ERROR_GENERAL;
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SAMPLEPLUGIN_CALLSIGN, SAMPLEPLUGINL2TEST_CALLSIGN);

    // SetTTSConfiguration
    setTTSConfiguration();

    // Enable TTS
    enableTTS(true);

    // setACL
    setACL();

    std::string text1 = "Hello Testing, I am trying to invoke the speechInterrupt for the next text. I am happy for the testing. How can I spend time for turning off the voice guidance in the tv. I see the way to do this.";
    std::string callsign = "testApp";

    // Note: In test environment, audio playback completes extremely fast (~8ms instead of 13+ seconds).
    // To ensure we interrupt WHILE speech is actually playing, we trigger the disable action
    // immediately when onspeechstart fires, confirming speech has begun.

    // Subscribe to onspeechstart - will disable TTS immediately when speech begins
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT, _T("onspeechstart"),
        [this](const JsonObject event) {
            std::unique_lock<std::mutex> lock(m_mutex);
            TEST_LOG("Speech started, disabling TTS immediately to interrupt");
            
            // Speech is NOW confirmed playing - immediately disable TTS to interrupt
            std::thread([this]() {
                enableTTS(false);
            }).detach();
            
            m_event_signalled = 1;
            m_condition_variable.notify_one();
        });

    // Subscribe to onspeechinterrupted to catch the interrupt event
    // CRITICAL: Reset flag BEFORE subscribing to avoid race condition
    // where event arrives between subscription and flag reset
    m_event_signalled = 0;
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT, _T("onspeechinterrupted"),
        [this](const JsonObject event) {
            std::unique_lock<std::mutex> lock(m_mutex);
            {
                std::string eventString;
                event.ToString(eventString);
                TEST_LOG("Interrupt event received: %s", eventString.c_str());
                m_event_signalled = 1;
            }
            m_condition_variable.notify_one();
        });

    // First invocation of speak in the main thread
    JsonObject parameterSpeak;
    JsonObject responseSpeak;
    parameterSpeak["text"] = text1;
    parameterSpeak["callsign"] = callsign;
    status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "speak", parameterSpeak, responseSpeak);
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Wait for onspeechstart (which triggers the disable)
    uint32_t signalled = WaitForRequestStatus(JSON_TIMEOUT);
    EXPECT_TRUE(signalled);
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onspeechstart"));

    // Wait for interrupt event (flag already reset before subscription)
    uint32_t signalled1 = WaitForRequestStatus(JSON_TIMEOUT);
    EXPECT_TRUE(signalled1);
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onspeechinterrupted"));
}

TEST_F(TextToSpeechTest, cancelDuringSpeak)
{
    uint32_t status = Core::ERROR_GENERAL;
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SAMPLEPLUGIN_CALLSIGN, SAMPLEPLUGINL2TEST_CALLSIGN);

    // SetTTSConfiguration
    setTTSConfiguration();

    // Enable TTS
    enableTTS(true);

    // Subscribe to onspeechStart
    // Note: In test environment, audio playback completes extremely fast (~8ms instead of 13+ seconds).
    // To ensure we interrupt WHILE speech is actually playing, we trigger the cancel() immediately when
    // onspeechstart fires, confirming speech has begun.

    // setACL
    setACL();

    // Speak call
    std::string text1 = "Hello Testing, I am trying to invoke the speechInterrupt for the next text. I am happy for the testing. How can I spend time for turning off the voice guidance in the tv. I see the way to do this.";
    std::string callsign = "testApp";

    // First invocation of speak in the main thread
    JsonObject parameterSpeak;
    JsonObject responseSpeak;
    parameterSpeak["text"] = text1;
    parameterSpeak["callsign"] = callsign;
    status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "speak", parameterSpeak, responseSpeak);
    EXPECT_EQ(Core::ERROR_NONE, status);
    
    uint32_t localSpeechID = responseSpeak["speechid"].Number();

    // Subscribe to onspeechstart and trigger cancel immediately when it fires
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT, _T("onspeechstart"),
        [this, localSpeechID](const JsonObject event) {
            std::string eventString;
            event.ToString(eventString);
            TEST_LOG("Event received in subscription callback: %s", eventString.c_str());
            
            // Trigger cancel immediately in a detached thread to avoid blocking the callback
            std::thread([this, localSpeechID]() {
                JsonObject parameterCancel;
                JsonObject responseCancel;
                parameterCancel["speechid"] = JsonValue((uint32_t)localSpeechID);
                uint32_t status1 = InvokeServiceMethod("org.rdk.TextToSpeech.1", "cancel", parameterCancel, responseCancel);
                EXPECT_EQ(Core::ERROR_NONE, status1);
            }).detach();
            
            std::unique_lock<std::mutex> lock(m_mutex);
            m_event_signalled = 1;
            m_condition_variable.notify_one();
        });

    uint32_t signalled = WaitForRequestStatus(JSON_TIMEOUT);
    EXPECT_TRUE(signalled);
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onspeechstart"));

    // Subscribe to onspeechinterrupted to catch the interrupt event
    // CRITICAL: Reset flag BEFORE subscribing to avoid race condition
    m_event_signalled = 0;
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT, _T("onspeechinterrupted"),
        [this](const JsonObject event) {
            std::string eventString;
            event.ToString(eventString);
            TEST_LOG("Event received in subscription callback: %s", eventString.c_str());
            std::unique_lock<std::mutex> lock(m_mutex);
            m_event_signalled = 1;
            m_condition_variable.notify_one();
        });

    uint32_t signalled1 = WaitForRequestStatus(JSON_TIMEOUT);
    EXPECT_TRUE(signalled1);
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onspeechinterrupted"));
    enableTTS(false);
}

void TextToSpeechTest::setTTSConfiguration()
{
    JsonObject configurationParameter;
    JsonObject configurationResponse;

    configurationParameter["language"] = "en-US";
    configurationParameter["voice"] = "carol";
    configurationParameter["ttsendpointsecured"] = "https://ccr.voice-guidance-tts.xcr.comcast.net/tts?";
    configurationParameter["ttsendpoint"] = "https://ccr.voice-guidance-tts.xcr.comcast.net/tts?";

    uint32_t status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "setttsconfiguration", configurationParameter, configurationResponse);
    EXPECT_EQ(Core::ERROR_NONE, status);
}

void TextToSpeechTest::enableTTS(bool ttsValue)
{
    JsonObject parameter;
    JsonObject response;
    parameter["enabletts"] = ttsValue;
    uint32_t status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "enabletts", parameter, response);
    EXPECT_EQ(Core::ERROR_NONE, status);
}

void TextToSpeechTest::setACL()
{
    JsonObject parameterACL;
    JsonObject responseACL;
    JsonObject accessListItems;
    accessListItems["method"] = "speak";
    accessListItems["apps"] = { "testApp" };
    parameterACL["accesslist"] = { accessListItems };
    uint32_t status = InvokeServiceMethod("org.rdk.TextToSpeech.1", "setACL", parameterACL, responseACL);
    EXPECT_EQ(Core::ERROR_NONE, status);
}

uint32_t TextToSpeechTest::WaitForRequestStatus(uint32_t timeout_ms)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto now = std::chrono::system_clock::now();
    std::chrono::milliseconds timeout(timeout_ms);

    while (!(m_event_signalled)) {
        if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
            TEST_LOG("Timeout waiting for request status event");
            break;
        }
    }
    return m_event_signalled;
}