#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "audio_stream.h"

GST_DEBUG_CATEGORY_EXTERN (gst_rtta_debug);
#define GST_CAT_DEFAULT gst_rtta_debug

AudioStream::AudioStream()
{
    adapter = gst_adapter_new();
}

AudioStream::~AudioStream()
{
  g_object_unref (G_OBJECT (adapter));
}

bool
AudioStream::configure(int rate_, int channels_, int bits_)
{
  freq = rate_;
  channels = channels_;
  bits = bits_;

  return true;
}

long
AudioStream::read_bytes (void *data, long length)
{ 
  gssize len = gst_adapter_available (adapter);

  if (len < length)
    length = len;

  GST_LOG ("Reading %d bytes from adapter", (int) length);
  gst_adapter_copy (adapter, data, 0, length);
  gst_adapter_flush (adapter, length);

  return length;
}

int
AudioStream::writeFloats(float **channelData, int length, int ch)
{
  GST_WARNING ("AudioStream::writeFloats not implemented");
  return -1;
}

int AudioStream::writeReadFloats(float **outChannelData, int outCh, float **inChannelData, int inCh, int length)
{
  GST_WARNING ("AudioStream::writeReadFloats not implemented");
  return -1;
}

void
AudioStream::submitBuffer (GstBuffer *buf)
{
  gst_adapter_push (adapter, buf);
}

int
AudioStream::bytesAvail()
{
  return gst_adapter_available (adapter);
}

