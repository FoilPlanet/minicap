LOCAL_PATH := $(call my-dir)

USE_MPP := 1

#
# minicap-common
#
include $(CLEAR_VARS)
LOCAL_MODULE := minicap-common

LOCAL_CPPFLAGS  += -fexceptions
LOCAL_SRC_FILES :=   \
    minicap.cpp      \
    SimpleServer.cpp \

LOCAL_SHARED_LIBRARIES := \
    minicap-shared \

ifeq ($(USE_MPP),1)
 LOCAL_CFLAGS += -DUSE_MPP=1
 LOCAL_STATIC_LIBRARIES += mpp-wrapper jpeg-turbo
 LOCAL_SHARED_LIBRARIES += libmpp
else
 LOCAL_SRC_FILES += JpgEncoder.cpp
 LOCAL_STATIC_LIBRARIES += jpeg-turbo
endif

include $(BUILD_STATIC_LIBRARY)

#
# minicap
# Enable PIE manually. Will get reset on $(CLEAR_VARS).
#
include $(CLEAR_VARS)
LOCAL_MODULE  := minicap
LOCAL_CFLAGS  += -fPIE
LOCAL_LDFLAGS += -fPIE -pie
LOCAL_STATIC_LIBRARIES += minicap-common
LOCAL_SHARED_LIBRARIES += minicap
include $(BUILD_EXECUTABLE)

#
# minicap-nopie
#
include $(CLEAR_VARS)
LOCAL_MODULE := minicap-nopie
LOCAL_STATIC_LIBRARIES += minicap-common
LOCAL_SHARED_LIBRARIES += minicap
include $(BUILD_EXECUTABLE)
