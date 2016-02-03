#ifndef AUDIO_STREAM_H
#define AUDIO_STREAM_H


#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "tartini/sound_stream.h"

class AudioStream : public SoundStream
{
 public:

  AudioStream();
  virtual ~AudioStream();

  bool configure(int rate_, int channels_, int bits_);

  int writeFloats(float **channelData, int length, int ch);
  long read_bytes(void *data, long length);
  int writeReadFloats(float **outChannelData, int outCh, float **inChannelData, int inCh, int length);

  void submitBuffer (GstBuffer *buf);
  int bytesAvail();

  bool processChunk();
 private:
  GstAdapter *adapter;
};

#endif
