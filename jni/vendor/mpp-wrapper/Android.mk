LOCAL_PATH := $(call my-dir)

# For building in AOSP
ifeq ($(TARGET_ARCH_ABI),)
TARGET_ARCH_ABI := $(TARGET_ARCH_VARIANT)
endif

#
# libmpp
#
include $(CLEAR_VARS)
LOCAL_MODULE := libmpp
LOCAL_SRC_FILES := $(LOCAL_PATH)/libs/$(TARGET_ARCH_ABI)/libmpp.so
include $(PREBUILT_SHARED_LIBRARY)

#
# libvpu (mpp/legacy)
#
include $(CLEAR_VARS)
LOCAL_MODULE := libvpu
LOCAL_SRC_FILES := $(LOCAL_PATH)/libs/$(TARGET_ARCH_ABI)/libvpu.so
include $(PREBUILT_SHARED_LIBRARY)

#
# libmpp_static
#
include $(CLEAR_VARS)
LOCAL_MODULE := libmpp_static
LOCAL_SRC_FILES := $(LOCAL_PATH)/libs/$(TARGET_ARCH_ABI)/libmpp_static.a
include $(PREBUILT_STATIC_LIBRARY)

#
# mpp-wrapper
#
include $(CLEAR_VARS)

LOCAL_MODULE := mpp-wrapper

LOCAL_CFLAGS += -DANBOX=1

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/inc         \
    $(LOCAL_PATH)/inc/osal    \
    
LOCAL_SRC_FILES += \
    src/MppEncoder.cc        \
    src/MppWrapper.cc        \

LOCAL_STATIC_LIBRARIES += minicap-common
# LOCAL_STATIC_LIBRARIES += libmpp_static
# LOCAL_SHARED_LIBRARIES += libmpp

# ifneq ($(USE_LIBJPEG_TURBO),)
LOCAL_STATIC_LIBRARIES += jpeg-turbo
# endif

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/src

include $(BUILD_STATIC_LIBRARY)
