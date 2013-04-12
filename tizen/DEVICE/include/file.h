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


#ifndef __FILE_H__
#define __FILE_H__

#define GCC_PACK	__attribute__((packed))

/* little-endian form */
#define mmioHeaderID(ch0,ch1,ch2,ch3)	\
	((unsigned int)(unsigned char)(ch0) | ((unsigned int)(unsigned char)(ch1) << 8) |	\
	((unsigned int)(unsigned char)(ch2) << 16) | ((unsigned int)(unsigned char)(ch3) << 24))

#define HEADER_ID		mmioHeaderID('T','H','F','M')		// 0x4D464854
#define FMT_ID			mmioHeaderID('f','m','t',' ')		// 0x20746D66
#define DATA_ID			mmioHeaderID('d','a','t','a')		// 0x61746164

typedef unsigned int ID;	/* a four character code */

typedef struct _FormatChunk {
	ID chunkID;						/* chunk ID */
	int chunkSize;					/* chunk Size */
	unsigned short wChannels;		/* number of channels (Mono = 1, Stereo = 2, etc.) */
	unsigned short wBlockAlign;		/* block size of data (wChannels*1byte) */
	unsigned int dwMagnitude;		/* max magnitude */
	unsigned int dwDuration;		/* duration */
} GCC_PACK FormatChunk;

typedef struct _DataChunk {
	ID chunkID;
	int chunkSize;
	unsigned char pData[];
} GCC_PACK DataChunk;

typedef struct _HapticFile {
	ID chunkID;			/* chunk ID */
	int chunkSize;		/* chunk Size */
	FormatChunk fmt;	/* Format chunk */
	DataChunk data;		/* Data chunk */
} GCC_PACK HapticFile;

typedef struct _HapticElement {
	int duration;
	int level;
} HapticElement;

int GetHapticLevelMax(int *max);

int InitializeBuffer(unsigned char *vibe_buffer, int max_bufsize);
int InsertElement(unsigned char *vibe_buffer, int max_bufsize, HapticElement *element);
int GetBufferSize(const unsigned char *vibe_buffer, int *size);
int GetBufferDuration(const unsigned char *vibe_buffer, int *duration);
int PlayOneshot(int handle, int duration, int level);
int PlayBuffer(int handle, const unsigned char *vibe_buffer, int iteration, int level);
int Stop(int handle);
int OpenDevice(int handle);
int CloseDevice(int handle);
int GetState(int handle, int *state);

#endif // __FIEL_H__
