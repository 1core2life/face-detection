LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)


LOCAL_MODULE    := main
LOCAL_SRC_FILES := main.cpp
LOCAL_LDLIBS += -llog -ljnigraphics -lz -lm

include $(BUILD_SHARED_LIBRARY)