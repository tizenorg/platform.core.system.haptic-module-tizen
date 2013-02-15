/*
 * haptic-module-tizen
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <haptic_plugin_intf.h>
#include "haptic_module_log.h"
#include "haptic_file.h"

#ifndef EXTAPI
#define EXTAPI __attribute__ ((visibility("default")))
#endif

#define DEFAULT_MOTOR_COUNT			1
#define DEFAULT_DEVICE_HANDLE		0x01
#define DEFAULT_EFFECT_HANDLE		0x02

#define HAPTIC_PLAY_FILE_EXT		".tht"

/* START of Static Function Section */
static int __feedback_to_tizen_type(int feedback)
{
	static int max = -1;
	int status = -1;

	if (max < 0) {
		status = GetHapticLevelMax(&max);
		if (status < 0) {
			MODULE_ERROR("GetHapticLevelMax fail : %d", status);
			return -1;
		}
	}

	MODULE_LOG("feedback value is changed : %d -> %d", feedback, (int)(feedback*max/HAPTIC_MODULE_FEEDBACK_MAX));
	return (int)(feedback*max/HAPTIC_MODULE_FEEDBACK_MAX);
}

static char* __check_file (const char *name)
{
	char *file_full_name = NULL;
	int name_length = -1;
	char match_ext[4];
	char *file_ext = NULL;

	assert(name);
	assert(*name);

	file_full_name = strdup(name);
	if (!file_full_name) {
		MODULE_ERROR("strdup failed");
		return NULL;
	}

	name_length = strlen(file_full_name) - 1;
	while (file_full_name[name_length] == ' ')
		name_length--;
	file_full_name[name_length + 1] = '\0';

	file_ext = strrchr(file_full_name, '.');
	if (!(file_ext && !strcmp(file_ext, HAPTIC_PLAY_FILE_EXT))) {
		free(file_full_name);
		MODULE_ERROR("Not supported file");
		return NULL;
	}

	return file_full_name;
}

static unsigned char* __convert_file_to_vibe_buffer(const char *file_name)
{
	char *file_full_name;
	FILE *pf;
	long file_size;
	unsigned char* pIVTData;

	if (!file_name) {
		MODULE_ERROR("Wrowng file name");
		return NULL;
	}

	file_full_name = __check_file(file_name);
	if(!file_full_name) {
		MODULE_ERROR("__check_file_faild");
		return NULL;
	}

	/* Get File Stream Pointer */
	pf = fopen(file_full_name, "rb");
	free(file_full_name);
	if (!pf) {
		MODULE_ERROR("file open failed");
		return NULL;
	}

	if (fseek(pf, 0, SEEK_END)) {
		MODULE_ERROR("fseek failed");
		fclose(pf);
		return NULL;
	}

	file_size = ftell(pf);
	if (file_size < 0) {
		MODULE_ERROR("ftell failed");
		fclose(pf);
		return NULL;
	}

	if (fseek(pf, 0, SEEK_SET)) {
		MODULE_ERROR("fseek failed");
		fclose(pf);
		return NULL;
	}

	pIVTData = (unsigned char*)malloc(file_size);
	if (!pIVTData) {
		fclose(pf);
		return NULL;
	}

	if (fread(pIVTData, 1, file_size, pf) != file_size) {
		fclose(pf);
		free(pIVTData);
		MODULE_ERROR("fread failed");
		return NULL;
	}

	fclose(pf);
	return pIVTData;
}
/* END of Static Function Section */

