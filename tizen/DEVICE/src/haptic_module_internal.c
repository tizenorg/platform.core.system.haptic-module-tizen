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
#include <vconf.h>

#include <haptic_plugin_intf.h>
#include "haptic_module_log.h"
#include "file.h"

#ifndef EXTAPI
#define EXTAPI __attribute__ ((visibility("default")))
#endif

#define DEFAULT_MOTOR_COUNT	1
#define DEFAULT_DEVICE_HANDLE	0x01
#define DEFAULT_EFFECT_HANDLE	0x02
#define HAPTIC_FEEDBACK_AUTO	101
#define HAPTIC_PLAY_FILE_EXT	".tht"

/* START of Static Function Section */
static int __to_level(int feedback, int *type)
{
	static int max = -1;
	int t;

	if (max == -1) {
		int status = GetHapticLevelMax(&max);
		if (status < 0) {
			MODULE_ERROR("GetHapticLevelMax fail : %d", status);
			return HAPTIC_MODULE_OPERATION_FAILED;
		}
	}

	t = feedback * max / HAPTIC_MODULE_FEEDBACK_MAX;
	MODULE_LOG("feedback value is changed : %d -> %d", feedback, t);

	if (type)
		*type = t;

	return 0;
}

static void __trim_name(const char *file_name, char *vibe_buffer, int size)
{
	int length;

	assert(file_name);
	assert(vibe_buffer);
	assert(size > 0);

	snprintf(vibe_buffer, size, "%s", file_name);

	length = strlen(vibe_buffer);
	while (vibe_buffer[--length] == ' ');
	vibe_buffer[length + 1] = '\0';
}

static int __check_ext(const char *name)
{
	char *ext;

	assert(name);

	ext = strrchr(name, '.');
	if (ext && !strcmp(ext, HAPTIC_PLAY_FILE_EXT))
		return 1;

	return 0;
}

static int __get_size(FILE *pf, const char *fname)
{
	int status;
	int size;

	assert(pf);

	status = fseek(pf, 0, SEEK_END);
	if (status == -1) {
		MODULE_ERROR("fseek failed: %s", fname);
		return -1;
	}

	size = ftell(pf);

	status = fseek(pf, 0, SEEK_SET);
	if (status == -1) {
		MODULE_ERROR("fseek failed: %s", fname);
		return -1;
	}

	return size;
}

static unsigned char *__read_file(const char *fname)
{
	int status;
	FILE *pf;
	long size;
	unsigned char *vibe_buffer;

	assert(fname);

	pf = fopen(fname, "rb");
	if (!pf) {
		MODULE_ERROR("fopen failed: %s", fname);
		return NULL;
	}

	size = __get_size(pf, fname);
	if (size <= 0) {
		fclose(pf);
		return NULL;
	}

	vibe_buffer = malloc(size);
	if (!vibe_buffer) {
		fclose(pf);
		MODULE_ERROR("buffer alloc failed");
		return NULL;
	}

	status = fread(vibe_buffer, 1, size, pf);
	if (status != size) {
		MODULE_ERROR("fread failed: expect %d read %d", size, status);
		free(vibe_buffer);
		vibe_buffer = NULL;
	}

	fclose(pf);

	return vibe_buffer;
}

static unsigned char* __convert_file_to_buffer(const char *file_name)
{
	char fname[FILENAME_MAX];
	int status;

	__trim_name(file_name, fname, sizeof(fname));
	status = __check_ext(fname);
	if (!status) {
		MODULE_ERROR("__check_file faild");
		return NULL;
	}

	return __read_file(fname);
}

