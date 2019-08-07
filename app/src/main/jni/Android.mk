LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := android_salvini
LOCAL_SRC_FILES := android-salvini.c \
    gstrtta.cpp audio_stream.cpp \
		tartini/analysisdata.cpp tartini/bspline.cpp \
		tartini/channel.cpp tartini/conversions.cpp \
		tartini/equalloudness.cpp tartini/fast_smooth.cpp \
		tartini/FastSmoothedAveragingFilter.cpp \
		tartini/FixedAveragingFilter.cpp tartini/gdata.cpp \
		tartini/GrowingAveragingFilter.cpp tartini/IIR_Filter.cpp \
		tartini/musicnotes.cpp tartini/myalgo.cpp \
		tartini/myio.cpp tartini/mymatrix.cpp \
		tartini/mystring.cpp tartini/mytransforms.cpp \
		tartini/notedata.cpp tartini/notesynth.cpp \
		tartini/prony.cpp tartini/settings.cpp \
		tartini/soundfile.cpp tartini/useful.cpp \
		tartini/sound_stream.cpp \
		tartini/rtta.cpp
LOCAL_SHARED_LIBRARIES := gstreamer_android
LOCAL_C_INCLUDES := tartini
LOCAL_CPPFLAGS := -DGST_PLUGIN_BUILD_STATIC -DVERSION=\"$(GST_PLUGIN_VERSION)\" -DPACKAGE=\"Salvini\"
LOCAL_LDLIBS := -landroid
# LOCAL_LDFLAGS := -Wl,-soname=libandroid_salvini.so
include $(BUILD_SHARED_LIBRARY)

ifndef GSTREAMER_ROOT_ANDROID
$(error GSTREAMER_ROOT_ANDROID is not defined!)
endif

ifeq ($(TARGET_ARCH_ABI),armeabi)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm
else ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/armv7
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm64
else ifeq ($(TARGET_ARCH_ABI),x86)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86
else ifeq ($(TARGET_ARCH_ABI),x86_64)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86_64
else
$(error Target arch ABI not supported: $(TARGET_ARCH_ABI))
endif

GSTREAMER_NDK_BUILD_PATH  := $(GSTREAMER_ROOT)/share/gst-android/ndk-build/
include $(GSTREAMER_NDK_BUILD_PATH)/plugins.mk

GSTREAMER_PLUGINS         := coreelements audioconvert audioresample volume $(GSTREAMER_PLUGINS_SYS)
GSTREAMER_EXTRA_LIBS      := -liconv -lgnutls
G_IO_MODULES              := gnutls
GSTREAMER_EXTRA_DEPS      := glib-2.0 gstreamer-fft-1.0 gstreamer-base-1.0 gstreamer-audio-1.0 gstreamer-tag-1.0
include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer-1.0.mk
