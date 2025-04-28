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
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "kms.h"
#include "drmsc.h"

#ifndef DEFAULT_DEVICE
#define DEFAULT_DEVICE "/dev/dri/card0"
#endif

using namespace std;

typedef struct DRMContext_s {
	int fd;
	kms_ctx *kms;
	uint64_t offset;
} DRMContext;


DRMScreenCapture* DRMScreenCapture_Init() {
	DRMScreenCapture *handle = nullptr;
	DRMContext *context = nullptr;

	do {
		// init variables
		handle = (DRMScreenCapture*)calloc(1, sizeof(DRMScreenCapture));
		if(!handle) {
			cout << "[SCREENCAP] fail to calloc DRMScreenCapture" << endl;
			break;
		}
		context = (DRMContext*)calloc(1, sizeof(DRMContext));
		if(!context) {
			cout << "[SCREENCAP] fail to calloc DRMContext" << endl;
			break;
		}
		handle->context = (void*) context;

		return handle;
	} while(0);

	// fail to calloc
	free(handle);
	free(context);
	return nullptr;
}

bool DRMScreenCapture_GetScreenInfo(DRMScreenCapture* handle) {
	return true;

}

bool DRMScreenCapture_ScreenCapture(DRMScreenCapture* handle, uint8_t* output, uint32_t bufSize) {
	bool ret = true;
        uint8_t* buffer = (uint8_t*) malloc(5120 * 720);
        memset(buffer, 0xff, 5120 * 720);
	memcpy(output, buffer, size);

	return ret;
}

bool DRMScreenCapture_Destroy(DRMScreenCapture* handle) {
	DRMContext *context;
	if(!handle) {
		cout << "[SCREENCAP] null input parameter" << endl;
		return false;
	}

	context = (DRMContext*) handle->context;
	if(context) {
		if(context->kms) {
			kms_cleanup_context(context->kms);
			free(context->kms);
			context->kms = nullptr;
		}

		if(context->fd) {
			close(context->fd);
			context->fd = 0;
		}
		free(context);
		handle->context = nullptr;
	}
	free(handle);
	return true;
}
