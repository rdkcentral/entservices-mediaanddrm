# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-03-11
### Added
- Initial implementation of GStreamerPlayer plugin
- Support for audio and video playback using GStreamer
- CreatePipeline API to create GStreamer pipeline with URI
- Play API to start media playback
- Pause API to pause media playback
- OnPipelineCreated event notification
- OnPlaybackStateChanged event notification
- Support for uridecodebin for dynamic source handling
- Automatic audio and video pad linking
- Bus message handling for errors, EOS, and state changes

### Features
- Audio playback with audioconvert, audioresample, and autoaudiosink elements
- Video playback with videoconvert and autovideosink elements
- Dynamic pad handling for audio and video streams
- State management (Playing, Paused, Error, EOS)
- Event notifications to plugin clients
- Thread-safe implementation with proper locking

[Unreleased]: https://github.com/rdkcentral/entservices-mediaanddrm/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/rdkcentral/entservices-mediaanddrm/releases/tag/v1.0.0
