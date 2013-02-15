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


#ifndef __HAPTIC_MODULE_LOG_H__
#define __HAPTIC_MODULE_LOG_H__

#define FEATURE_HAPTIC_MODULE_DLOG

#ifdef FEATURE_HAPTIC_MODULE_DLOG
    #define LOG_TAG "HAPTIC_MODULE"
    #include <dlog.h>
    #define MODULE_LOG(fmt, args...)       SLOGD(fmt, ##args)
    #define MODULE_ERROR(fmt, args...)     SLOGE(fmt, ##args)
#else
    #define MODULE_LOG(x, ...)
    #define MODULE_ERROR(x, ...)
#endif

#endif //__HAPTIC_MODULE_LOG_H__
