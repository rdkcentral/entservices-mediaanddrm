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

#include "GStreamerPlayerImplementation.h"
#include <glib.h>

#define GSTPLAYER_MAJOR_VERSION 1
#define GSTPLAYER_MINOR_VERSION 0

#define CONVERT_PARAMETERS_TOJSON() JsonObject parameters, response; parameters.FromString(input);
#define CONVERT_PARAMETERS_FROMJSON() response.ToString(output);

#undef returnResponse
#define returnResponse(success) \
    response["success"] = success; \
    CONVERT_PARAMETERS_FROMJSON(); \
    return (Core::ERROR_NONE);

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(GStreamerPlayerImplementation, GSTPLAYER_MAJOR_VERSION, GSTPLAYER_MINOR_VERSION);

    // Structure to contain all pipeline information
    typedef struct _CustomData {
        GstElement *pipeline;
        GstElement *source;

        /* Audio elements */
        GstElement *audioconvert;
        GstElement *audioresample;
        GstElement *audiosink;

        /* Video elements */
        GstElement *videoconvert;
        GstElement *videosink;

        GStreamerPlayerImplementation* player;

    } CustomData;

    // Forward declarations
    static void pad_added_handler(GstElement *src, GstPad *new_pad, CustomData *data);
    static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer data);

    GStreamerPlayerImplementation::GStreamerPlayerImplementation() 
        : _adminLock()
        , _pipeline(nullptr)
    {
        // Initialize GStreamer
        gst_init(nullptr, nullptr);
        TRACE_L1("GStreamerPlayerImplementation Constructor");
    }

    GStreamerPlayerImplementation::~GStreamerPlayerImplementation()
    {
        if (_pipeline != nullptr) {
            gst_element_set_state(_pipeline, GST_STATE_NULL);
            gst_object_unref(_pipeline);
            _pipeline = nullptr;
        }
        TRACE_L1("GStreamerPlayerImplementation Destructor");
    }

    Core::hresult GStreamerPlayerImplementation::Configure(PluginHost::IShell* service)
    {
        return Core::ERROR_NONE;
    }

    Core::hresult GStreamerPlayerImplementation::Register(INotification* sink)
    {
        _adminLock.Lock();

        // Make sure a sink is not registered multiple times.
        ASSERT(std::find(_notificationClients.begin(), _notificationClients.end(), sink) == _notificationClients.end());

        _notificationClients.push_back(sink);
        sink->AddRef();

        _adminLock.Unlock();

        TRACE_L1("Registered a notification sink %p", sink);
        return Core::ERROR_NONE;
    }

    Core::hresult GStreamerPlayerImplementation::Unregister(INotification* sink)
    {
        _adminLock.Lock();

        std::list<Exchange::IGStreamerPlayer::INotification*>::iterator index(
            std::find(_notificationClients.begin(), _notificationClients.end(), sink));

        // Make sure you do not unregister something you did not register !!!
        ASSERT(index != _notificationClients.end());

        if (index != _notificationClients.end()) {
            (*index)->Release();
            _notificationClients.erase(index);
            TRACE_L1("Unregistered a notification sink %p", sink);
        }

        _adminLock.Unlock();
        return Core::ERROR_NONE;
    }

    Core::hresult GStreamerPlayerImplementation::CreatePipeline(const string &input, string &output)
    {
        TRACE_L1("GStreamerPlayerImplementation CreatePipeline: %s", input.c_str());
        CONVERT_PARAMETERS_TOJSON();

        if (!parameters.HasLabel("uri")) {
            TRACE_L1("Parameter 'uri' is not found");
            returnResponse(false);
        }

        string uri = parameters["uri"].String();
        if (uri.empty()) {
            TRACE_L1("URI is empty");
            returnResponse(false);
        }

        _adminLock.Lock();

        // Clean up existing pipeline if any
        if (_pipeline != nullptr) {
            gst_element_set_state(_pipeline, GST_STATE_NULL);
            gst_object_unref(_pipeline);
            _pipeline = nullptr;
        }

        // Create CustomData structure
        CustomData *data = new CustomData();
        data->player = this;

        /* Create elements */
        data->source = gst_element_factory_make("uridecodebin", "source");

        /* Audio branch */
        data->audioconvert  = gst_element_factory_make("audioconvert", "audioconvert");
        data->audioresample = gst_element_factory_make("audioresample", "audioresample");
        data->audiosink     = gst_element_factory_make("autoaudiosink", "audiosink");

        /* Video branch */
        data->videoconvert  = gst_element_factory_make("videoconvert", "videoconvert");
        data->videosink     = gst_element_factory_make("autovideosink", "videosink");

        /* Create pipeline */
        data->pipeline = gst_pipeline_new("gstreamer-player-pipeline");

        if (!data->pipeline || !data->source ||
            !data->audioconvert || !data->audioresample || !data->audiosink ||
            !data->videoconvert || !data->videosink) {
            TRACE_L1("Not all elements could be created");
            delete data;
            _adminLock.Unlock();
            returnResponse(false);
        }

        /* Build the pipeline */
        gst_bin_add_many(GST_BIN(data->pipeline),
                        data->source,
                        data->audioconvert, data->audioresample, data->audiosink,
                        data->videoconvert, data->videosink,
                        NULL);

        /* Link audio elements */
        if (!gst_element_link_many(data->audioconvert,
                                   data->audioresample,
                                   data->audiosink,
                                   NULL)) {
            TRACE_L1("Audio elements could not be linked");
            gst_object_unref(data->pipeline);
            delete data;
            _adminLock.Unlock();
            returnResponse(false);
        }

        /* Link video elements */
        if (!gst_element_link_many(data->videoconvert,
                                   data->videosink,
                                   NULL)) {
            TRACE_L1("Video elements could not be linked");
            gst_object_unref(data->pipeline);
            delete data;
            _adminLock.Unlock();
            returnResponse(false);
        }

        /* Set URI */
        g_object_set(data->source, "uri", uri.c_str(), NULL);

        /* Connect to pad-added signal */
        g_signal_connect(data->source, "pad-added", G_CALLBACK(pad_added_handler), data);

        /* Add bus watch */
        GstBus *bus = gst_element_get_bus(data->pipeline);
        gst_bus_add_watch(bus, bus_callback, data);
        gst_object_unref(bus);

        // Store pipeline
        _pipeline = data->pipeline;

        _adminLock.Unlock();

        // Dispatch notification event
        Core::IWorkerPool::Instance().Submit(Job::Create(this, Event::ONPIPELINECREATED, string("{\"status\":\"created\"}")));

        response["message"] = "Pipeline created successfully";
        returnResponse(true);
    }

    Core::hresult GStreamerPlayerImplementation::Play(const string &input, string &output)
    {
        TRACE_L1("GStreamerPlayerImplementation Play: %s", input.c_str());
        CONVERT_PARAMETERS_TOJSON();

        _adminLock.Lock();

        if (_pipeline == nullptr) {
            _adminLock.Unlock();
            TRACE_L1("Pipeline not created");
            response["message"] = "Pipeline not created. Call createPipeline first.";
            returnResponse(false);
        }

        /* Start playing */
        GstStateChangeReturn ret = gst_element_set_state(_pipeline, GST_STATE_PLAYING);
        
        _adminLock.Unlock();

        if (ret == GST_STATE_CHANGE_FAILURE) {
            TRACE_L1("Unable to set the pipeline to the playing state");
            response["message"] = "Failed to start playback";
            returnResponse(false);
        }

        // Dispatch notification event
        Core::IWorkerPool::Instance().Submit(Job::Create(this, Event::ONPLAYSTATECHANGED, string("{\"state\":\"playing\"}")));

        response["message"] = "Playback started";
        returnResponse(true);
    }

    Core::hresult GStreamerPlayerImplementation::Pause(const string &input, string &output)
    {
        TRACE_L1("GStreamerPlayerImplementation Pause: %s", input.c_str());
        CONVERT_PARAMETERS_TOJSON();

        _adminLock.Lock();

        if (_pipeline == nullptr) {
            _adminLock.Unlock();
            TRACE_L1("Pipeline not created");
            response["message"] = "Pipeline not created";
            returnResponse(false);
        }

        /* Pause pipeline */
        GstStateChangeReturn ret = gst_element_set_state(_pipeline, GST_STATE_PAUSED);
        
        _adminLock.Unlock();

        if (ret == GST_STATE_CHANGE_FAILURE) {
            TRACE_L1("Unable to set the pipeline to the paused state");
            response["message"] = "Failed to pause playback";
            returnResponse(false);
        }

        // Dispatch notification event
        Core::IWorkerPool::Instance().Submit(Job::Create(this, Event::ONPLAYSTATECHANGED, string("{\"state\":\"paused\"}")));

        response["message"] = "Playback paused";
        returnResponse(true);
    }

    void GStreamerPlayerImplementation::Dispatch(Event event, string data)
    {
        _adminLock.Lock();
        std::list<Exchange::IGStreamerPlayer::INotification*>::iterator index(_notificationClients.begin());

        while (index != _notificationClients.end()) {
            switch (event) {
                case Event::ONPIPELINECREATED:
                    (*index)->OnPipelineCreated(data);
                    break;
                case Event::ONPLAYSTATECHANGED:
                    (*index)->OnPlaybackStateChanged(data);
                    break;
                default:
                    break;
            }
            index++;
        }
        _adminLock.Unlock();
    }

    /* Pad-added handler */
    static void pad_added_handler(GstElement *src, GstPad *new_pad, CustomData *data)
    {
        GstPad *sink_pad = NULL;
        GstCaps *new_pad_caps = NULL;
        GstStructure *new_pad_struct = NULL;
        const gchar *new_pad_type = NULL;
        GstPadLinkReturn ret;

        TRACE_L1("Received new pad '%s' from '%s'", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

        new_pad_caps = gst_pad_get_current_caps(new_pad);
        new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
        new_pad_type = gst_structure_get_name(new_pad_struct);

        /* Handle Audio */
        if (g_str_has_prefix(new_pad_type, "audio/x-raw")) {
            sink_pad = gst_element_get_static_pad(data->audioconvert, "sink");

            if (!gst_pad_is_linked(sink_pad)) {
                ret = gst_pad_link(new_pad, sink_pad);
                if (GST_PAD_LINK_FAILED(ret))
                    TRACE_L1("Audio link failed");
                else
                    TRACE_L1("Audio link succeeded");
            }
        }
        /* Handle Video */
        else if (g_str_has_prefix(new_pad_type, "video/x-raw")) {
            sink_pad = gst_element_get_static_pad(data->videoconvert, "sink");

            if (!gst_pad_is_linked(sink_pad)) {
                ret = gst_pad_link(new_pad, sink_pad);
                if (GST_PAD_LINK_FAILED(ret))
                    TRACE_L1("Video link failed");
                else
                    TRACE_L1("Video link succeeded");
            }
        }
        else {
            TRACE_L1("Unknown pad type '%s'. Ignoring.", new_pad_type);
        }

        if (new_pad_caps != NULL)
            gst_caps_unref(new_pad_caps);

        if (sink_pad != NULL)
            gst_object_unref(sink_pad);
    }

    /* Bus callback */
    static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer user_data)
    {
        CustomData *data = static_cast<CustomData*>(user_data);

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError *err;
                gchar *debug_info;
                gst_message_parse_error(msg, &err, &debug_info);
                TRACE_L1("Error received from element %s: %s", 
                         GST_OBJECT_NAME(msg->src), err->message);
                TRACE_L1("Debugging information: %s", debug_info ? debug_info : "none");
                
                // Dispatch error notification
                string errorData = string("{\"state\":\"error\",\"message\":\"") + err->message + "\"}";
                Core::IWorkerPool::Instance().Submit(
                    GStreamerPlayerImplementation::Job::Create(
                        data->player, 
                        GStreamerPlayerImplementation::Event::ONPLAYSTATECHANGED, 
                        errorData));

                g_clear_error(&err);
                g_free(debug_info);
                break;
            }

            case GST_MESSAGE_EOS:
                TRACE_L1("End-Of-Stream reached");
                // Dispatch EOS notification
                Core::IWorkerPool::Instance().Submit(
                    GStreamerPlayerImplementation::Job::Create(
                        data->player, 
                        GStreamerPlayerImplementation::Event::ONPLAYSTATECHANGED, 
                        string("{\"state\":\"eos\"}")));
                break;

            case GST_MESSAGE_STATE_CHANGED:
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->pipeline)) {
                    GstState old_state, new_state, pending_state;
                    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                    TRACE_L1("Pipeline state changed from %s to %s",
                             gst_element_state_get_name(old_state),
                             gst_element_state_get_name(new_state));
                }
                break;

            default:
                break;
        }

        return TRUE;
    }

} // namespace Plugin
} // namespace WPEFramework
