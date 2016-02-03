/***************************************************************************
                          gdata.h  -  
                             -------------------
    begin                : 2003
    copyright            : (C) 2003-2005 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/
#ifndef GDATA_H
#define GDATA_H

#define TARTINI_NAME_STR      "Tartini1.2"
#define TARTINI_VERSION_STR   "1.2.0"
//#define TARTINI_VERSION       1.2

#include <vector>
//#include "sound_file_stream.h"
//#include "audio_stream.h"
//#include "audio_thread.h"
//#include "chirp_xform.h"
#include "settings.h"

#ifndef WINDOWS
#include <sys/time.h>
#endif
#include "array2d.h"
#include "useful.h"
#include "analysisdata.h"

#include "rtta.h"
extern int gMusicKey;

#ifndef WINDOWS
//for multi-threaded profiling
extern struct itimerval profiler_value;
extern struct itimerval profiler_ovalue;
#endif

#define STREAM_STOP     0
#define STREAM_FORWARD  1
#define STREAM_PAUSE    2
#define STREAM_UPDATE   3

#define SOUND_PLAY      0x01
#define SOUND_REC       0x02
#define SOUND_PLAY_REC  0x03

#define NUM_WIN_SIZES 5
extern int frame_window_sizes[NUM_WIN_SIZES];
extern const char *frame_window_strings[NUM_WIN_SIZES];

#define NUM_STEP_SIZES 6
extern float step_sizes[NUM_STEP_SIZES];
extern const char *step_size_strings[NUM_STEP_SIZES];

#define NUM_PITCH_METHODS 8
extern const char *pitch_method_strings[NUM_PITCH_METHODS];

#define NUM_INTERPOLATING_TYPES 3
extern const char *interpolating_type_strings[NUM_INTERPOLATING_TYPES];

class FBuffer;
class SoundStream;
class SoundFileStream;
class AudioStream;
class FWinFunc;
class Filter;
class SoundFile;
class Channel;

class GlobalData
{
public:
  enum SavingModes { ALWAYS_ASK, NEVER_SAVE, ALWAYS_SAVE };

  GlobalData(Settings *settings_);
  virtual ~GlobalData();

  int soundMode;

  AudioStream *audio_stream;
  RTTAdata *rtta_data;

  bool need_update;

  std::vector<Filter*> filter_hp; //highpass filter
  std::vector<Filter*> filter_lp; //lowpass filter

  //bool equalLoudness;
  //bool useMasking;
  //bool useRidgeFile; //output a file with ridges found
  double cur_note;
  float peakThreshold;
  float correlationThreshold;

  //char *inputFile;

  //int in_channels;
  //int process_channels;
  //int out_channels;
  //int pitch_method[2]; //a pitch method for each channel
  int interpolating_type;
  int bisection_steps;
  int fast_correlation_repeats;
  bool using_coefficients_table;
  //chirp_xform fct;QColor myLineColor1(32, 32, 32);

  //float *fct_in_data;
  //float *fct_out_data;
  //FrameRGB *fct_draw_data;

  std::vector<SoundFile*> soundFiles;
  std::vector<Channel*> channels;

  void setActiveChannel(Channel *toActive);
  Channel* getActiveChannel() { return activeChannel; }
  SoundFile* getActiveSoundFile();

  Settings *getSettings() { return settings; }

private:
  Settings  *settings;

  Channel *activeChannel; /**< Pointer to the active channel */ 
  bool _doingHarmonicAnalysis;
  bool _doingFreqAnalysis;
  bool _doingEqualLoudness;
  bool _doingAutoNoiseFloor;
  int _doingActiveAnalysis;
  int _doingActiveFFT;
  int _doingActiveCepstrum;
  bool _doingDetailedPitch;
  int _fastUpdateSpeed;
  int _slowUpdateSpeed;
  bool _polish;
  bool _showMeanVarianceBars;
  int _savingMode;
  bool _vibratoSineStyle;
  //int _musicKey;
  int _musicKeyType;
  int _temperedType;
  bool _mouseWheelZooms;
  double _freqA;
  double _semitoneOffset;

  //double _noiseThreshold;
  //double _noiseThresholdDB;
  //double _changenessThreshold;
  int _amplitudeMode;
  int _pitchContourMode;
  AnalysisMode _analysisMode;

