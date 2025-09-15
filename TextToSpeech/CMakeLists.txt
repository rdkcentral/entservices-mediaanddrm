###
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2024 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###

set(PLUGIN_NAME TextToSpeech)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_TEXTTOSPEECH_AUTOSTART "true" CACHE STRING "Automatically start TestToSpeech plugin")
set(PLUGIN_TEXTTOSPEECH_STARTUPORDER "" CACHE STRING "Automatically start TextToSpeech plugin")
set(PLUGIN_TEXTTOSPEECH_MODE "Local" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")

find_package(${NAMESPACE}Plugins REQUIRED)

find_package(WPEFrameworkSecurityUtil)
if(NOT WPEFRAMEWORK_SECURITYUTIL_FOUND)
    add_definitions(-DSECURITY_TOKEN_ENABLED=0)
endif()

#TTS_TEXT_LOG is used to disable unwanted logs in prod build
if(TTS_BUILD_TYPE STREQUAL "Release")
    message(STATUS "_______________ TTS Release MODE _______________")
    add_definitions(-DTTS_TEXT_LOG=0)
elseif(TTS_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "_______________ TTS Debug MODE _______________")
    add_definitions(-DTTS_TEXT_LOG=1)
else()
    message(WARNING "TTS_BUILD_TYPE not set! Defaulting to Debug mode.")
    add_definitions(-DTTS_TEXT_LOG=1)
endif()

set_source_files_properties(
        TextToSpeechImplementation.cpp
        PROPERTIES COMPILE_FLAGS "-fexceptions")

set_source_files_properties(
        TextToSpeechJsonRpc.cpp
        PROPERTIES COMPILE_FLAGS "-fexceptions")

add_library(${MODULE_NAME} SHARED
        Module.cpp
        TextToSpeech.cpp
        TextToSpeechJsonRpc.cpp
        TextToSpeechImplementation.cpp
        impl/TTSManager.cpp
        impl/TTSSpeaker.cpp
        impl/logger.cpp
        impl/TTSDownloader.cpp
        impl/TTSURLConstructer.cpp
        impl/NetworkStatusObserver.cpp
        impl/SatToken.cpp
        impl/RFCURLObserver.cpp
        )
set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

list(APPEND CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(GStreamer REQUIRED)

find_package(Curl)

find_package(${NAMESPACE}Plugins REQUIRED)

find_package(${NAMESPACE}Definitions REQUIRED)

if (RDK_SERVICE_L2_TEST)
    message ("L2 test Enabled")
    find_library(TESTMOCKLIB_LIBRARIES NAMES TestMocklib)
    if (TESTMOCKLIB_LIBRARIES)
        message ("linking mock libraries ${TESTMOCKLIB_LIBRARIES} library")
        target_link_libraries(${MODULE_NAME} PRIVATE ${TESTMOCKLIB_LIBRARIES})
    else (TESTMOCKLIB_LIBRARIES)
        message ("Require ${TESTMOCKLIB_LIBRARIES} library")
    endif (TESTMOCKLIB_LIBRARIES)
endif()
target_include_directories(${MODULE_NAME} PRIVATE ../helpers ${GSTREAMER_INCLUDES})

if(RDK_SERVICES_L1_TEST OR RDK_SERVICE_L2_TEST)
    target_include_directories(${MODULE_NAME} PRIVATE
        ../Tests/mocks
    )
    target_link_libraries(${MODULE_NAME} PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
        ${CURL_LIBRARY} ${GSTREAMER_LIBRARIES} ${WPEFRAMEWORK_SECURITYUTIL_LIBRARIES}
)
else()
    target_link_libraries(${MODULE_NAME} PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
        ${CURL_LIBRARY} ${GSTREAMER_LIBRARIES} ${WPEFRAMEWORK_SECURITYUTIL_LIBRARIES}
        systemaudioplatform
    )
endif()


install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config( TextToSpeech )
