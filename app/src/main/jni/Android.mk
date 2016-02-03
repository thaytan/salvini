LOCAL_PATH := $(call my-dir)
APP_PLATFORM := android-16

include $(CLEAR_VARS)

ifndef GSTREAMER_ROOT
ifndef GSTREAMER_SDK_ROOT_ANDROID
$(error GSTREAMER_SDK_ROOT_ANDROID is not defined!)
endif
GSTREAMER_ROOT        := $(GSTREAMER_SDK_ROOT_ANDROID)
endif

LOCAL_MODULE    := android-salvini
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
LOCAL_CPPFLAGS := -DGST_PLUGIN_BUILD_STATIC -DVERSION=$(GST_PLUGIN_VERSION) -DPACKAGE=\"Salvini\"
LOCAL_LDLIBS := -landroid
LOCAL_LDFLAGS := -Wl,-soname=libandroid-salvini.so
include $(BUILD_SHARED_LIBRARY)

GSTREAMER_NDK_BUILD_PATH  := $(GSTREAMER_ROOT)/share/gst-android/ndk-build/
include $(GSTREAMER_NDK_BUILD_PATH)/plugins.mk
GSTREAMER_PLUGINS         := coreelements audioconvert audioresample gio typefindfunctions videoscale volume autodetect videofilter $(GSTREAMER_PLUGINS_CODECS) $(GSTREAMER_PLUGINS_SYS) $(GSTREAMER_PLUGINS_PLAYBACK)
G_IO_MODULES              := gnutls
GSTREAMER_EXTRA_DEPS      := glib-2.0 gstreamer-fft-1.0
include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer-1.0.mk