public:
  //void      setLeftTime(double x); /**< Allows you to specify the leftmost time a file starts */
  //void      setRightTime(double x); /**< Allows you to specify the rightmost time a file starts */
  //void      setTopPitch(double y); /**< Allows you to specify the top note pitch the programme should allow */
  double    topPitch() { return settings->topPitch(); }
  void      setDoingActiveAnalysis(bool x) { _doingActiveAnalysis += (x) ? 1 : -1; }
  void      setDoingActiveFFT(bool x) { _doingActiveFFT += (x) ? 1 : -1; }
  void      setDoingActiveCepstrum(bool x) { _doingActiveCepstrum += (x) ? 1 : -1; setDoingActiveFFT(x); }
  void      setDoingDetailedPitch(bool x) { _doingDetailedPitch = x; }

  int       getAnalysisBufferSize(int rate);
  int       getAnalysisStepSize(int rate);
  bool      doingHarmonicAnalysis() { return _doingHarmonicAnalysis; }
  bool      doingAutoNoiseFloor() { return _doingAutoNoiseFloor; }
  bool      doingEqualLoudness() { return _doingEqualLoudness; }
  bool      doingFreqAnalysis() { return _doingFreqAnalysis; }
  bool      doingActiveAnalysis() { return _doingActiveAnalysis; }
  bool      doingActiveFFT() { return _doingActiveFFT; }
  bool      doingActiveCepstrum() { return _doingActiveCepstrum; }
  bool      doingDetailedPitch() { return _doingDetailedPitch; }
  bool      doingActive() { return (doingActiveAnalysis() || doingActiveFFT() || doingActiveCepstrum()); }
  bool      vibratoSineStyle() { return _vibratoSineStyle; }
  int       amplitudeMode() { return _amplitudeMode; }
  int       pitchContourMode() { return _pitchContourMode; }
  int       fastUpdateSpeed() { return _fastUpdateSpeed; }
  int       slowUpdateSpeed() { return _slowUpdateSpeed; }
  bool      mouseWheelZooms() { return _mouseWheelZooms; }

  //double  noiseThreshold() { return _noiseThreshold; }
  //double  noiseThresholdDB() { return _noiseThresholdDB; }
  //double  changenessThreshold() { return _changenessThreshold; }
  //void    setNoiseThresholdDB(double noiseThresholdDB_);
  //void    setChangenessThreshold(double changenessThreshold_);
  void      setAmpThreshold(int mode, int index, double value);
  double    ampThreshold(int mode, int index);
  void      setAmpWeight(int mode, double value);
  double    ampWeight(int mode);
  AnalysisMode analysisMode() { return _analysisMode; }
  bool      polish() { return _polish; }
  //void    setPolish(bool polish_) { _polish = polish_; }
  bool      showMeanVarianceBars() { return _showMeanVarianceBars; }
  //void    setShowMeanVarianceBars(bool showMeanVarianceBars_) { _showMeanVarianceBars = showMeanVarianceBars_; }
  int       savingMode() { return _savingMode; }

  void      clearFreqLookup();
  void      clearAmplitudeLookup();
  void      recalcScoreThresholds();
  int       getActiveIntThreshold();

  int       getAnalysisThreshold() { return settings->getAnalysisThreshold(); }
  float     getVolumeThreshold() { return settings->getVolumeThreshold(); }
  float     getClarityThreshold() { return settings->getClarityThreshold() / 100.0; }
  int       getYellowLimit() { return settings->getYellowLimit(); }
  int       getRedLimit() { return settings->getRedLimit(); }
  float     getSamplesThreshold() { return settings->getSamplesThreshold() / 100.0; }
  int       getBasePitch() { return settings->getBasePitch(); }

  double    dBFloor() { return settings->dBFloor(); }
  void      setDBFloor(double dBFloor_) { settings->setDBFloor(dBFloor_); }
  double&   rmsFloor() { return settings->rmsFloor(); }
  double&   rmsCeiling() { return settings->rmsCeiling(); }

  int       musicKey()     { return gMusicKey; }
  int       musicKeyType() { return _musicKeyType; }
  int       temperedType() { return _temperedType; }
  double    freqA() { return _freqA; }
  double    semitoneOffset() { return _semitoneOffset; }

#if 0
signals:
  void      activeChannelChanged(Channel *active);
  void      activeIntThresholdChanged(int thresholdPercentage);
  void      leftTimeChanged(double x);
  void      rightTimeChanged(double x);
  void      timeRangeChanged(double leftTime_, double rightTime_);
  void      channelsChanged();
  void      onChunkUpdate();

  void      musicKeyChanged(int key);
  void      musicKeyTypeChanged(int type);
  void      temperedTypeChanged(int type);

public slots:
#endif
  //void    setBuffers(int freq, int channels);
  //void    setFrameWindowSize(int index);
  //void    setWinFunc(int index);
  //void    setPitchMethod(int channel, int index) { pitch_method[channel] = index; }
  void      setInterpolatingType(int type) { interpolating_type = type; }
  void      setBisectionSteps(int num_steps) { bisection_steps = num_steps; }
  void      setFastRepeats(int num_repeats) { fast_correlation_repeats = num_repeats; }
  void      setAmplitudeMode(int amplitudeMode);
  void      setPitchContourMode(int pitchContourMode);

  //void      setMusicKey(int key)      { if(gMusicKey != key) { gMusicKey = key; emit musicKeyChanged(key); } }
  //void      setMusicKeyType(int type) { if(_musicKeyType != type) { _musicKeyType = type; emit musicKeyTypeChanged(type); } }
  void      setTemperedType(int type);
  void      setFreqA(double x);
  void      setFreqA(int x) { setFreqA(double(x)); }

  //void    setStepSize(int index);
  void      pauseSound();

  bool      setStream(AudioStream *mAudioStream);
  //void    jump_forward(int frames);
  void      updateViewLeftRightTimes();
  void      updateActiveChunkTime(double t);
  void      updateQuickRefSettings();
  
  void      resetActiveIntThreshold(int thresholdPercentage);

  void      doChunkUpdate();

};

//extern GlobalData *gdata;

#endif
