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
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <device-node.h>

#include "file.h"
#include "haptic_module_log.h"

#define BITPERMS				50
#define MAX_LEVEL				255.0f
#define DEFAULT_EFFECT_HANDLE	0x02

enum {
	PLAY_HAPTIC = 0,
	STOP_HAPTIC,
	LEVEL_HAPTIC,
};

typedef struct {
	unsigned char **ppbuffer;
	int channels;
	int length;
	int iteration;
} BUFFER;

static pthread_t tid;
static BUFFER gbuffer;

static int _check_valid_haptic_format(HapticFile *file)
{
	if (file->chunkID != HEADER_ID)
		return -1;

	if (file->fmt.chunkID != FMT_ID)
		return -1;

	if (file->data.chunkID != DATA_ID)
		return -1;

	return 0;
}

static int _create_thread(void* data, void*(*func)(void*))
{
	if (tid) {
		MODULE_ERROR("pthread already created");
		return -1;
	}

	if (pthread_create(&tid, NULL, func, data) != 0) {
		MODULE_ERROR("pthread_create is failed : %s", strerror(errno));
		return -1;
	}

	return 0;
}

static int _cancel_thread(void)
{
	int *ptr = NULL;
	int ret = -1;

	if (!tid) {
		MODULE_LOG("pthread not initialized");
		return 0;
	}

	if ((ret = pthread_cancel(tid)) < 0) {
		MODULE_ERROR("pthread_cancel is failed : %s, ret(%d)", strerror(errno), ret);
		return -1;
	}

	if (pthread_join(tid, (void**)&ptr) < 0) {
        tid = 0;
		MODULE_ERROR("pthread_join is failed : %s", strerror(errno));
		return -1;
	}

    tid = 0;
	if (ptr == PTHREAD_CANCELED) {
		MODULE_LOG("pthread canceled");
	} else {
		MODULE_LOG("pthread already finished");
	}

	return 0;
}

static void __clean_up(void *arg)
{
	BUFFER *pbuffer = (BUFFER*)arg;
	int i = 0;

	MODULE_LOG("clean up handler!!! : %d", tid);
	__haptic_predefine_action(STOP_HAPTIC, NULL);

	for (i = 0; i < pbuffer->channels; ++i) {
		free(pbuffer->ppbuffer[i]);
		pbuffer->ppbuffer[i] = NULL;
	}

	free(pbuffer->ppbuffer);
	pbuffer->ppbuffer = NULL;

	pbuffer->channels = 0;
	pbuffer->length = 0;
}

static void* __play_cb(void *arg)
{
	BUFFER *pbuffer = (BUFFER*)arg;
	int i = -1, j = -1, k = -1, value = -1;
	unsigned char ch;
	unsigned char prev = -1;

	MODULE_LOG("Start thread");

	pthread_cleanup_push(__clean_up, arg);

	__haptic_predefine_action(PLAY_HAPTIC, NULL);

	/* Copy buffer from source buffer */
	for (i = 0; i < pbuffer->iteration; i++) {
		for (j = 0; j < pbuffer->length; ++j) {
			for (k = 0; k < pbuffer->channels; ++k) {
				ch = pbuffer->ppbuffer[k][j];
				if (ch != prev) {
					__haptic_predefine_action(LEVEL_HAPTIC, ch);
					prev = ch;
				}
				usleep(BITPERMS * 1000);
			}
		}
	}

	pthread_cleanup_pop(1);
	pthread_exit((void *)0);
}

int GetHapticLevelMax(int *max)
{
	int status = -1;
	status = device_get_property(DEVICE_TYPE_VIBRATOR, PROP_VIBRATOR_LEVEL_MAX, max);
	if (status < 0) {
		MODULE_ERROR("device_get_property fail : %d", status);
		return -1;
	}
	return 0;
}

int SetHapticEnable(int value)
{
	int status = -1;
	status = device_set_property(DEVICE_TYPE_VIBRATOR, PROP_VIBRATOR_ENABLE, value);
	if (status < 0) {
		MODULE_ERROR("device_set_property fail : %d", status);
		return -1;
	}
	return 0;
}

int SetHapticLevel(int value)
{
	int status = -1;
	status = device_set_property(DEVICE_TYPE_VIBRATOR, PROP_VIBRATOR_LEVEL, value);
	if (status < 0) {
		MODULE_ERROR("device_set_property fail : %d", status);
		return -1;
	}
	return 0;
}

int SetHapticOneshot(int value)
{
	int status = -1;
	status = device_set_property(DEVICE_TYPE_VIBRATOR, PROP_VIBRATOR_ONESHOT, value);
	if (status < 0) {
		MODULE_ERROR("device_set_property fail : %d", status);
		return -1;
	}
	return 0;
}

