//
//  Thu Jun 26 21:20:04 2008 -- Scott R. Turner
//  
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//

/*
  Note:  calculation of "clarity":

      AnalysisData *data = ch->dataAtChunk(intChunk);
      err = data->correlation();
      vol = dB2Normalised(data->logrms(), ch->rmsCeiling, ch->rmsFloor);

      clarity = err * vol;
      
*/      
#include <math.h>
#include <stdio.h>
#include <glib.h>

#include "gdata.h"
#include "rtta.h"

//
//  C1 = 23.5
//  B8 = 118.5

const char* scale_names[12] = {
  "C",
  "C#",
  "D",
  "D#",
  "E",
  "F",
  "F#",
  "G",
  "G#",
  "A",
  "A#",
  "B"};

const float C1_semitones = 23.5;

RTTAdata::RTTAdata(GlobalData *gdata_)
{
  int next_note = 0;

  gdata = gdata_;
  use_window = false;
  window_size = 0;

  for(int i=0;i<RTTA_OCTAVE_RANGE;i++) {
    for (int j=0;j<12;j++) {
      rttaInfo[next_note++].note_name = NULL;
    }
  }

  reset();
};

RTTAdata::RTTAdata(GlobalData *gdata_, bool win, int win_size)
{
  int next_note = 0;

  gdata = gdata_;
  use_window = win;
  window_size = win_size;

  for(int i=0;i<RTTA_OCTAVE_RANGE;i++) {
    for (int j=0;j<12;j++) {
      rttaInfo[next_note++].note_name = NULL;
    }
  }

  reset();
};

RTTAdata::~RTTAdata()
{
  int next_note = 0;

  for(int i=0;i<RTTA_OCTAVE_RANGE;i++) {
    for (int j=0;j<12;j++) {
      if (rttaInfo[next_note].note_name)
        delete[] rttaInfo[next_note].note_name;
      next_note++;
    }
  }
}

void RTTAdata::reset(bool win, int win_size)
{
  use_window = win;
  window_size = win_size;
  reset();
};

void RTTAdata::reset()
{
  int basePitchA;
  float baseShift = 0.0;
  int next_note = 0;

  basePitchA = gdata->getBasePitch();
  if (basePitchA != 440) {
    baseShift = log2(basePitchA/440.0)*12.0;
  };

  for(int i=0;i<RTTA_OCTAVE_RANGE;i++) {
    for (int j=0;j<12;j++) {
      //
      //  Potentially have to delete the old string name.
      //
      if (rttaInfo[next_note].note_name)
	      delete [] rttaInfo[next_note].note_name;
      char *tmp = new char[4];
      tmp[0] = tmp[1] = tmp[2] = tmp[3] = 0;
      (void) sprintf(tmp, "%2s%1d", scale_names[j],i+1);
      rttaInfo[next_note].note_name = tmp;
      rttaInfo[next_note].start_val = C1_semitones +
	next_note +
	baseShift;  // Correction if we don't have A440
      //
      //  Correct for temperament
      //
#if 0
      switch (j) {
      case 0:
	rttaInfo[next_note].start_val +=
	  gdata->qsettings->value("RTTA/customIntonationC",0).toInt()/100.0;
	break;
      case 1:
	rttaInfo[next_note].start_val +=
	  gdata->qsettings->value("RTTA/customIntonationCS",0).toInt()/100.0;
	break;
      case 2:
	rttaInfo[next_note].start_val +=
	  gdata->qsettings->value("RTTA/customIntonationD",0).toInt()/100.0;
	break;
      case 3:
	rttaInfo[next_note].start_val +=
	  gdata->qsettings->value("RTTA/customIntonationDS",0).toInt()/100.0;
	break;
      case 4:
	rttaInfo[next_note].start_val +=
	  gdata->qsettings->value("RTTA/customIntonationE",0).toInt()/100.0;
	break;
      case 5:
	rttaInfo[next_note].start_val +=
	  gdata->qsettings->value("RTTA/customIntonationF",0).toInt()/100.0;
	break;
      case 6:
	rttaInfo[next_note].start_val +=
	  gdata->qsettings->value("RTTA/customIntonationFS",0).toInt()/100.0;
	break;
      case 7:
	rttaInfo[next_note].start_val +=
	  gdata->qsettings->value("RTTA/customIntonationG",0).toInt()/100.0;
	break;
      case 8:
	rttaInfo[next_note].start_val +=
	  gdata->qsettings->value("RTTA/customIntonationGS",0).toInt()/100.0;
	break;
      case 9:
	rttaInfo[next_note].start_val +=
	  gdata->qsettings->value("RTTA/customIntonationA",0).toInt()/100.0;
	break;
      case 10:
	rttaInfo[next_note].start_val +=
	  gdata->qsettings->value("RTTA/customIntonationAS",0).toInt()/100.0;
	break;
      case 11:
	rttaInfo[next_note].start_val +=
	  gdata->qsettings->value("RTTA/customIntonationB",0).toInt()/100.0;
	break;
      };	
#endif
      rttaInfo[next_note].use_window = use_window;
      rttaInfo[next_note].window_size = window_size;
      rttaInfo[next_note].num_samples = 0;
      rttaInfo[next_note].total = 0.0;
      next_note++;
    };
  };
  total_samples = 0;
};

void RTTAdata::addInfo(float pitch)
{
  for(int j=0; j<RTTA_NOTE_RANGE; j++) {
    if (pitch >= rttaInfo[j].start_val &&
	pitch <= rttaInfo[j].start_val + 1.0) {
      //
      //  If we're not using a window we don't need to bother
      //  with the samples buffer and just keep a running total.
      //
      if (!rttaInfo[j].use_window) {
	total_samples++;
	rttaInfo[j].num_samples++;
	rttaInfo[j].total += pitch;
      } else {
	//
	//  If we have a window we have to check to see if the samples
	//  buffer has filled up.  If it has we have to pop the oldest
	//  sample off and remove it from the running total before adding
	//  the latest sample.
	//
	total_samples++;
	if (rttaInfo[j].samples.size() == rttaInfo[j].samples.capacity()) {
	  float last_val = 0.0;
	  rttaInfo[j].samples.get(&last_val);
	  rttaInfo[j].total -= last_val;
	  rttaInfo[j].num_samples--;
	  total_samples--;
	};
	rttaInfo[j].num_samples++;
	rttaInfo[j].total += pitch;
	rttaInfo[j].samples.put(pitch);
      };
    };
  };
};
