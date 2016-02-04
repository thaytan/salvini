/*
 * GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) 2015 Jan Schmidt <thaytan@noraisin.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-rtta
 *
 * FIXME:Describe rtta here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m audiotestsrc ! rtta ! autoaudiosink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiofilter.h>
#include <string.h>

#include "tartini/gdata.h"
#include "tartini/soundfile.h"
#include "audio_stream.h"

GST_DEBUG_CATEGORY (gst_rtta_debug);
#define GST_CAT_DEFAULT gst_rtta_debug

typedef struct _GstRtta GstRtta;
typedef struct _GstRttaClass GstRttaClass;

#define GST_TYPE_RTTA \
  (gst_rtta_get_type())
#define GST_RTTA(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTTA,GstRtta))
#define GST_RTTA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTTA,GstRttaClass))
#define GST_IS_RTTA(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTTA))
#define GST_IS_RTTA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTTA))

struct _GstRtta
{
  GstAudioFilter audiofilter;

  Settings *settings;
  GlobalData *gdata;
  AudioStream *stream;
  SoundFile *sound_file;

  gboolean post_messages;

  int bpf;
  int rate;
  int last_rtta_samples;
  GstClockTime timestamp;
};

struct _GstRttaClass
{
  GstAudioFilterClass parent_class;
};

#define DEFAULT_POST_MESSAGES         TRUE
enum
{
  PROP_0,
  PROP_POST_MESSAGES
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_rtta_debug, "rtta", 0, "Template rtta");

G_DEFINE_TYPE_WITH_CODE (GstRtta, gst_rtta, GST_TYPE_AUDIO_FILTER, DEBUG_INIT);

G_BEGIN_DECLS static void gst_rtta_finalize (GObject * gobject);
static gboolean gst_rtta_filter_start (GstBaseTransform * base_transform);
static gboolean gst_rtta_filter_stop (GstBaseTransform * base_transform);

static void gst_rtta_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_rtta_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_rtta_setup (GstAudioFilter * filter,
    const GstAudioInfo * info);
static GstFlowReturn
gst_rtta_filter_inplace (GstBaseTransform * base_transform, GstBuffer * buf);

G_END_DECLS
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define ALLOWED_CAPS \
  GST_AUDIO_CAPS_MAKE ("{ F32LE, F64LE, S8, S16LE, S24LE, S32LE }") \
      ", layout = (string) interleaved"
#else
#define ALLOWED_CAPS \
  GST_AUDIO_CAPS_MAKE ("{ F32BE, F64BE, S8, S16BE, S24BE, S32BE }") \
      "layout = (string) interleaved"
#endif
    static void
gst_rtta_class_init (GstRttaClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) (klass);
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *) klass;
  GstAudioFilterClass *audio_filter_class = (GstAudioFilterClass *) klass;
  GstCaps *caps;

  gobject_class->finalize = gst_rtta_finalize;
  gobject_class->set_property = gst_rtta_set_property;
  gobject_class->get_property = gst_rtta_get_property;

  g_object_class_install_property (gobject_class, PROP_POST_MESSAGES,
      g_param_spec_boolean ("post-messages", "Post Messages",
          "Whether to post an element message on the bus for each "
          "new note detection", DEFAULT_POST_MESSAGES,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class, "RTTA",
      "Filter/Analyzer/Audio",
      "Real-Time Tuning Analysis element based on Tartini",
      "Jan Schmidt <jan@centricular.com>");

  caps = gst_caps_from_string (ALLOWED_CAPS);
  gst_audio_filter_class_add_pad_templates (audio_filter_class, caps);
  gst_caps_unref (caps);

  audio_filter_class->setup = gst_rtta_setup;

  btrans_class->start = gst_rtta_filter_start;
  btrans_class->stop = gst_rtta_filter_stop;

  btrans_class->transform_ip = gst_rtta_filter_inplace;
}

static void
gst_rtta_init (GstRtta * rtta)
{
  GST_DEBUG_OBJECT (rtta, "init");

  rtta->settings = new Settings;
  rtta->gdata = new GlobalData (rtta->settings);
  rtta->stream = new AudioStream;

  rtta->gdata->setStream (rtta->stream);

  rtta->post_messages = DEFAULT_POST_MESSAGES;
}

static void
gst_rtta_finalize (GObject * gobject)
{
  GstRtta *rtta = GST_RTTA (gobject);

  if (rtta->sound_file)
    delete rtta->sound_file;

  delete rtta->stream;
  delete rtta->gdata;
  delete rtta->settings;

  G_OBJECT_CLASS (gst_rtta_parent_class)->finalize (gobject);
}

static gboolean
gst_rtta_filter_start (GstBaseTransform * btrans)
{
  GstRtta *rtta = GST_RTTA (btrans);

  rtta->last_rtta_samples = 0;
  rtta->gdata->rtta_data->reset ();
  rtta->sound_file = new SoundFile (rtta->gdata);
  rtta->timestamp = 0;

  if (GST_BASE_TRANSFORM_CLASS (gst_rtta_parent_class)->start)
    return GST_BASE_TRANSFORM_CLASS (gst_rtta_parent_class)->start (btrans);

  return TRUE;
}

static gboolean
gst_rtta_filter_stop (GstBaseTransform * btrans)
{
  GstRtta *rtta = GST_RTTA (btrans);

  if (rtta->sound_file) {
    delete rtta->sound_file;
    rtta->sound_file = NULL;
  }

  if (GST_BASE_TRANSFORM_CLASS (gst_rtta_parent_class)->stop)
    return GST_BASE_TRANSFORM_CLASS (gst_rtta_parent_class)->stop (btrans);
  return TRUE;
}

static void
gst_rtta_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtta *filter = GST_RTTA (object);

  switch (prop_id) {
    case PROP_POST_MESSAGES:
      filter->post_messages = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtta_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtta *filter = GST_RTTA (object);

  switch (prop_id) {
    case PROP_POST_MESSAGES:
      g_value_set_boolean (value, filter->post_messages);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_rtta_setup (GstAudioFilter * filter, const GstAudioInfo * info)
{
  GstRtta *rtta = GST_RTTA (filter);
  GlobalData *gdata = rtta->gdata;
  SoundFile *soundfile = rtta->sound_file;

  soundfile = rtta->sound_file;

  rtta->bpf = info->channels * GST_AUDIO_INFO_WIDTH (info);
  rtta->rate = info->rate;

  rtta->stream->configure (info->rate, info->channels,
      GST_AUDIO_INFO_WIDTH (info));

  if (!soundfile->configure (info->rate, info->channels,
          GST_AUDIO_INFO_DEPTH (info))) {
    GST_ERROR_OBJECT (filter, "Failed to configure analyser");
    return FALSE;
  }

  gdata->setActiveChannel (soundfile->channels (0));

  return TRUE;
}

static GstMessage *
gst_rtta_message_new (GstRtta * rtta, GValue * note_list)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM_CAST (rtta);
  GstStructure *s;
  GstClockTime running_time, stream_time;

  running_time = gst_segment_to_running_time (&trans->segment,
      GST_FORMAT_TIME, rtta->timestamp);
  stream_time = gst_segment_to_stream_time (&trans->segment,
      GST_FORMAT_TIME, rtta->timestamp);

  s = gst_structure_new ("rtta",
      "timestamp", G_TYPE_UINT64, rtta->timestamp,
      "stream-time", G_TYPE_UINT64, stream_time,
      "running-time", G_TYPE_UINT64, running_time, NULL);
  gst_structure_set_value (s, "summary", note_list);

  return gst_message_new_element (GST_OBJECT (rtta), s);
}

static void
post_message (GstRtta * rtta)
{
  GlobalData *gdata = rtta->gdata;
  RTTAinfo *rInfo = gdata->rtta_data->rttaInfo;
  int total_samples = gdata->rtta_data->total_samples;

  if (total_samples > 0 && total_samples > rtta->last_rtta_samples) {
    int cutoff = (int) total_samples * gdata->getSamplesThreshold ();
    GstMessage *msg;
    GValue note_list = G_VALUE_INIT;

    g_value_init (&note_list, GST_TYPE_LIST);

    for (int i = 0; i < RTTA_NOTE_RANGE; i++) {
      if (rInfo[i].num_samples > cutoff) {
        GstStructure *s;
        GValue note_entry = G_VALUE_INIT;
        int cents;

        cents =
            (int) (((rInfo[i].total / rInfo[i].num_samples) -
                rInfo[i].start_val - 0.50) * 100);

        GST_LOG_OBJECT (rtta, "note %5s: samples %8ld cents %8d",
            rInfo[i].note_name, rInfo[i].num_samples, cents);

        s = gst_structure_new ("note",
            "name", G_TYPE_STRING, rInfo[i].note_name,
            "samples", G_TYPE_INT, (int) rInfo[i].num_samples,
            "cents", G_TYPE_INT, (int) cents, NULL);

        g_value_init (&note_entry, GST_TYPE_STRUCTURE);
        g_value_take_boxed (&note_entry, s);
        gst_value_list_append_value (&note_list, &note_entry);
        g_value_unset (&note_entry);
      }

    }

    msg = gst_rtta_message_new (rtta, &note_list);
    g_value_unset (&note_list);
    gst_element_post_message (GST_ELEMENT (rtta), msg);

    rtta->last_rtta_samples = total_samples;
  }
}

static GstFlowReturn
gst_rtta_filter_inplace (GstBaseTransform * base_transform, GstBuffer * buf)
{
  GstRtta *rtta = GST_RTTA (base_transform);
  GstFlowReturn ret = GST_FLOW_OK;
  gint samplesPerChunk;
  SoundFile *soundfile = rtta->sound_file;
  guint samples_handled = 0;
  GstClockTime basetime;

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_PTS (buf)))
    rtta->timestamp = GST_BUFFER_PTS (buf);
  basetime = rtta->timestamp;

  rtta->stream->submitBuffer (gst_buffer_ref (buf));

  samplesPerChunk = soundfile->framesPerChunk ();

  rtta->gdata->setDoingActiveAnalysis (true);
  while (rtta->stream->bytesAvail () / rtta->bpf >= samplesPerChunk) {
    soundfile->recordChunk (samplesPerChunk);
    samples_handled += samplesPerChunk;
    rtta->timestamp =
        basetime + gst_util_uint64_scale (samples_handled, GST_SECOND,
        rtta->rate);
    if (rtta->post_messages)
      post_message (rtta);
  }
  rtta->gdata->setDoingActiveAnalysis (false);


  return ret;
}

static gboolean
rtta_init (GstPlugin * rtta)
{
  return gst_element_register (rtta, "rtta", GST_RANK_NONE, GST_TYPE_RTTA);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    rtta,
    "Real-time Tuning Analysis plugin",
    rtta_init, VERSION, "LGPL", "GStreamer", "http://gstreamer.net/");
