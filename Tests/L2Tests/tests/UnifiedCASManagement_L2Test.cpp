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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "L2Tests.h"
#include "L2TestsMock.h"

using namespace WPEFramework;

#define UNIFIEDCASMANAGEMENT_CALLSIGN _T("org.rdk.UnifiedCASManagement")
#define UNIFIEDCASMANAGEMENTL2TEST_CALLSIGN _T("L2tests.1")
#define JSON_TIMEOUT (1000)


class UnifiedCASManagementL2Test : public L2TestMocks {
protected:
    virtual ~UnifiedCASManagementL2Test() override {
        std::cout << "UnifiedCASManagementL2Test destructor called" << std::endl;
        uint32_t status = Core::ERROR_GENERAL;
        status = DeactivateService("org.rdk.UnifiedCASManagement");
        EXPECT_EQ(Core::ERROR_NONE, status);
    }
public:
    UnifiedCASManagementL2Test() {
        std::cout << "UnifiedCASManagementL2Test constructor called" << std::endl;
        uint32_t status = Core::ERROR_GENERAL;
        status = ActivateService("org.rdk.UnifiedCASManagement");
        EXPECT_EQ(Core::ERROR_NONE, status);
    }
};

TEST_F(UnifiedCASManagementL2Test, Manage_Api_Test) {
    std::cout << "Manage_Api_Test test started" << std::endl;
    JsonObject params, result;
    params["mediaurl"] = "http://test/media";
    params["mode"] = "MODE_NONE";
    params["manage"] = "MANAGE_FULL";
    params["casinitdata"] = "initdata";
    params["casocdmid"] = "ocdmid";
    uint32_t status = InvokeServiceMethod(UNIFIEDCASMANAGEMENT_CALLSIGN, "manage", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    // Validate success response as per API spec
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
    std::cout << "Manage_Api_Test test completed" << std::endl;
}
/**/
// Negative test: mode != "MODE_NONE"
TEST_F(UnifiedCASManagementL2Test, Manage_Api_Mode_Invalid_Test) {
    JsonObject params, result;
    params["mediaurl"] = "http://test/media";
    params["mode"] = "MODE_X"; // Invalid mode
    params["manage"] = "MANAGE_FULL";
    params["casinitdata"] = "initdata";
    params["casocdmid"] = "ocdmid";
    uint32_t status = InvokeServiceMethod(UNIFIEDCASMANAGEMENT_CALLSIGN, "manage", params, result);
    EXPECT_NE(Core::ERROR_NONE, status);
}

// Negative test: manage not in allowed values
TEST_F(UnifiedCASManagementL2Test, Manage_Api_Manage_Invalid_Test) {
    JsonObject params, result;
    params["mediaurl"] = "http://test/media";
    params["mode"] = "MODE_NONE";
    params["manage"] = "INVALID_MANAGE"; // Invalid manage
    params["casinitdata"] = "initdata";
    params["casocdmid"] = "ocdmid";
    uint32_t status = InvokeServiceMethod(UNIFIEDCASMANAGEMENT_CALLSIGN, "manage", params, result);
    EXPECT_NE(Core::ERROR_NONE, status);
}

// Negative test: casocdmid is empty
TEST_F(UnifiedCASManagementL2Test, Manage_Api_CasOcdmId_Empty_Test) {
    JsonObject params, result;
    params["mediaurl"] = "http://test/media";
    params["mode"] = "MODE_NONE";
    params["manage"] = "MANAGE_FULL";
    params["casinitdata"] = "initdata";
    params["casocdmid"] = ""; // Empty casocdmid
    uint32_t status = InvokeServiceMethod(UNIFIEDCASMANAGEMENT_CALLSIGN, "manage", params, result);
    EXPECT_NE(Core::ERROR_NONE, status);
}


TEST_F(UnifiedCASManagementL2Test, Unmanage_Api_Test) {
    std::cout << "Unmanage_Api_Test test started" << std::endl;
    // First, start a session with manage
    JsonObject manageParams, manageResult;
    manageParams["mediaurl"] = "http://test/media";
    manageParams["mode"] = "MODE_NONE";
    manageParams["manage"] = "MANAGE_FULL";
    manageParams["casinitdata"] = "initdata";
    manageParams["casocdmid"] = "ocdmid";
    uint32_t manageStatus = InvokeServiceMethod(UNIFIEDCASMANAGEMENT_CALLSIGN, "manage", manageParams, manageResult);
    EXPECT_EQ(Core::ERROR_NONE, manageStatus);
    EXPECT_TRUE(manageResult.HasLabel("success"));
    EXPECT_TRUE(manageResult["success"].Boolean());

    // Now call unmanage
    JsonObject params, result;
    uint32_t status = InvokeServiceMethod(UNIFIEDCASMANAGEMENT_CALLSIGN, "unmanage", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    // Validate success response as per API spec
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
    std::cout << "Unmanage_Api_Test test completed" << std::endl;
}

TEST_F(UnifiedCASManagementL2Test, Unmanage_Api_Negative_Test) {
    std::cout << "Unmanage_Api_Negative_Test test started" << std::endl;
    JsonObject params, result;
    uint32_t status = InvokeServiceMethod(UNIFIEDCASMANAGEMENT_CALLSIGN, "unmanage", params, result);
    EXPECT_NE(Core::ERROR_NONE, status);
    // Validate success response as per API spec
    EXPECT_FALSE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
    std::cout << "Unmanage_Api_Negative_Test test completed" << std::endl;
}

TEST_F(UnifiedCASManagementL2Test, Send_Api_Test) {
    std::cout << "Send_Api_Test test started" << std::endl;
    // First, start a session with manage
    JsonObject manageParams, manageResult;
    manageParams["mediaurl"] = "http://test/media";
    manageParams["mode"] = "MODE_NONE";
    manageParams["manage"] = "MANAGE_FULL";
    manageParams["casinitdata"] = "initdata";
    manageParams["casocdmid"] = "ocdmid";
    uint32_t manageStatus = InvokeServiceMethod(UNIFIEDCASMANAGEMENT_CALLSIGN, "manage", manageParams, manageResult);
    EXPECT_EQ(Core::ERROR_NONE, manageStatus);
    EXPECT_TRUE(manageResult.HasLabel("success"));
    EXPECT_TRUE(manageResult["success"].Boolean());

    // Now call send
    JsonObject params, result;
    params["payload"] = "testdata";
    params["source"] = "testsource";
    uint32_t status = InvokeServiceMethod(UNIFIEDCASMANAGEMENT_CALLSIGN, "send", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    // Validate success response as per API spec
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
    std::cout << "Send_Api_Test test completed" << std::endl;
}

TEST_F(UnifiedCASManagementL2Test, Send_Api_Negative_Test) {
    std::cout << "Send_Api_Negative_Test test started" << std::endl;

    JsonObject params, result;
    params["payload"] = "testdata";
    params["source"] = "testsource";
    uint32_t status = InvokeServiceMethod(UNIFIEDCASMANAGEMENT_CALLSIGN, "send", params, result);
    EXPECT_NE(Core::ERROR_NONE, status);
    // Validate success response as per API spec
    EXPECT_FALSE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
    std::cout << "Send_Api_Negative_Test test completed" << std::endl;
}