int InitializeHapticBuffer(unsigned char *vibe_buffer, int max_bufsize)
{
	HapticFile *pfile = NULL;

	if (max_bufsize < sizeof(HapticFile)) {
		MODULE_ERROR("buffer lacks a memory : size(%d) minimum size(%d)", max_bufsize, sizeof(HapticFile));
		return -1;
	}

	MODULE_LOG("FormatChunk : %d, DataChunk : %d, HapticFile : %d", sizeof(FormatChunk), sizeof(DataChunk), sizeof(HapticFile));
	MODULE_LOG("Bufsize : %d", max_bufsize);

	memset(vibe_buffer, 0, sizeof(char)*max_bufsize);

	pfile = (HapticFile*)vibe_buffer;

	pfile->chunkID = HEADER_ID;
	pfile->chunkSize = sizeof(HapticFile);
	pfile->fmt.chunkID = FMT_ID;
	pfile->fmt.chunkSize = sizeof(FormatChunk);
	pfile->fmt.wChannels = 1;
	pfile->fmt.wBlockAlign = 1;	// wChannels*1byte
	pfile->fmt.dwMagnitude = 99;
	pfile->fmt.dwDuration = 0;
	pfile->data.chunkID = DATA_ID;
	pfile->data.chunkSize = sizeof(DataChunk);
	return 0;
}

int InsertHapticElement(unsigned char *vibe_buffer, int max_bufsize, HapticElement *element)
{
	HapticFile *pfile = NULL;
	int databuf = -1;
	int needbuf = -1;
	int duration;
	unsigned char level;
	int i = -1;

	pfile = (HapticFile*)vibe_buffer;
	if (_check_valid_haptic_format(pfile) < 0) {
		MODULE_ERROR("this buffer is not HapticFormat");
		return -1;
	}

	duration = element->duration/BITPERMS;
	level = (unsigned char)((unsigned int)element->level*MAX_LEVEL/100);

	databuf = max_bufsize - sizeof(HapticFile);
	needbuf = (pfile->fmt.dwDuration + duration)*pfile->fmt.wBlockAlign;
	MODULE_LOG("Need buffer size : %d", needbuf);

	if (databuf < needbuf) {
		MODULE_ERROR("buffer lacks a memory : data buf(%d), need buf(%d)", databuf, needbuf);
		return -1;
	}

	for (i = pfile->fmt.dwDuration; i < pfile->fmt.dwDuration+duration; i++) {
		pfile->data.pData[i] = level;
	}

	pfile->chunkSize = sizeof(HapticFile)+needbuf ;
	pfile->fmt.dwDuration = pfile->fmt.dwDuration+duration;
	pfile->data.chunkSize = sizeof(DataChunk)+needbuf;
	return 0;
}

int GetHapticBufferSize(const unsigned char *vibe_buffer, int *size)
{
	HapticFile *pfile = NULL;

	pfile = (HapticFile*)vibe_buffer;
	if (_check_valid_haptic_format(pfile) < 0) {
		MODULE_ERROR("this buffer is not HapticFormat");
		return -1;
	}

	*size = pfile->chunkSize;
	return 0;
}

int GetHapticBufferDuration(const unsigned char *vibe_buffer, int *duration)
{
	HapticFile *pfile = NULL;

	pfile = (HapticFile*)vibe_buffer;
	if (_check_valid_haptic_format(pfile) < 0) {
		MODULE_ERROR("this buffer is not HapticFormat");
		return -1;
	}

	*duration = pfile->fmt.dwDuration;
	return 0;
}

int PlayHapticBuffer(const unsigned char *vibe_buffer, int iteration, int level, int *effect_handle)
{
	HapticFile *pfile = NULL;
	unsigned char **ppbuffer = NULL;
	unsigned int channels, length, align;
	unsigned char data;
	int i = -1, j = -1;

	pfile = (HapticFile*)vibe_buffer;
	if (_check_valid_haptic_format(pfile) < 0) {
		MODULE_ERROR("this buffer is not HapticFormat");
		return -1;
	}

	/* Temporary code
	   This code does not support handle and multi channel concept.
	   Only this code adds to test for playing file. */

	if (_cancel_thread() < 0) {
		MODULE_ERROR("_cancel_thread fail");
		return -1;
	}

	channels = pfile->fmt.wChannels;
	align = pfile->fmt.wBlockAlign;
	length = (pfile->data.chunkSize-8)/align;
	MODULE_LOG("channels : %d, length : %d, align : %d, level : %d", channels, length, align, level);

	/* Create buffer */
	ppbuffer = (unsigned char**)malloc(sizeof(unsigned char*)*channels);
	for (i = 0; i < channels; ++i) {
		ppbuffer[i] = (unsigned char*)malloc(sizeof(unsigned char)*length);
		memset(ppbuffer[i], 0, sizeof(unsigned char)*length);
	}

	/* Copy buffer from source buffer */
	for (i = 0; i < length; ++i) {
		for (j = 0; j < channels; ++j) {
			data = (unsigned char)(pfile->data.pData[i*align+j]);
			ppbuffer[j][i] = (unsigned char)(data*level/0xFF);
			MODULE_LOG("ppbuffer[%2d][%2d] : data(%x) -> (%x)", j, i, data, ppbuffer[j][i]);
		}
	}

	gbuffer.ppbuffer = ppbuffer;
	gbuffer.channels = channels;
	gbuffer.length = length;
	gbuffer.iteration = iteration;

	/* Start thread */
	if (_create_thread(&gbuffer, __play_cb) < 0) {
		MODULE_ERROR("_create_thread fail");
		return -1;
	}

	*effect_handle = DEFAULT_EFFECT_HANDLE;
	return 0;
}

int CloseHapticDevice(void)
{
	if (_cancel_thread() < 0) {
		MODULE_ERROR("_cancel_thread fail");
		return -1;
	}
	return 0;
}
