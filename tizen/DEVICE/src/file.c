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
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <device-node.h>

#include "file.h"
#include "haptic_module_log.h"

#define BITPERMS				50
#define MAX_LEVEL				255.0f
#define DEFAULT_EFFECT_HANDLE	0x02

#define STATE_PLAY	0
#define STATE_STOP	1

#define PREDEF_HAPTIC           "haptic"

enum {
	OPEN = 0,
	CLOSE,
	PLAY,
	ONESHOT,
	STOP,
	LEVEL,
};

typedef struct {
	int handle;
	unsigned char **ppbuffer;
	int channels;
	int length;
	int iteration;
} BUFFER;

static pthread_t tid;
static BUFFER gbuffer;
static int stop;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

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

static int __haptic_predefine_action(int handle, int prop, int val)
{
	char buf_pid[32];
	char buf_prop[32];
	char buf_handle[32];
	char buf_val[32];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_prop, sizeof(buf_prop), "%d", prop);
	snprintf(buf_handle, sizeof(buf_handle), "%d", handle);
	snprintf(buf_val, sizeof(buf_val), "%d", val);

	MODULE_LOG("pid : %s(%d), prop : %s, handle : %s", buf_pid, pthread_self(), buf_prop, buf_handle);
	return __haptic_call_predef_action(PREDEF_HAPTIC, 4, buf_pid, buf_prop, buf_handle, buf_val);
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
	int *ptr;
	int ret;

	if (!tid) {
		MODULE_LOG("pthread not initialized");
		return 0;
	}

	MODULE_LOG("cancel thread!!!");

	stop = 1;

	while (pthread_mutex_trylock(&mutex) == EBUSY) {
		usleep(100);
		MODULE_LOG("Already locked..");
	}

	pthread_mutex_unlock(&mutex);

	if ((ret = pthread_cancel(tid)) < 0) {
		MODULE_ERROR("pthread_cancel is failed : %s, ret(%d)", strerror(errno), ret);
		return -1;
	}

	if (pthread_join(tid, (void**)&ptr) < 0) {
		MODULE_ERROR("pthread_join is failed : %s", strerror(errno));
		return -1;
	}

	stop = 0;

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
	int i;

	MODULE_LOG("clean up handler!!! : %d", tid);

	for (i = 0; i < pbuffer->channels; ++i) {
		free(pbuffer->ppbuffer[i]);
		pbuffer->ppbuffer[i] = NULL;
	}

	free(pbuffer->ppbuffer);
	pbuffer->ppbuffer = NULL;

	pbuffer->channels = 0;
	pbuffer->length = 0;

        if(stop){
             __haptic_predefine_action(gbuffer.handle, STOP, NULL);
             pthread_mutex_unlock(&mutex);
        }
}

static void* __play_cb(void *arg)
{
	BUFFER *pbuffer = (BUFFER*)arg;
	int i, j, k;
	unsigned char ch;
	unsigned char prev = -1;

	MODULE_LOG("Start thread");

	pthread_cleanup_push(__clean_up, arg);

	/* Copy buffer from source buffer */
	for (i = 0; i < pbuffer->iteration; i++) {
		for (j = 0; j < pbuffer->length; ++j) {
			for (k = 0; k < pbuffer->channels; ++k) {
				pthread_mutex_lock(&mutex);
				if (stop) {
					pthread_exit((void*)0);
				}
				ch = pbuffer->ppbuffer[k][j];
				if (ch != prev) {
					__haptic_predefine_action(pbuffer->handle, LEVEL, ch);
					prev = ch;
				}
				pthread_mutex_unlock(&mutex);
				usleep(BITPERMS * 1000);
			}
		}
	}

	pthread_mutex_lock(&mutex);
	__haptic_predefine_action(gbuffer.handle, STOP, NULL);
	pthread_mutex_unlock(&mutex);

	pthread_cleanup_pop(1);
	pthread_exit((void *)0);
}

int GetHapticLevelMax(int *max)
{
	int status;
	status = device_get_property(DEVICE_TYPE_VIBRATOR, PROP_VIBRATOR_LEVEL_MAX, max);
	if (status < 0) {
		MODULE_ERROR("device_get_property fail : %d", status);
		return -1;
	}
	return 0;
}