static int __save_file(const unsigned char *vibe_buferf, int size, const char *file_name)
{
	int status;
	FILE *pf;
	int fd;

	pf = fopen(file_name, "wb+");
	if (pf == NULL) {
		MODULE_ERROR("To open file is failed");
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	status = fwrite(vibe_buferf, 1, size, pf);
	if (status != size) {
		MODULE_ERROR("To write file is failed");
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	fd = fileno(pf);
	if (fd == -1) {
		MODULE_ERROR("To get file descriptor is failed");
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	status = fsync(fd);
	if (status == -1) {
		MODULE_ERROR("To be synchronized with the disk is failed");
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	fclose(pf);

	return 0;
}

static int __vibrate(const unsigned char *vibe_buffer, int iteration, int feedback, int priority, int *effect_handle)
{
	int status;
	int level;
	int handle;

	assert(vibe_buffer);

	status = __to_level(feedback, &level);
	if (status != HAPTIC_MODULE_ERROR_NONE)
		return status;

	status = PlayHapticBuffer(vibe_buffer, iteration, level, &handle);
	if (status < 0) {
		MODULE_ERROR("PlayHapticBuffer fail: %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	if (effect_handle)
		*effect_handle = handle;

	return 0;
}
/* END of Static Function Section */


static int _get_device_count(int *count)
{
	if (count == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	*count = DEFAULT_MOTOR_COUNT;

	return HAPTIC_MODULE_ERROR_NONE;
}

static int _open_device(int device_index, int *device_handle)
{
	if (device_index < HAPTIC_MODULE_DEVICE_0 || device_index > HAPTIC_MODULE_DEVICE_ALL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (device_handle == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	*device_handle = DEFAULT_DEVICE_HANDLE;
	return HAPTIC_MODULE_ERROR_NONE;
}

static int _close_device(int device_handle)
{
	int status;

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

static int _vibrate_monotone(int device_handle, int duration, int feedback, int priority, int *effect_handle)
{
	int status;
	int input_feedback;

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

	status = __to_level(feedback, &input_feedback);
	if (status != HAPTIC_MODULE_ERROR_NONE)
		return status;

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

static int _vibrate_file(int device_handle, const char *file_path, int iteration, int feedback, int priority, int  *effect_handle)
{
	int status;
	unsigned char *vibe_buffer;

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

	vibe_buffer = __convert_file_to_buffer(file_path);
	if (!vibe_buffer) {
		MODULE_ERROR("File load filed");
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	status = __vibrate(vibe_buffer, iteration, feedback , priority, effect_handle);
	free(vibe_buffer);

	return status;
}

static int _vibrate_buffer(int device_handle, const unsigned char *vibe_buffer, int iteration, int feedback, int priority, int *effect_handle)
{
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

	return __vibrate(vibe_buffer, iteration, feedback, priority, effect_handle);
}

static int _stop_effect(int device_handle, int effect_handle)
{
	int status;

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

static int _stop_all_effects(int device_handle)
{
	int status;

	if (device_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	status = SetHapticEnable(0);
	if (status < 0) {
		MODULE_ERROR("SetHapticEnable fail : %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	return HAPTIC_MODULE_ERROR_NONE;
}

static int _pause_effect(int device_handle, int effect_handle)
{
	if (device_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (effect_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	MODULE_ERROR("This device is not supported this function(%s)", __func__);
	return HAPTIC_MODULE_NOT_SUPPORTED;
}

static int _resume_effect(int device_handle, int effect_handle)
{
	if (device_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (effect_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	MODULE_ERROR("This device is not supported this function(%s)", __func__);
	return HAPTIC_MODULE_NOT_SUPPORTED;
}

static int _get_effect_state(int device_handle, int effect_handle, int *state)
{
	if (device_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (effect_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (state == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	MODULE_ERROR("This device is not supported this function(%s)", __func__);
	return HAPTIC_MODULE_NOT_SUPPORTED;
}

static int _create_effect(unsigned char *vibe_buffer, int max_bufsize, haptic_module_effect_element *elem_arr, int max_elemcnt)
{
	int status;
	int i;
	HapticElement elem;

	if (vibe_buffer == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (max_bufsize < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (elem_arr == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (max_elemcnt < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	status = InitializeHapticBuffer(vibe_buffer, max_bufsize);
	if (status < 0) {
		MODULE_ERROR("InitializeHapticBuffer fail: %d", status);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	MODULE_LOG("effect count : %d", max_elemcnt);
	for (i = 0; i < max_elemcnt; ++i) {
		elem.duration = elem_arr[i].haptic_duration;
		if (elem_arr[i].haptic_level == HAPTIC_FEEDBACK_AUTO) {
			vconf_get_int(VCONFKEY_SETAPPL_TOUCH_FEEDBACK_VIBRATION_LEVEL_INT, &elem_arr[i].haptic_level);
			elem.level = elem_arr[i].haptic_level*20;
		}
		else {
			elem.level = elem_arr[i].haptic_level;
		}
		MODULE_LOG("%d) duration : %d, level : %d", i, elem_arr[i].haptic_duration, elem_arr[i].haptic_level);

		status = InsertHapticElement(vibe_buffer, max_bufsize, &elem);
		if (status < 0) {
			MODULE_ERROR("InsertHapticElement fail: %d", status);
			return HAPTIC_MODULE_OPERATION_FAILED;
		}
	}

	return HAPTIC_MODULE_ERROR_NONE;
}

static int _save_effect(const unsigned char *vibe_buffer, int max_bufsize, const char *file_path)
{
	int status;
	int size;

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

	return __save_file(vibe_buffer, size, file_path);
}

static int _get_file_duration(int device_handle, const char *file_path, int *file_duration)
{
	int status;
	unsigned char *vibe_buffer;
	int duration = -1;

	if (device_handle < 0)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (file_path == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	if (file_duration == NULL)
		return HAPTIC_MODULE_INVALID_ARGUMENT;

	vibe_buffer = __convert_file_to_buffer(file_path);
	if (!vibe_buffer) {
		MODULE_ERROR("File load filed");
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	status = GetHapticBufferDuration(vibe_buffer, &duration);
	free(vibe_buffer);
	if (status < 0) {
		MODULE_ERROR("GetHapticBufferDuration fail: %d", status);
		free(vibe_buffer);
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	*file_duration = duration;
	return HAPTIC_MODULE_ERROR_NONE;
}

static int _get_buffer_duration(int device_handle, const unsigned char *vibe_buffer, int *buffer_duration)
{
	int status;
	int duration;

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

static int _convert_binary (void)
{
	MODULE_ERROR("This device is not supported this function(%s)", __func__);
	return HAPTIC_MODULE_NOT_SUPPORTED;
}

static const haptic_plugin_interface haptic_plugin_tizen = {
	.haptic_internal_get_device_count		= _get_device_count,
	.haptic_internal_open_device			= _open_device,
	.haptic_internal_close_device			= _close_device,
	.haptic_internal_vibrate_monotone		= _vibrate_monotone,
	.haptic_internal_vibrate_file			= _vibrate_file,
	.haptic_internal_vibrate_buffer			= _vibrate_buffer,
	.haptic_internal_stop_effect			= _stop_effect,
	.haptic_internal_stop_all_effects		= _stop_all_effects,
	.haptic_internal_pause_effect			= _pause_effect,
	.haptic_internal_resume_effect			= _resume_effect,
	.haptic_internal_get_effect_state		= _get_effect_state,
	.haptic_internal_create_effect			= _create_effect,
	.haptic_internal_save_effect			= _save_effect,
	.haptic_internal_get_file_duration		= _get_file_duration,
	.haptic_internal_get_buffer_duration	= _get_buffer_duration,
	.haptic_internal_convert_binary			= _convert_binary,
};

EXTAPI
const haptic_plugin_interface *get_haptic_plugin_interface()
{
	return &haptic_plugin_tizen;
}