int haptic_internal_get_device_count(int *count)
{
	if (count == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	*count = DEFAULT_MOTOR_COUNT;
	return HAPTIC_MODULE_ERROR_NONE;
}

int haptic_internal_open_device(int device_index, int *device_handle)
{
	if (device_index < HAPTIC_MODULE_DEVICE_0 || device_index > HAPTIC_MODULE_DEVICE_ALL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (device_handle == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	*device_handle = DEFAULT_DEVICE_HANDLE;
	return HAPTIC_MODULE_ERROR_NONE;
}

int haptic_internal_close_device(int device_handle)
{
	int status = -1;

	if (device_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	status = CloseHapticDevice();
	if (status < 0) {
		MODULE_ERROR("CloseHapticDevice fail : %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	status = SetHapticEnable(0);
	if (status < 0) {
		MODULE_ERROR("SetHapticEnable fail : %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	return HAPTIC_MODULE_ERROR_NONE;
}

int haptic_internal_vibrate_monotone(int device_handle, int duration, int feedback, int priority, int *effect_handle)
{
	int status = -1;
	int input_feedback = -1;

	if (device_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (duration < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (feedback < HAPTIC_MODULE_FEEDBACK_MIN || feedback > HAPTIC_MODULE_FEEDBACK_MAX)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (priority < HAPTIC_MODULE_PRIORITY_MIN || priority > HAPTIC_MODULE_PRIORITY_HIGH)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (effect_handle == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (feedback == HAPTIC_MODULE_FEEDBACK_MIN)
		return HAPTIC_MODULE_ERROR_NONE;

	input_feedback = __feedback_to_tizen_type(feedback);
	if (input_feedback < 0)
		return HAPTIC_MODULE_OPERATION_FAILED;

	status = SetHapticLevel(input_feedback);
	if (status < 0) {
		MODULE_ERROR("SetHapticLevel fail : %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	status = SetHapticOneshot(duration);
	if (status < 0) {
		MODULE_ERROR("SetHapticOneshot fail : %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	*effect_handle = DEFAULT_EFFECT_HANDLE;
	return HAPTIC_MODULE_ERROR_NONE;
}

int haptic_internal_vibrate_file(int device_handle, const char *file_path, int iteration, int feedback, int priority, int  *effect_handle)
{
	int status = -1;
	unsigned char *vibe_buffer = NULL;
	int input_feedback = -1;
	int handle = -1;
	int i = -1;

	if (device_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (file_path == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (iteration < HAPTIC_MODULE_ITERATION_ONCE || iteration > HAPTIC_MODULE_ITERATION_INFINITE)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (feedback < HAPTIC_MODULE_FEEDBACK_MIN || feedback > HAPTIC_MODULE_FEEDBACK_MAX)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (priority < HAPTIC_MODULE_PRIORITY_MIN || priority > HAPTIC_MODULE_PRIORITY_HIGH)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (effect_handle == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (feedback == HAPTIC_MODULE_FEEDBACK_MIN)
		return HAPTIC_MODULE_ERROR_NONE;

	input_feedback = __feedback_to_tizen_type(feedback);
	if (input_feedback < 0)
		return HAPTIC_MODULE_OPERATION_FAILED;

	status = SetHapticLevel(input_feedback);
	if (status < 0) {
		MODULE_ERROR("SetHapticLevel fail : %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	vibe_buffer = __convert_file_to_vibe_buffer(file_path);
	if (!vibe_buffer) {
		MODULE_ERROR("File load filed");
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	status = PlayHapticBuffer(vibe_buffer, iteration, &handle);
	if (status < 0) {
		MODULE_ERROR("PlayHapticBuffer fail: %d", status);
		free(vibe_buffer);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	*effect_handle = handle;
	free(vibe_buffer);
	return HAPTIC_MODULE_ERROR_NONE;
}

int haptic_internal_vibrate_buffer(int device_handle, const unsigned char *vibe_buffer, int iteration, int feedback, int priority, int *effect_handle)
{
	int status = -1;
	int input_feedback = -1;
	int handle = -1;
	int i = -1;

	if (device_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (vibe_buffer == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (iteration < HAPTIC_MODULE_ITERATION_ONCE || iteration > HAPTIC_MODULE_ITERATION_INFINITE)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (feedback < HAPTIC_MODULE_FEEDBACK_MIN || feedback > HAPTIC_MODULE_FEEDBACK_MAX)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (priority < HAPTIC_MODULE_PRIORITY_MIN || priority > HAPTIC_MODULE_PRIORITY_HIGH)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (effect_handle == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (feedback == HAPTIC_MODULE_FEEDBACK_MIN)
		return HAPTIC_MODULE_ERROR_NONE;

	input_feedback = __feedback_to_tizen_type(feedback);
	if (input_feedback < 0)
		return HAPTIC_MODULE_OPERATION_FAILED;

	status = SetHapticLevel(input_feedback);
	if (status < 0) {
		MODULE_ERROR("SetHapticLevel fail : %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	status = PlayHapticBuffer(vibe_buffer, iteration, &handle);
	if (status < 0) {
		MODULE_ERROR("PlayHapticBuffer fail: %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	*effect_handle = handle;
	return HAPTIC_MODULE_ERROR_NONE;
}

int haptic_internal_stop_effect(int device_handle, int effect_handle)
{
	int status = -1;

	if (device_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (effect_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	status = SetHapticEnable(0);
	if (status < 0) {
		MODULE_ERROR("SetHapticEnable fail : %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	return HAPTIC_MODULE_ERROR_NONE;
}

int haptic_internal_stop_all_effects(int device_handle)
{
	int status = -1;

	if (device_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	status = SetHapticEnable(0);
	if (status < 0) {
		MODULE_ERROR("SetHapticEnable fail : %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	return HAPTIC_MODULE_ERROR_NONE;
}

int haptic_internal_pause_effect(int device_handle, int effect_handle)
{
	MODULE_ERROR("This device is not supported this function(%s)", __func__);
	return HAPTIC_MODULE_NOT_SUPPORTED;
}

int haptic_internal_resume_effect(int device_handle, int effect_handle)
{
	MODULE_ERROR("This device is not supported this function(%s)", __func__);
	return HAPTIC_MODULE_NOT_SUPPORTED;
}

int haptic_internal_get_effect_state(int device_handle, int effect_handle, int *state)
{
	MODULE_ERROR("This device is not supported this function(%s)", __func__);
	return HAPTIC_MODULE_NOT_SUPPORTED;
}

int haptic_internal_create_effect(unsigned char *vibe_buffer, int max_bufsize, haptic_module_effect_element *elem_arr, int max_element)
{
	int status = -1;
	HapticElement elem;
	int i = -1;

	if (vibe_buffer == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (max_bufsize < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (elem_arr == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (max_element < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	status = InitializeHapticBuffer(vibe_buffer, max_bufsize);
	if (status < 0) {
		MODULE_ERROR("InitializeHapticBuffer fail: %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	MODULE_LOG("effect count : %d", max_element);
	for (i = 0; i < max_element; ++i) {
		elem.stime = elem_arr[i].haptic_stime;
		elem.duration = elem_arr[i].haptic_duration;
		MODULE_LOG("%d) time : %d, duration : %d", i, elem_arr[i].haptic_stime, elem_arr[i].haptic_duration);

		status = InsertHapticElement(vibe_buffer, max_bufsize, &elem);
		if (status < 0) {
			MODULE_ERROR("InsertHapticElement fail: %d", status);
			return HAPTIC_MODULE_OPERATION_FAILED;
		}
	}

	return HAPTIC_MODULE_ERROR_NONE;
}

int haptic_internal_save_effect(const unsigned char *vibe_buffer, int max_bufsize, const char *file_path)
{
	int status = -1;
	FILE *file = NULL;
	int fd = -1;
	int size = -1;
	int ret;

	if (vibe_buffer == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (max_bufsize < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (file_path == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	status = GetHapticBufferSize(vibe_buffer, &size);
	if (status < 0) {
		MODULE_ERROR("GetHapticBufferSize fail: %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	file = fopen(file_path, "wb");
	if (file == NULL) {
		MODULE_ERROR("To open file is failed");
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	ret = fwrite(vibe_buffer, 1, size, file);
	if (ret != size) {
		MODULE_ERROR("To write file is failed");
		fclose(file);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	fd = fileno(file);
	if (fd < 0) {
		MODULE_ERROR("To get file descriptor is failed");
		fclose(file);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	if (fsync(fd) < 0) {
		MODULE_ERROR("To be synchronized with the disk is failed");
		fclose(file);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	fclose(file);
	return HAPTIC_MODULE_ERROR_NONE;
}

int haptic_internal_get_file_duration(int device_handle, const char *file_path, int *file_duration)
{
	int status = -1;
	unsigned char *vibe_buffer = NULL;
	int duration = -1;

	if (device_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (file_path == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (file_duration == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	vibe_buffer = __convert_file_to_vibe_buffer(file_path);
	if (!vibe_buffer) {
		MODULE_ERROR("File load filed");
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	status = GetHapticBufferDuration(vibe_buffer, &duration);
	if (status < 0) {
		MODULE_ERROR("GetHapticBufferDuration fail: %d", status);
		free(vibe_buffer);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	*file_duration = duration;
	free(vibe_buffer);
	return HAPTIC_MODULE_ERROR_NONE;
}

int haptic_internal_get_buffer_duration(int device_handle, const unsigned char *vibe_buffer, int *buffer_duration)
{
	int status = -1;
	int duration = -1;

	if (device_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (vibe_buffer == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (buffer_duration == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	status = GetHapticBufferDuration(vibe_buffer, &duration);
	if (status < 0) {
		MODULE_ERROR("GetHapticBufferDuration fail: %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	*buffer_duration = duration;
	return HAPTIC_MODULE_ERROR_NONE;
}

int haptic_internal_convert_binary (void)
{
	MODULE_ERROR("This device is not supported this function(%s)", __func__);
	return HAPTIC_MODULE_NOT_SUPPORTED;
}

static haptic_plugin_interface haptic_plugin_tizen;

EXTAPI
const haptic_plugin_interface *get_haptic_plugin_interface()
{
	haptic_plugin_tizen.haptic_internal_get_device_count= &haptic_internal_get_device_count;
	haptic_plugin_tizen.haptic_internal_open_device	 = &haptic_internal_open_device;
	haptic_plugin_tizen.haptic_internal_close_device	= &haptic_internal_close_device;
	haptic_plugin_tizen.haptic_internal_vibrate_monotone= &haptic_internal_vibrate_monotone;
	haptic_plugin_tizen.haptic_internal_vibrate_file	= &haptic_internal_vibrate_file;
	haptic_plugin_tizen.haptic_internal_vibrate_buffer  = &haptic_internal_vibrate_buffer;
	haptic_plugin_tizen.haptic_internal_stop_effect	 = &haptic_internal_stop_effect;
	haptic_plugin_tizen.haptic_internal_stop_all_effects= &haptic_internal_stop_all_effects;
	haptic_plugin_tizen.haptic_internal_pause_effect	= &haptic_internal_pause_effect;
	haptic_plugin_tizen.haptic_internal_resume_effect   = &haptic_internal_resume_effect;
	haptic_plugin_tizen.haptic_internal_get_effect_state= &haptic_internal_get_effect_state;
	haptic_plugin_tizen.haptic_internal_create_effect   = &haptic_internal_create_effect;
	haptic_plugin_tizen.haptic_internal_save_effect	 = &haptic_internal_save_effect;
	haptic_plugin_tizen.haptic_internal_get_file_duration   = &haptic_internal_get_file_duration;
	haptic_plugin_tizen.haptic_internal_get_buffer_duration = &haptic_internal_get_buffer_duration;
	haptic_plugin_tizen.haptic_internal_convert_binary  = &haptic_internal_convert_binary;

	return &haptic_plugin_tizen;
}
