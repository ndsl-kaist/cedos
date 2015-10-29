# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)/../jni

include $(CLEAR_VARS)

#LOCAL_STATIC_LIBRARIES := LOCAL_WHOLE_STATIC_LIBRARIES

LOCAL_MODULE    := dtn_manager
LOCAL_SRC_FILES := \
    dtp_sockwrapper.c \
	dtp_socket.c \
    dtp_transport.c \
    dtp.c \
	context.c \
	crypt.c \
	dtp_select.c \
	dtp_log.c \
	scheduler.c \
	dtp_mobile.c \
	dtp_retrans_queue.c \
    debug.c \

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_LDLIBS := -levent  -lcrypto-static -L$(SYSROOT)/usr/lig -llog

# NOTE: Do never remove -DNO_FDSET_EXTENSION
LOCAL_CFLAGS := -DCONFIG_EMBDED -DUSE_IND_THREAD -DIN_MOBILE -DNO_FDSET_EXTENSION #-DHAVE_SCHEDULER

#LOCAL_STATIC_LIBRARIES += -L$(LOCAL_PATH)/event/lib/libevent.la 

include $(BUILD_SHARED_LIBRARY)