int InitializeBuffer(unsigned char *vibe_buffer, int max_bufsize)
{
	HapticFile *pfile;

	if (max_bufsize < sizeof(HapticFile)) {
		MODULE_ERROR("buffer lacks a memory : size(%d) minimum size(%d)",
					max_bufsize, sizeof(HapticFile));
		return -1;
	}

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

int InsertElement(unsigned char *vibe_buffer, int max_bufsize, HapticElement *element)
{
	HapticFile *pfile;
	int databuf;
	int needbuf;
	int duration;
	unsigned char level;
	int i;

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

int GetBufferSize(const unsigned char *vibe_buffer, int *size)
{
	HapticFile *pfile;

	pfile = (HapticFile*)vibe_buffer;
	if (_check_valid_haptic_format(pfile) < 0) {
		MODULE_ERROR("this buffer is not HapticFormat");
		return -1;
	}

	*size = pfile->chunkSize;
	return 0;
}

int GetBufferDuration(const unsigned char *vibe_buffer, int *duration)
{
	HapticFile *pfile;

	pfile = (HapticFile*)vibe_buffer;
	if (_check_valid_haptic_format(pfile) < 0) {
		MODULE_ERROR("this buffer is not HapticFormat");
		return -1;
	}

	*duration = pfile->fmt.dwDuration;
	return 0;
}

int PlayOneshot(int handle, int duration, int level)
{
	char buf_pid[32];
	char buf_prop[32];
	char buf_handle[32];
	char buf_duration[32];
	char buf_level[32];

	if (_cancel_thread() < 0) {
		MODULE_ERROR("_cancel_thread fail");
		return -1;
	}

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_prop, sizeof(buf_prop), "%d", ONESHOT);
	snprintf(buf_handle, sizeof(buf_handle), "%d", handle);
	snprintf(buf_duration, sizeof(buf_duration), "%d", duration);
	snprintf(buf_level, sizeof(buf_level), "%d", level);

	MODULE_LOG("pid : %s, prop : %s, handle : %s", buf_pid, buf_prop, buf_handle);
	return __haptic_call_predef_action(PREDEF_HAPTIC, 5, buf_pid, buf_prop,
										buf_handle, buf_duration, buf_level);
}

int PlayBuffer(int handle, const unsigned char *vibe_buffer, int iteration, int level)
{
	HapticFile *pfile;
	unsigned char **ppbuffer;
	unsigned int channels, length, align;
	unsigned char data;
	int i, j;

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

	gbuffer.handle = handle;
	gbuffer.ppbuffer = ppbuffer;
	gbuffer.channels = channels;
	gbuffer.length = length;
	gbuffer.iteration = iteration;

	__haptic_predefine_action(gbuffer.handle, PLAY, NULL);

	/* Start thread */
	if (_create_thread(&gbuffer, __play_cb) < 0) {
		MODULE_ERROR("_create_thread fail");
		return -1;
	}

	return 0;
}

int Stop(int handle)
{
	char buf_pid[32];
	char buf_prop[32];
	char buf_handle[32];

	if (_cancel_thread() < 0) {
		MODULE_ERROR("_cancel_thread fail");
		return -1;
	}

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_prop, sizeof(buf_prop), "%d", STOP);
	snprintf(buf_handle, sizeof(buf_handle), "%d", handle);

	MODULE_LOG("pid : %s, prop : %s, handle : %s", buf_pid, buf_prop, buf_handle);
	return __haptic_call_predef_action(PREDEF_HAPTIC, 3, buf_pid, buf_prop, buf_handle);
}

int OpenDevice(int handle)
{
	char buf_pid[32];
	char buf_prop[32];
	char buf_handle[32];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_prop, sizeof(buf_prop), "%d", OPEN);
	snprintf(buf_handle, sizeof(buf_handle), "%d", handle);

	MODULE_LOG("pid : %s, prop : %s, handle : %s", buf_pid, buf_prop, buf_handle);
	return __haptic_call_predef_action(PREDEF_HAPTIC, 3, buf_pid, buf_prop, buf_handle);
}

int CloseDevice(int handle)
{
	char buf_pid[32];
	char buf_prop[32];
	char buf_handle[32];

	if (_cancel_thread() < 0) {
		MODULE_ERROR("_cancel_thread fail");
		return -1;
	}

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_prop, sizeof(buf_prop), "%d", CLOSE);
	snprintf(buf_handle, sizeof(buf_handle), "%d", handle);

	MODULE_LOG("pid : %s, prop : %s, handle : %s", buf_pid, buf_prop, buf_handle);
	return __haptic_call_predef_action(PREDEF_HAPTIC, 3, buf_pid, buf_prop, buf_handle);
}

int GetState(int handle, int *state)
{
	if (gbuffer.handle == handle) {
		*state = STATE_PLAY;
		return 0;
	}

	*state = STATE_STOP;
	return 0;
}
