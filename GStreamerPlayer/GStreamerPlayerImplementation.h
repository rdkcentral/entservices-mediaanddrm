/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026
 *
 * Licensed under the Apache License, Version 2.0
 */

#pragma once

#include "Module.h"
#include <interfaces/Ids.h>
#include "tracing/Logging.h"

#include <interfaces/IGStreamerPlayer.h>

#include <gst/gst.h>
#include <list>

namespace WPEFramework {
namespace Plugin {

class GStreamerPlayerImplementation : public Exchange::IGStreamerPlayer {
public:

    enum Event {
        ONPIPELINECREATED,
        ONPLAYSTATECHANGED
    };

    class EXTERNAL Job : public Core::IDispatch {
    protected:
        Job(GStreamerPlayerImplementation* parent, Event event, string data)
            : _parent(parent)
            , _event(event)
            , _data(data)
        {
            if (_parent != nullptr) {
                _parent->AddRef();
            }
        }

    public:
        Job() = delete;
        Job(const Job&) = delete;
        Job& operator=(const Job&) = delete;

        ~Job() {
            if (_parent != nullptr) {
                _parent->Release();
            }
        }

    public:
        static Core::ProxyType<Core::IDispatch> Create(
            GStreamerPlayerImplementation* parent,
            Event event,
            string data)
        {
            return Core::ProxyType<Core::IDispatch>(
                Core::ProxyType<Job>::Create(parent, event, data));
        }

        virtual void Dispatch() override
        {
            _parent->Dispatch(_event, _data);
        }

    private:
        GStreamerPlayerImplementation* _parent;
        const Event _event;
        const string _data;
    };

public:

    GStreamerPlayerImplementation(const GStreamerPlayerImplementation&) = delete;
    GStreamerPlayerImplementation& operator=(const GStreamerPlayerImplementation&) = delete;

    virtual Core::hresult Configure(PluginHost::IShell* service) override;

    virtual Core::hresult Register(INotification* sink) override;
    virtual Core::hresult Unregister(INotification* sink) override;

    virtual Core::hresult CreatePipeline(const string& input, string& output) override;
    virtual Core::hresult Play(const string& input, string& output) override;
    virtual Core::hresult Pause(const string& input, string& output) override;

    BEGIN_INTERFACE_MAP(GStreamerPlayerImplementation)
        INTERFACE_ENTRY(Exchange::IGStreamerPlayer)
    END_INTERFACE_MAP

private:

    void Dispatch(Event event, string data);

private:

    mutable Core::CriticalSection _adminLock;
    std::list<Exchange::IGStreamerPlayer::INotification*> _notificationClients;

    GstElement* _pipeline;

public:

    GStreamerPlayerImplementation();
    virtual ~GStreamerPlayerImplementation();

    friend class Job;
};

} // Plugin
} // WPEFramework
