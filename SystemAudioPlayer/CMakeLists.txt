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

set(PLUGIN_NAME SystemAudioPlayer)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_SYSTEMAUDIOPLAYER_AUTOSTART "true" CACHE STRING "Automatically start SystemAudioPlayer plugin")
set(PLUGIN_SYSTEMAUDIOPLAYER_MODE "Local" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")
set(PLUGIN_SYSTEMAUDIOPLAYER_STARTUPORDER "40" CACHE STRING "To configure startup order of SystemAudioPlayer plugin")

find_package(${NAMESPACE}Plugins REQUIRED)
if (USE_THUNDER_R4)
    find_package(${NAMESPACE}COM REQUIRED)
    find_package(${NAMESPACE}WebSocket REQUIRED)
endif (USE_THUNDER_R4)

find_package(Boost COMPONENTS system filesystem REQUIRED)
find_package(websocketpp REQUIRED)
find_package(OpenSSL REQUIRED)

set_source_files_properties(
    SystemAudioPlayerImplementation.cpp
    PROPERTIES COMPILE_FLAGS "-fexceptions")

add_library(${MODULE_NAME} SHARED
        Module.cpp
        SystemAudioPlayer.cpp
        SystemAudioPlayerJsonRpc.cpp
        SystemAudioPlayerImplementation.cpp
        impl/AudioPlayer.cpp
        impl/BufferQueue.cpp
        impl/SecuredWebSocketClient.cpp
        impl/UnsecuredWebSocketClient.cpp
        impl/logger.cpp
        ../helpers/WebSockets/WSEndpoint.cpp
        ../helpers/WebSockets/JsonRpc/Request.cpp
        ../helpers/WebSockets/JsonRpc/Response.cpp
        ../helpers/WebSockets/JsonRpc/Notification.cpp
        )

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

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

list(APPEND CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-app-1.0)

find_package(GStreamer REQUIRED)

find_package(Curl)


target_include_directories(${MODULE_NAME} PRIVATE ./)
target_include_directories(${MODULE_NAME} PRIVATE ../helpers)
target_include_directories(${MODULE_NAME} PRIVATE ../helpers/WebSockets)
target_include_directories(${MODULE_NAME} PRIVATE ${GSTREAMER_INCLUDES})
target_include_directories(${MODULE_NAME} PRIVATE ${GSTREAMERBASE_INCLUDE_DIRS})
if(RDK_SERVICES_L1_TEST)
    target_include_directories(${MODULE_NAME} PRIVATE ../Tests/mocks)
endif()

target_link_libraries(${MODULE_NAME} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins)
target_link_libraries(${MODULE_NAME} PRIVATE ${CURL_LIBRARY})
target_link_libraries(${MODULE_NAME} PRIVATE ${GSTREAMER_LIBRARIES})
target_link_libraries(${MODULE_NAME} PRIVATE ${GSTREAMERBASE_LIBRARIES})

if(NOT RDK_SERVICES_L1_TEST)
    target_link_libraries(${MODULE_NAME} PRIVATE systemaudioplatform)
endif()

target_link_libraries(${MODULE_NAME} PRIVATE trower-base64)
target_link_libraries(${MODULE_NAME} PRIVATE ${Boost_SYSTEM_LIBRARY})
target_link_libraries(${MODULE_NAME} PRIVATE OpenSSL::SSL)
target_link_libraries(${MODULE_NAME} PRIVATE OpenSSL::Crypto)
target_link_libraries(${MODULE_NAME} PRIVATE ${Boost_FILESYSTEM_LIBRARY})

if (USE_THUNDER_R4)
target_link_libraries(${MODULE_NAME} PRIVATE ${NAMESPACE}COM::${NAMESPACE}COM)
target_link_libraries(${MODULE_NAME} PRIVATE ${NAMESPACE}WebSocket::${NAMESPACE}WebSocket)
endif (USE_THUNDER_R4)

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config( SystemAudioPlayer )
