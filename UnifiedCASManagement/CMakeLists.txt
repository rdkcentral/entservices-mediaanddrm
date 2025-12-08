# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
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

set(PLUGIN_NAME UnifiedCASManagement)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_IMPLEMENTATION ${MODULE_NAME}Implementation)

set(PLUGIN_UNIFIEDCASMANAGEMENT_STARTUPORDER "" CACHE STRING "To configure startup order of UnifiedCASManagement plugin")

find_package(${NAMESPACE}Plugins REQUIRED)

list(APPEND CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(IARMBus)

if (NOT RDK_SERVICES_L1_TEST AND NOT RDK_SERVICE_L2_TEST)
find_package(LMPLAYER)  # Optional dependency
else()
include_directories(
        ${PROJECT_SOURCE_DIR}/../entservices-testframework/Tests/mocks/
        ${PROJECT_SOURCE_DIR}/helpers
)
endif()

# Main plugin library (JSON-RPC interface)
if (LMPLAYER_FOUND AND NOT RDK_SERVICES_L1_TEST AND NOT RDK_SERVICE_L2_TEST)
    add_library(${MODULE_NAME} SHARED
            UnifiedCASManagement.cpp
            Module.cpp
            LibMediaPlayerImpl.cpp)
else()
    add_library(${MODULE_NAME} SHARED
            UnifiedCASManagement.cpp
            Module.cpp)
endif()

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES)

add_definitions( -DRT_PLATFORM_LINUX=1 )

target_include_directories(${MODULE_NAME} PRIVATE ../helpers)
target_link_libraries(${MODULE_NAME} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins)

if (LMPLAYER_FOUND)
    add_definitions(-DLMPLAYER_FOUND)
    target_include_directories(${MODULE_NAME} PRIVATE ${LMPLAYER_INCLUDE_DIRS})
    target_link_libraries(${MODULE_NAME} PRIVATE ${LMPLAYER_LIBRARIES})
else(LMPLAYER_FOUND)
    message ("MISSING A PLAYER IMPLEMENTATION.")
endif(LMPLAYER_FOUND)

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

# Implementation library (COM-RPC business logic)
add_library(${PLUGIN_IMPLEMENTATION} SHARED
        UnifiedCASManagementImplementation.cpp
        Module.cpp)

set_target_properties(${PLUGIN_IMPLEMENTATION} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES)

if (RDK_SERVICE_L2_TEST)
   find_library(TESTMOCKLIB_LIBRARIES NAMES TestMocklib)
   if (TESTMOCKLIB_LIBRARIES)
       message ("linking mock libraries ${TESTMOCKLIB_LIBRARIES} library")
       target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${TESTMOCKLIB_LIBRARIES})
   else (TESTMOCKLIB_LIBRARIES)
       message ("Require ${TESTMOCKLIB_LIBRARIES} library")
   endif (TESTMOCKLIB_LIBRARIES)
endif (RDK_SERVICE_L2_TEST)

# LMPLAYER dependency - only needed for Implementation library
if (LMPLAYER_FOUND)
    add_definitions(-DLMPLAYER_FOUND)
    target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ${LMPLAYER_INCLUDE_DIRS})
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${LMPLAYER_LIBRARIES})
    
    # Add LibMediaPlayerImpl to implementation library build
    # Include it for L2 tests as well since LMPLAYER_FOUND is ON
    target_sources(${PLUGIN_IMPLEMENTATION} PRIVATE LibMediaPlayerImpl.cpp)
else(LMPLAYER_FOUND)
    message ("MISSING A PLAYER IMPLEMENTATION.")
endif(LMPLAYER_FOUND)

# IARMBus dependency - needed for both libraries
if (IARMBus_FOUND)
    target_include_directories(${MODULE_NAME} PRIVATE ${IARMBUS_INCLUDE_DIRS})
    target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ${IARMBUS_INCLUDE_DIRS})
    target_link_libraries(${MODULE_NAME} PRIVATE ${IARMBUS_LIBRARIES})
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${IARMBUS_LIBRARIES})
else (IARMBus_FOUND)
    message ("Module IARMBus required.")
endif(IARMBus_FOUND)

target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ../helpers)
target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins)

install(TARGETS ${PLUGIN_IMPLEMENTATION}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
