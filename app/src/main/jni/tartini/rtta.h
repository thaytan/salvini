//
//  RTTA.h
//  Thu Jun 26 14:31:14 2008 -- Scott R. Turner
//
//  RTTA support.
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
#ifndef RTTA_H
#define RTTA_H
#include "RingBuffer.h"

const int RTTA_OCTAVE_RANGE = 8;
const int RTTA_NOTE_RANGE = RTTA_OCTAVE_RANGE*12;

class GlobalData;

struct RTTAinfo {
  const char* note_name;
  float start_val;
  long int num_samples;
  float total;
  bool use_window;
  int window_size;
  RingBuffer<float> samples;
};

class RTTAdata
{
 private:
  bool use_window;
  int window_size;
  GlobalData *gdata;
 public:
  //  We do this staticly because I'm too lazy to do the right thing :-)
  RTTAinfo rttaInfo[RTTA_OCTAVE_RANGE*12];

  RTTAdata(GlobalData *gdata_);
  RTTAdata(GlobalData *gdata_, bool win, int win_s);
  ~RTTAdata();

  // Reset the info
  void reset(bool win, int win_size);
  void reset();
  // Add something to the info
  void addInfo(float val);
  long int total_samples;
};

#endif
