/*
 * haptic-module-tizen
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdarg.h>
#include <errno.h>

#include "haptic_module_log.h"

#define HAPTIC_RETRY_READ_COUNT			5
#define HAPTIC_PARAM_CNT				3
#define HAPTIC_MAXSTR					100
#define HAPTIC_MAXARG					16
#define PREDEF_HAPTIC					"haptic"
#define SYSNOTI_SOCKET_PATH				"/tmp/sn"

enum sysnoti_cmd {
	ADD_HAPTIC_ACTION,
	CALL_HAPTIC_ACTION,
};

struct sysnoti_type {
	int pid;
	int cmd;
	char *type;
	char *path;
	int argc;
	char *argv[HAPTIC_MAXARG];
};

static inline int __send_int(int fd, int val)
{
	return write(fd, &val, sizeof(int));
}

static inline int __send_str(int fd, char *str)
{
	int len;
	int r;

	if (str == NULL) {
		len = 0;
		r = write(fd, &len, sizeof(int));
	} else {
		len = strlen(str);
		if (len > HAPTIC_MAXSTR)
			len = HAPTIC_MAXSTR;
		r = write(fd, &len, sizeof(int));
		r = write(fd, str, len);
	}

	return r;
}

static int __sysnoti_send(struct sysnoti_type *msg)
{
	int sockfd;
	int len;
	struct sockaddr_un addr;
	int retry_cnt;
	int ret;
	int i;
	int r;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd == -1) {
		MODULE_ERROR("socket create failed");
		return -1;
	}

	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SYSNOTI_SOCKET_PATH, sizeof(addr.sun_path) - 1);
	len = sizeof(addr);

	if (connect(sockfd, (struct sockaddr *)&addr, len) < 0) {
		MODULE_ERROR("connect failed");
		close(sockfd);
		return -1;
	}

	__send_int(sockfd, msg->pid);
	__send_int(sockfd, msg->cmd);
	__send_str(sockfd, msg->type);
	__send_str(sockfd, msg->path);
	__send_int(sockfd, msg->argc);
	for (i = 0; i < msg->argc; i++)
		__send_str(sockfd, msg->argv[i]);

	retry_cnt = 0;
	while ((r = read(sockfd, &ret, sizeof(int))) < 0) {

		if (errno != EINTR) {
			MODULE_LOG("read fail : %s(%d)", strerror(errno), errno);
			ret = -1;
			break;
		}

		if (retry_cnt == HAPTIC_RETRY_READ_COUNT) {
			MODULE_ERROR("retry(%d) fail", retry_cnt);
			ret = -1;
			break;
		}

		MODULE_ERROR("Re-read for error(EINTR)");
		++retry_cnt;
	}

	close(sockfd);
	return ret;
}

static int __haptic_call_predef_action(const char *type, int num, ...)
{
	struct sysnoti_type msg;
	char *args;
	va_list argptr;
	int i;

	if (type == NULL || num > HAPTIC_MAXARG) {
		errno = EINVAL;
		return -1;
	}

	msg.pid = getpid();
	msg.cmd = CALL_HAPTIC_ACTION;
	msg.type = (char *)type;
	msg.path = NULL;
	msg.argc = num;

	va_start(argptr, num);
	for (i = 0; i < num; i++) {
		args = va_arg(argptr, char *);
		msg.argv[i] = args;
	}
	va_end(argptr);

	return __sysnoti_send(&msg);
}

void __haptic_predefine_action(int prop, int val)
{
	char pid_buf[255];
	char prop_buf[255];
	char val_buf[255];

	snprintf(pid_buf, sizeof(pid_buf), "%d", getpid());
	snprintf(prop_buf, sizeof(prop_buf), "%d", prop);
	snprintf(val_buf, sizeof(val_buf), "%d", val);

	MODULE_LOG("pid : %d, type : %d, val : %d", getpid(), prop, val);
	__haptic_call_predef_action(PREDEF_HAPTIC, HAPTIC_PARAM_CNT, pid_buf, prop_buf, val_buf);
}
