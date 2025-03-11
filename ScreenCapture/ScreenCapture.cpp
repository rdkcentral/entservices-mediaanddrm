/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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

#include "ScreenCapture.h"

#include "UtilsJsonRpc.h"

#include <png.h>
#include <curl/curl.h>

#include <iostream>
#include <vector>
#include <iomanip>

#ifdef HAS_SCREENCAPTURE_PLATFORM
#include "screencaptureplatform.h"
#elif USE_DRM_SCREENCAPTURE
#include "Implementation/drm/drmsc.h"
#else
#error "Can't build without drm or platform getter !"
#endif



// Methods
#define METHOD_UPLOAD "uploadScreenCapture"

// Events
#define EVT_UPLOAD_COMPLETE "uploadComplete"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 3

namespace WPEFramework
{

    namespace {

        static Plugin::Metadata<Plugin::ScreenCapture> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
    }

    namespace Plugin
    {
        SERVICE_REGISTRATION(ScreenCapture, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

        ScreenCapture::ScreenCapture()
            : PluginHost::JSONRPC()
	    , screenShotDispatcher(nullptr)
        {
            Register(METHOD_UPLOAD, &ScreenCapture::uploadScreenCapture, this);
        }

        ScreenCapture::~ScreenCapture()
        {
        }

        /* virtual */ const string ScreenCapture::Initialize(PluginHost::IShell* service)
        {
            screenShotDispatcher = new WPEFramework::Core::TimerType<ScreenShotJob>(64 * 1024, "ScreenCaptureDispatcher");
            return { };
        }

        void ScreenCapture::Deinitialize(PluginHost::IShell* /* service */)
        {

            delete screenShotDispatcher;
        }

        uint32_t ScreenCapture::uploadScreenCapture(const JsonObject& parameters, JsonObject& response)
        {
            
            std::lock_guard<std::mutex> guard(m_callMutex);

            LOGINFOMETHOD();

            if(!parameters.HasLabel("url"))
            {
                response["message"] = "Upload url is not specified";

                returnResponse(false);
            }

            url = parameters["url"].String();

            std::cout <<"akshay : url = "<<url<<std::endl;

            if(parameters.HasLabel("callGUID"))
              callGUID = parameters["callGUID"].String();

            std::cout <<"akshay : callGUID = "<<callGUID<<std::endl;
            std::cout <<"akshay calling ScreenShotJob"<<std::endl;
            screenShotDispatcher->Schedule( Core::Time::Now().Add(0), ScreenShotJob( this) );

            std::cout <<"akshay after ScreenShotJob"<<std::endl;

            returnResponse(true);
        }

        static void PngWriteCallback(png_structp  png_ptr, png_bytep data, png_size_t length)
        {
            std::vector<unsigned char> *p = (std::vector<unsigned char>*)png_get_io_ptr(png_ptr);
            p->insert(p->end(), data, data + length);
        }
       
        static bool saveToPng(unsigned char *data, int width, int height, std::vector<unsigned char> &png_out_data)
    {
        std::cout << "akshay inside saveToPng" << std::endl;
        int bitdepth = 8;
        int colortype = PNG_COLOR_TYPE_RGBA;
        int pitch = 4 * width;
        int transform = PNG_TRANSFORM_IDENTITY;

    int i = 0;
    int r = 0;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep* row_pointers = NULL;

    if (NULL == data)
    {
        LOGERR("Error: failed to save the png because the given data is NULL.");
        r = -1;
        goto error;
    }
    std::cout << "akshay data is valid" << std::endl;

    if (0 == pitch)
    {
        LOGERR("Error: failed to save the png because the given pitch is 0.");
        r = -3;
        goto error;
    }
    std::cout << "akshay pitch is valid" << std::endl;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (NULL == png_ptr)
    {
        LOGERR("Error: failed to create the png write struct.");
        r = -5;
        goto error;
    }
    std::cout << "akshay png_create_write_struct successful" << std::endl;

    info_ptr = png_create_info_struct(png_ptr);
    if (NULL == info_ptr)
    {
        LOGERR("Error: failed to create the png info struct.");
        r = -6;
        goto error;
    }
    std::cout << "akshay png_create_info_struct successful" << std::endl;

    png_set_IHDR(png_ptr,
                 info_ptr,
                 width,
                 height,
                 bitdepth,                 /* e.g. 8 */
                 colortype,                /* PNG_COLOR_TYPE_{GRAY, PALETTE, RGB, RGB_ALPHA, GRAY_ALPHA, RGBA, GA} */
                 PNG_INTERLACE_NONE,       /* PNG_INTERLACE_{NONE, ADAM7 } */
                 PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);
    std::cout << "akshay png_set_IHDR successful" << std::endl;

    row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    if (NULL == row_pointers)
    {
        LOGERR("Error: failed to allocate memory for row pointers.");
        r = -7;
        goto error;
    }
    std::cout << "akshay row_pointers allocation successful" << std::endl;

    for (i = 0; i < height; ++i)
        row_pointers[i] = data + i * pitch;
    std::cout << "akshay row_pointers setup successful" << std::endl;

    png_set_write_fn(png_ptr, &png_out_data, PngWriteCallback, NULL);
    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_write_png(png_ptr, info_ptr, transform, NULL);
    std::cout << "akshay png_write_png successful" << std::endl;

error:

    if (NULL != png_ptr)
    {
        if (NULL == info_ptr)
        {
            LOGERR("Error: info ptr is null. not supposed to happen here.\n");
        }

        png_destroy_write_struct(&png_ptr, &info_ptr);
        png_ptr = NULL;
        info_ptr = NULL;
    }
    std::cout << "akshay cleanup png_ptr and info_ptr" << std::endl;

    if (NULL != row_pointers)
    {
        free(row_pointers);
        row_pointers = NULL;
    }
    std::cout << "akshay cleanup row_pointers" << std::endl;

    return (r == 0);
    }

        void printPngData(const std::vector<unsigned char>& png_data) 
        {
            std::cout << "akshay png_data size: " << png_data.size() << std::endl;
            std::cout << "akshay png_data contents: ";
            for (size_t i = 0; i < png_data.size(); ++i) {
            std::cout << "akshay" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(png_data[i]) << " ";
            if ((i + 1) % 16 == 0) { // Print 16 bytes per line for readability
                std::cout << "akshay in bytes"<<std::endl;
            }
    }
    std::cout << "akshay value of png_data" << std::dec << std::endl; // Reset to decimal format
}


#ifdef USE_DRM_SCREENCAPTURE
        static bool getScreenContent(std::vector<unsigned char> &png_out_data)
        {
            bool ret = true;
            uint8_t *buffer = nullptr;
            DRMScreenCapture *handle = nullptr;
            uint32_t size;
            do
            {
                handle = DRMScreenCapture_Init();
                if (handle == nullptr)
                {
                    LOGERR("[SCREENCAP] fail to DRMScreenCapture_Init ");
                    ret = false;
                    break;
                }

                // get screen size
                ret = DRMScreenCapture_GetScreenInfo(handle);
                if (!ret)
                {
                    LOGERR("[SCREENCAP] fail to DRMScreenCapture_GetScreenInfo ");
                    break;
                }

                // allocate buffer
                size = handle->pitch * handle->height;
                buffer = (uint8_t *)malloc(size);
                if (!buffer)
                {
                    LOGERR("[SCREENCAP] out of memory, fail to allocate buffer size=%d", size);
                    ret = false;
                    break;
                }

                ret = DRMScreenCapture_ScreenCapture(handle, buffer, size);
                if (!ret)
                {
                    LOGERR("[SCREENCAP] fail to DRMScreenCapture_ScreenCapture ");
                    break;
                }

                // process the rgb buffer
                for (unsigned int n = 0; n < handle->height; n++)
                {
                    for (unsigned int i = 0; i < handle->width; i++)
                    {
                        unsigned char *color = buffer + n * handle->pitch + i * 4;
                        unsigned char blue = color[0];
                        color[0] = color[2];
                        color[2] = blue;
                    }
                }

                std::cout <<"akshay before saveToPng"<<std::endl;
                if (!saveToPng(buffer, handle->width, handle->height, png_out_data))
                {
                    std::cout <<"akshay after saveToPng"<<std::endl;
                    LOGERR("[SCREENCAP] could not convert screenshot to png");
                    std::cout <<"akshay setting ret to false"<<std::endl;
                    ret = false;
                    break;
                }

                LOGINFO("[SCREENCAP] done");
            } while (false);

            // Finish and clean up
            if (buffer)
            {
                free(buffer);
            }
            if (handle)
            {
                ret = DRMScreenCapture_Destroy(handle);
                if (!ret)
                    LOGERR("[SCREENCAP] fail to DRMScreenCapture_Destroy ");
            }

            return ret;
        }
#endif

        uint64_t ScreenShotJob::Timed(const uint64_t scheduledTime)
        {
            std::cout <<"akshay inside ScreenShotJob"<<std::endl;
            if(!m_screenCapture)
            {
                LOGERR("!m_screenCapture");
                return 0;
            }

            std::vector<unsigned char> png_data;
            bool got_screenshot = false;

            got_screenshot = getScreenContent(png_data);
            printPngData(png_data);


            m_screenCapture->doUploadScreenCapture(png_data, got_screenshot);

            return 0;
        }

        bool ScreenCapture::doUploadScreenCapture(const std::vector<unsigned char> &png_data, bool got_screenshot)
        {
            if(got_screenshot)
            {
                std::string error_str;

                printPngData(png_data);

                LOGWARN("uploading %u of png data to '%s'", (uint32_t)png_data.size(), url.c_str() );

                if(uploadDataToUrl(png_data, url.c_str(), error_str))
                {
                    JsonObject params;
                    params["status"] = true;
                    params["message"] = "Success";
                    params["call_guid"] = callGUID;

                    sendNotify(EVT_UPLOAD_COMPLETE, params);

                    return true;
                }
                else
                {
                    JsonObject params;
                    params["status"] = false;
                    params["message"] = std::string("Upload Failed: ") + error_str;
                    params["call_guid"] = callGUID;

                    sendNotify(EVT_UPLOAD_COMPLETE, params);

                    return false;
                }
            }
            else
            {
                LOGERR("Error: could not get the screenshot");

                JsonObject params;
                params["status"] = false;
                params["message"] = "Failed to get screen data";
                params["call_guid"] = callGUID;

                sendNotify(EVT_UPLOAD_COMPLETE, params);

                return false;
            }
        }

        bool ScreenCapture::uploadDataToUrl(const std::vector<unsigned char> &data, const char *url, std::string &error_str)
        {
            CURL *curl;
            CURLcode res;
            bool call_succeeded = true;

            if(!url || !strlen(url))
            {
                LOGERR("no url given");
                return false;
            }

            LOGWARN("uploading png data of size %u to '%s'", (uint32_t)data.size(), url);

            //init curl
            curl_global_init(CURL_GLOBAL_ALL);
            curl = curl_easy_init();

            if(!curl)
            {
                LOGERR("could not init curl\n");
                return false;
            }

            //create header
            struct curl_slist *chunk = NULL;
            chunk = curl_slist_append(chunk, "Content-Type: image/png");

            //set url and data
            res = curl_easy_setopt(curl, CURLOPT_URL, url);
            if( res != CURLE_OK )
            {
                LOGERR("CURLOPT_URL failed URL with error code: %s\n", curl_easy_strerror(res));
            }
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            if( res != CURLE_OK )
            {
                LOGERR("Failed to set CURLOPT_HTTPHEADER with error code  %s\n", curl_easy_strerror(res));
            }
            res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());
            if( res != CURLE_OK )
            {
                LOGERR("Failed to set CURLOPT_POSTFIELDSIZE with error code  %s\n", curl_easy_strerror(res));
            }
            res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, &data[0]);
            if( res != CURLE_OK )
            {
                LOGERR("Failed to set CURLOPT_POSTFIELDS with code  %s\n", curl_easy_strerror(res));
            }

            //perform blocking upload call
            res = curl_easy_perform(curl);

            //output success / failure log
            if(CURLE_OK == res)
            {
                long response_code;

                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

                if(600 > response_code && response_code >= 400)
                {
                    LOGERR("uploading failed with response code %ld\n", response_code);
                    error_str = std::string("response code:") + std::to_string(response_code);
                    call_succeeded = false;
                }
                else
                    LOGWARN("upload done");
            }
            else
            {
                LOGERR("upload failed with error %d:'%s'", res, curl_easy_strerror(res));
                error_str = std::to_string(res) + std::string(":'") + std::string(curl_easy_strerror(res)) + std::string("'");
                call_succeeded = false;
            }

            //clean up curl object
            curl_easy_cleanup(curl);
            curl_slist_free_all(chunk);

            return call_succeeded;
        }


    } // namespace Plugin
} // namespace WPEFramework
