# GStreamerPlayer Plugin

## Overview

The GStreamerPlayer plugin is a Thunder/WPEFramework plugin that provides media playback capabilities using the GStreamer multimedia framework. It enables applications to play audio and video content through a simple JSON-RPC interface.

## Features

- **Media Playback**: Play audio and video content from various sources (HTTP, HTTPS, WebSocket, file)
- **Dynamic Pipeline Creation**: Create GStreamer pipelines dynamically based on URI
- **State Management**: Control playback state (play, pause)
- **Event Notifications**: Receive notifications for pipeline creation and playback state changes
- **Auto-detection**: Automatically detects and handles audio and video streams

## Architecture

The plugin consists of the following main components:

1. **GStreamerPlayer**: Main plugin class that implements Thunder plugin interfaces
2. **GStreamerPlayerImplementation**: Core implementation containing GStreamer pipeline logic
3. **GStreamerPlayerJsonRpc**: JSON-RPC method handlers for external API access

## APIs

### createPipeline

Creates a GStreamer pipeline with the specified URI.

**Parameters:**
- `uri` (string): The media URI to play

**Response:**
```json
{
  "success": true,
  "message": "Pipeline created successfully"
}
```

**Example:**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "org.rdk.GStreamerPlayer.1.createPipeline",
  "params": {
    "uri": "https://example.com/media/video.mp4"
  }
}
```

### play

Starts media playback.

**Parameters:**
- None required (uses the current pipeline)

**Response:**
```json
{
  "success": true,
  "message": "Playback started"
}
```

**Example:**
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "org.rdk.GStreamerPlayer.1.play",
  "params": {}
}
```

### pause

Pauses media playback.

**Parameters:**
- None required

**Response:**
```json
{
  "success": true,
  "message": "Playback paused"
}
```

**Example:**
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "org.rdk.GStreamerPlayer.1.pause",
  "params": {}
}
```

### getapiversion

Returns the API version number.

**Parameters:**
- None

**Response:**
```json
{
  "version": 1
}
```

## Events

### onPipelineCreated

Fired when a GStreamer pipeline is successfully created.

**Data:**
```json
{
  "status": "created"
}
```

### onPlaybackStateChanged

Fired when the playback state changes.

**Data:**
```json
{
  "state": "playing" | "paused" | "eos" | "error",
  "message": "Optional error message"
}
```

## GStreamer Pipeline Structure

The plugin creates a pipeline with the following structure:

```
uridecodebin --> audioconvert --> audioresample --> autoaudiosink (audio branch)
              |
              --> videoconvert --> autovideosink (video branch)
```

- **uridecodebin**: Automatically detects and decodes the media source
- **audioconvert**: Converts audio format
- **audioresample**: Resamples audio to appropriate rate
- **autoaudiosink**: Automatically selects appropriate audio sink
- **videoconvert**: Converts video format
- **autovideosink**: Automatically selects appropriate video sink

## Configuration

The plugin can be configured through CMake variables:

- `PLUGIN_GSTREAMERPLAYER_AUTOSTART`: Auto-start the plugin (default: "true")
- `PLUGIN_GSTREAMERPLAYER_MODE`: Process mode - "Local", "Off", or "Out-Of-Process" (default: "Local")
- `PLUGIN_GSTREAMERPLAYER_STARTUPORDER`: Plugin startup order (default: "50")

## Building

The plugin requires the following dependencies:

- Thunder/WPEFramework
- GStreamer 1.0
- GLib 2.0

Build with CMake:

```bash
mkdir build
cd build
cmake ..
make
```

## Usage Example

1. Create a pipeline with a media URI:
```bash
curl -X POST http://localhost:9998/jsonrpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "org.rdk.GStreamerPlayer.1.createPipeline",
    "params": {
      "uri": "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm"
    }
  }'
```

2. Start playback:
```bash
curl -X POST http://localhost:9998/jsonrpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 2,
    "method": "org.rdk.GStreamerPlayer.1.play",
    "params": {}
  }'
```

3. Pause playback:
```bash
curl -X POST http://localhost:9998/jsonrpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 3,
    "method": "org.rdk.GStreamerPlayer.1.pause",
    "params": {}
  }'
```

## License

Licensed under the Apache License, Version 2.0. See LICENSE file for details.

## Contributing

Please see CONTRIBUTING.md for guidelines on contributing to this project.
