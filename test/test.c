/*
	* haptic-module-tizen
	*
	* Copyright (c) 2011 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
	*
	* Contact: Jae-young Hwang <j-zero.hwang@samsung.com>
	*
	* PROPRIETARY/CONFIDENTIAL
	*
	* This software is the confidential and proprietary information of
	* SAMSUNG ELECTRONICS ("Confidential Information"). You agree and acknowledge
	* that this software is owned by Samsung and you shall not disclose
	* such Confidential Information and shall use it only in accordance with the
	* terms of the license agreement you entered into with SAMSUNG ELECTRONICS.
	* SAMSUNG make no representations or warranties about the suitability
	* of the software, either express or implied, including but not limited
	* to the implied warranties of merchantability, fitness for a particular
	* purpose,
	* by licensee arising out of or releated to this software.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vconf.h>
#include <dlfcn.h>

#include <haptic_plugin_intf.h>

#define HAPTIC_MODULE_PATH          "/usr/lib/libhaptic-module.so"

static void *dlopen_handle;
static const haptic_plugin_interface *plugin_intf;

int main ()
{
	char *error = NULL;

	printf("start!\n");
	dlopen_handle = dlopen(HAPTIC_MODULE_PATH, RTLD_NOW);
	if (!dlopen_handle) {
		printf("fail dlopen\n");
		printf("%s\n", dlerror());
		return -1;
	}

	const haptic_plugin_interface *(*get_haptic_plugin_interface) ();
	get_haptic_plugin_interface = dlsym(dlopen_handle, "get_haptic_plugin_interface");

	if ((error = dlerror()) != NULL) {
		printf("dlsym error\n");
		printf("%s\n", error);
		dlclose(dlopen_handle);
		return -1;
	}

	plugin_intf = get_haptic_plugin_interface();
	if (!plugin_intf) {
		printf("plugin_intf error\n");
		dlclose(dlopen_handle);
		return -1;
	}

	while (1)
	{
		char input = -1;
		int duration = -1;
		int handle = -1;
		int status = -1;

		printf ("============================================\n");
		printf ("haptic_internal_vibrate_monotone : a\n");
		printf ("hatpic_internal_stop_all_effects : b\n");
		printf ("Quit : z\n");
		printf ("============================================\n");

		status = scanf ("%c", &input);

		switch (input)
		{
			case 'a' :
				printf ("Handle : ");
				status = scanf ("%d", &handle);
				if (status < 0)
					return status;

				printf ("Duration : ");
				status = scanf ("%d", &duration);
				if (status < 0)
					return status;

				plugin_intf->haptic_internal_vibrate_monotone(handle, duration, HAPTIC_MODULE_FEEDBACK_MAX, HAPTIC_MODULE_PRIORITY_HIGH, &handle);
				break;

			case 'b':
				printf ("Handle : ");
				status = scanf ("%d", &handle);
				if (status < 0)
					return status;
				plugin_intf->haptic_internal_stop_all_effects(handle);
				break;

			case 'c':
				break;

			case 'z':
				return 0;
		}
	}

	if (dlopen_handle) {
		dlclose(dlopen_handle);
	}

	return 0;
}
