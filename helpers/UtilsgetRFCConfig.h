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

#pragma once

#include "rfcapi.h"

namespace Utils {
inline bool getRFCConfig(const char* paramName, RFC_ParamData_t& paramOutput)
{
    WDMP_STATUS wdmpStatus = getRFCParameter(nullptr, paramName, &paramOutput);
    std::cout << "akshay: getRFCConfig called for paramName: " << paramName << " with status: " << wdmpStatus << std::endl;
    std::cout << "akshay: getRFCConfig paramOutput.value: " << paramOutput.value << std::endl;
    if (wdmpStatus == WDMP_SUCCESS || wdmpStatus == WDMP_ERR_DEFAULT_VALUE) {
        std::cout << "akshay: getRFCConfig succeeded for paramName: " << paramName << std::endl;
        std::cout << "akshay: getRFCConfig paramOutput.value: " << paramOutput.value << std::endl;
        std::cout << "akshay: retuting true from getRFCConfig" << std::endl;
        return true;
    }
    std::cout << "akshay: getRFCConfig failed for paramName: " << paramName << " with status: " << wdmpStatus << std::endl;
    std::cout << "akshay: returning false from getRFCConfig" << std::endl;
    return false;
}
}
