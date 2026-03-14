###
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2026 RDK Management
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

set(PLUGIN_NAME GStreamerPlayer)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_GSTREAMERPLAYER_AUTOSTART "true" CACHE STRING "Automatically start GStreamerPlayer plugin")
set(PLUGIN_GSTREAMERPLAYER_MODE "Local" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")
set(PLUGIN_GSTREAMERPLAYER_STARTUPORDER "50" CACHE STRING "To configure startup order of GStreamerPlayer plugin")

find_package(${NAMESPACE}Plugins REQUIRED)
if (USE_THUNDER_R4)
    find_package(${NAMESPACE}COM REQUIRED)
endif (USE_THUNDER_R4)

set_source_files_properties(
    GStreamerPlayerImplementation.cpp
    PROPERTIES COMPILE_FLAGS "-fexceptions")

add_library(${MODULE_NAME} SHARED
        Module.cpp
        GStreamerPlayer.cpp
        GStreamerPlayerJsonRpc.cpp
        GStreamerPlayerImplementation.cpp
        )

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

list(APPEND CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-base-1.0)

target_include_directories(${MODULE_NAME} PRIVATE ./)
target_include_directories(${MODULE_NAME} PRIVATE ../helpers)
target_include_directories(${MODULE_NAME} PRIVATE ${GLIB_INCLUDE_DIRS})
target_include_directories(${MODULE_NAME} PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_include_directories(${MODULE_NAME} PRIVATE ${GSTREAMERBASE_INCLUDE_DIRS})

find_package(${NAMESPACE}Definitions REQUIRED)

target_link_libraries(${MODULE_NAME} PRIVATE
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    ${NAMESPACE}Definitions::${NAMESPACE}Definitions
    ${GLIB_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMERBASE_LIBRARIES}
)

if (USE_THUNDER_R4)
    target_link_libraries(${MODULE_NAME} PRIVATE ${NAMESPACE}COM::${NAMESPACE}COM)
endif (USE_THUNDER_R4)

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config( GStreamerPlayer )
