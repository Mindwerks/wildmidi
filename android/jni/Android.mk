LOCAL_PATH := $(call my-dir)/../..
include $(CLEAR_VARS)

LOCAL_MODULE     := WildMidi
LOCAL_C_INCLUDES := $(LOCAL_PATH)/android/jni $(LOCAL_PATH)/include
LOCAL_ARM_MODE   := arm
LOCAL_CFLAGS     += -DWILDMIDI_BUILD
LOCAL_CFLAGS     += -fvisibility=hidden -DSYM_VISIBILITY

LOCAL_SRC_FILES := \
	src/f_hmi.c \
	src/f_hmp.c \
	src/f_midi.c \
	src/f_mus.c \
	src/f_xmidi.c \
	src/file_io.c \
	src/gus_pat.c \
	src/internal_midi.c \
	src/lock.c \
	src/mus2mid.c \
	src/patches.c \
	src/reverb.c \
	src/sample.c \
	src/wildmidi_lib.c \
	src/wm_error.c \
	src/xmi2mid.c

include $(BUILD_SHARED_LIBRARY)
