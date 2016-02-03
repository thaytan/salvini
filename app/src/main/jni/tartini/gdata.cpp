/***************************************************************************
                          gdata.cpp  -  
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
#if defined(_OS_LINUX_) || defined(Q_OS_LINUX)
#include <unistd.h>
#endif

#include <stdio.h>

#include <glib.h>

#include "gdata.h"
#include "audio_stream.h"
#include "Filter.h"
#include "mystring.h"
#include "soundfile.h"
#include "channel.h"
#include "conversions.h"
#include "musicnotes.h"

#ifndef WINDOWS
//for multi-threaded profiling
struct itimerval profiler_value;
struct itimerval profiler_ovalue;
#endif

int frame_window_sizes[NUM_WIN_SIZES] = { 512, 1024, 2048, 4096, 8192 };
const char *frame_window_strings[NUM_WIN_SIZES] = { "512", "1024", "2048", "4096", "8192" };

float step_sizes[NUM_STEP_SIZES] = { 1.0f, 0.5f, 0.25f, 0.2f, 0.1f, 0.05f };
const char *step_size_strings[NUM_STEP_SIZES] = { "100%", "50%", "25%", "20%", "10%", "5%" };

const char *pitch_method_strings[NUM_PITCH_METHODS] = { "FFT interpolation", "Fast-correlation",  "Correlation (squared error) 1", "Correlation (squared error) 2", "Correlation (abs error) 1", "Correlation (abs error) 2", "Correlation (multiplied) 1", "Correlation (multiplied) 2" };

const char *interpolating_type_strings[NUM_INTERPOLATING_TYPES] = { "Linear", "Cubic B-Spline", "Hermite Cubic" };
//GlobalData *gdata = NULL;

//Define the Phase function. This one is applicable to 
//accelerating sources since the phase goes as x^2.
float phase_function(float x)
{
  float phase;
  
  //phase = x*x;
  phase = x;
  return(phase);
}

GlobalData::GlobalData(Settings *settings_) : settings(settings_)
{
  _polish = true;

  //setNoiseThresholdDB(-100.0);
  //setChangenessThreshold(0.8); //1.8);

  activeChannel = NULL;
  _doingActiveAnalysis = 0;
  _doingActiveFFT = 0;
  _amplitudeMode = 0;
  _pitchContourMode = 0;
  _analysisMode = MPM;
  _doingActiveCepstrum = 0;

  updateQuickRefSettings();
  
    peakThreshold = -60.0; //in dB
    correlationThreshold = 0.00001f; //0.5 in the other scale (log);
    //equalLoudness = false; //true;
    //useMasking = false; //true;
    //harmonicNum = 1;

    interpolating_type = 2; //HERMITE_CUBIC;
    using_coefficients_table = true;

    audio_stream = NULL;
    soundMode = SOUND_PLAY;

    cur_note = 0.0;

    need_update = false;

  //_musicKey = 2; //C
  _musicKeyType = 0; //ALL_NOTES
  _temperedType = 0; //EVEN_TEMPERED
  initMusicStuff();

  rtta_data = new RTTAdata(this);
}

GlobalData::~GlobalData()
{
    //Note: The soundFiles is responsible for cleaning up the data the channels point to
    channels.clear();
    for(uint j=0; j<soundFiles.size(); j++)
      delete soundFiles[j];
    soundFiles.clear();
      
    std::vector<Filter*>::iterator fi;
    for(fi=filter_hp.begin(); fi!=filter_hp.end(); ++fi)
	    delete (*fi);
    filter_hp.clear();
    for(fi=filter_lp.begin(); fi!=filter_lp.end(); ++fi)
	    delete (*fi);

    filter_lp.clear();

    //if(fwinfunc) delete fwinfunc;
    //if(loudnessFunc) delete loudnessFunc;
    //if(sound_file_stream) delete sound_file_stream;
    //if(audio_stream) delete audio_stream;

#if 0
    free(fct_in_data);
    free(fct_out_data);
    free(fct_draw_data);
#endif

    delete rtta_data;
}

/*
void GlobalData::setBuffers(int freq, int channels)
{
    //bufferMutex.lock();

    //coefficients_table.resize(buffer_size*4*channels);
    coefficients_table.resize(buffer_size*4, channels);
    fwinfunc->create(winfunc, buffer_size);
    loudnessFunc->create_loudness(buffer_size/2, freq);
    freqHistory.resize(channels);

    std::vector<Filter*>::iterator fi;
    for(fi=filter_hp.begin(); fi!=filter_hp.end(); ++fi)
	delete (*fi);
    filter_hp.clear();
    for(fi=filter_lp.begin(); fi!=filter_lp.end(); ++fi)
	delete (*fi);
    filter_lp.clear();

    const double R = 0.94; //0.94 to 0.99
    double fhp_a[3] = { 1.0, -2.0, 1.0 };
    double fhp_b[2] = { -2.0*R, R*R };
    double flp_a[2] = { 0.109, 0.109 };
    double flp_b[8] = { -2.5359, 3.9295, -4.7532, 4.7251, -3.5548, 2.1396, -0.9879, 0.2836 };

    for(int j=0; j<channels; j++) {
	Filter *hp = new Filter();
	hp->make_IIR(fhp_a, 3, fhp_b, 2);
	Filter *lp = new Filter();
	lp->make_IIR(flp_a, 2, flp_b, 8);
	filter_hp.push_back(hp);
	filter_lp.push_back(lp);
    }

    //bufferMutex.unlock();
}
*/

/*
void GlobalData::setFrameWindowSize(int index)
{
    buffer_size = frame_window_sizes[index];
    if(input_stream) setBuffers(input_stream->freq, input_stream->channels);
    need_update = false; //wait for buffers to get more data before the next update
}

void GlobalData::setWinFunc(int index)
{
    winfunc = index;
    fwinfunc->create(winfunc, buffer_size);
}

void GlobalData::setStepSize(int index)
{
    //printf("gdata waiting for lock\n"); fflush(stdout);
    //bufferMutex.lock();
    //printf("gdata got lock\n"); fflush(stdout);
    step_size = step_sizes[index];
    //printf("gdata lost lock\n"); fflush(stdout);
    //bufferMutex.unlock();
}
*/

SoundFile* GlobalData::getActiveSoundFile()
{
  return (activeChannel) ? activeChannel->getParent() : NULL;
}

bool GlobalData::setStream(AudioStream *mAudioStream)
{
  soundMode = SOUND_REC;
  audio_stream = mAudioStream;

  return true;
}

/*
void GlobalData::jump_forward(int frames)
{
    if(running == STREAM_PAUSE) {
	if(sound_file_stream) {
	    if(frames != 0) {
		//bufferMutex.lock();
		sound_file_stream->jump_forward(frames);
		//bufferMutex.unlock();
	    }
	    running = STREAM_UPDATE;
	}
    }
}
*/

void GlobalData::updateViewLeftRightTimes()
{
  double left = 0.0; //in seconds
  double right = 0.0; //in seconds
  Channel *ch;
  for(uint j = 0; j < channels.size(); j++) {
    ch = channels.at(j);
    //if(ch->isVisible()) {
      if(ch->startTime() < left) left = ch->startTime();
      if(ch->finishTime() > right) right = ch->finishTime();
    //}
  }
  //setLeftTime(left); //in seconds
  //setRightTime(right); //in seconds
}

void GlobalData::setActiveChannel(Channel *toActive)
{
  activeChannel = toActive;
  //if(activeChannel) updateActiveChunkTime(view->currentTime());
  //emit activeChannelChanged(activeChannel);
  //emit activeIntThresholdChanged(getActiveIntThreshold());
  //view->doUpdate();
}

void GlobalData::updateActiveChunkTime(double t)
{
  doChunkUpdate();
}


int GlobalData::getAnalysisBufferSize(int rate)
{  
  int windowSize = settings->getAnalysisBufferMS();
  // convert to samples
  windowSize = int(double(windowSize) * double(rate) / 1000.0);

  windowSize = toInt(nearestPowerOf2(windowSize));

  return windowSize;
}

int GlobalData::getAnalysisStepSize(int rate)
{  
  int stepSize = settings->getStepSizeMS();
  // convert to samples
  stepSize = int(double(stepSize) * double(rate) / 1000.0);

  stepSize = toInt(nearestPowerOf2(stepSize));

  return stepSize;
}

void GlobalData::updateQuickRefSettings()
{
#if 0 // FIXME
  _doingHarmonicAnalysis = qsettings->value("Analysis/doingHarmonicAnalysis", true).toBool();
  _doingFreqAnalysis = qsettings->value("Analysis/doingFreqAnalysis", true).toBool();
  _doingEqualLoudness = qsettings->value("Analysis/doingEqualLoudness", true).toBool();
  _doingAutoNoiseFloor = qsettings->value("Analysis/doingAutoNoiseFloor", true).toBool();
  _doingDetailedPitch = qsettings->value("Analysis/doingDetailedPitch", false).toBool();
  _fastUpdateSpeed = qsettings->value("Display/fastUpdateSpeed", 75).toInt();
  _slowUpdateSpeed = qsettings->value("Display/slowUpdateSpeed", 150).toInt();
  _vibratoSineStyle = qsettings->value("Advanced/vibratoSineStyle", false).toBool();
  _showMeanVarianceBars = qsettings->value("Advanced/showMeanVarianceBars", false).toBool();
#else
  _doingHarmonicAnalysis = true;
  _doingFreqAnalysis = true;
  _doingEqualLoudness = true;
  _doingAutoNoiseFloor = true;
  _doingDetailedPitch = true;
  _fastUpdateSpeed = 75;
  _slowUpdateSpeed = 150;
  _vibratoSineStyle = false;
  _showMeanVarianceBars = false;
#endif

  _analysisMode = settings->getAnalysisMode();
}

/*
void GlobalData::setNoiseThreshold(double noiseThreshold_)
{
  setNoiseThresholdDB(log10(noiseThreshold_) * 20.0);
}
*/
/*
void GlobalData::setNoiseThresholdDB(double noiseThresholdDB_)
{
  _noiseThresholdDB = bound(noiseThresholdDB_, dBFloor(), 0.0);
  _noiseThreshold = pow10(_noiseThresholdDB / 20.0);
  clearFreqLookup();
}

void GlobalData::setChangenessThreshold(double changenessThreshold_)
{
  if(changenessThreshold_ != _changenessThreshold) {
    _changenessThreshold = bound(changenessThreshold_, 0.0, 1.0);
    //clearAmplitudeLookup();
  }
}
*/
void GlobalData::clearFreqLookup()
{
    /*
  for(std::vector<Channel*>::iterator it1=channels.begin(); it1 != channels.end(); it1++) {
    (*it1)->clearFreqLookup();
  }
  */
  g_print ("FIXME: clearFreqLookup\n");
}

void GlobalData::setAmplitudeMode(int amplitudeMode)
{
  if(amplitudeMode != _amplitudeMode) {
    _amplitudeMode = amplitudeMode;
    clearAmplitudeLookup();
  }
}

void GlobalData::setPitchContourMode(int pitchContourMode)
{
  if(pitchContourMode != _pitchContourMode) {
    _pitchContourMode = pitchContourMode;
    clearFreqLookup();
  }
}

void GlobalData::clearAmplitudeLookup()
{
    // FIXME:
#if 0
  for(std::vector<Channel*>::iterator it1=channels.begin(); it1 != channels.end(); it1++) {
    (*it1)->clearAmplitudeLookup();
  }
#endif
}
  
int GlobalData::getActiveIntThreshold()
{
  Channel* active = getActiveChannel();
  if(active) return toInt(active->threshold() * 100.0f);
  //else return settings.getInt("Analysis", "thresholdValue");
  else return settings->getAnalysisThreshold();
}

void GlobalData::resetActiveIntThreshold(int thresholdPercentage)
{
  Channel* active = getActiveChannel();
  if(active) active->resetIntThreshold(thresholdPercentage);
}

void GlobalData::setAmpThreshold(int mode, int index, double value)
{
  settings->setAmpThreshold (mode, index, value);
  clearFreqLookup();
  recalcScoreThresholds();
}

double GlobalData::ampThreshold(int mode, int index)
{
  return settings->ampThreshold (mode, index);
}

void GlobalData::setAmpWeight(int mode, double value)
{
  settings->setAmpWeight(mode, value);
  clearFreqLookup();
  recalcScoreThresholds();
}

double GlobalData::ampWeight(int mode)
{
  return settings->ampWeight(mode);
}

void GlobalData::recalcScoreThresholds()
{
  for(std::vector<Channel*>::iterator it1=channels.begin(); it1 != channels.end(); it1++) {
    (*it1)->recalcScoreThresholds();
  }
}

void GlobalData::doChunkUpdate()
{
  //emit onChunkUpdate();
  g_print ("FIXME: doChunkUpdate\n");
}

void GlobalData::setTemperedType(int type)
{
  if(_temperedType != type) {
    if(_temperedType == 0 && type > 0) { //remove out the minors
      //if(mainWindow->keyTypeComboBox->currentIndex() >= 2) mainWindow->keyTypeComboBox->setCurrentIndex(1);
      //FIXME set music key type again?
      //if(_musicKeyType >= 2) setMusicKeyType(0);
    }
    _temperedType = type; // emit temperedTypeChanged(type);
  }
}

void GlobalData::setFreqA(double x)
{
	_freqA = x;
	_semitoneOffset = freq2pitch(x) - freq2pitch(440.0);
	settings->setFreqA(x);
}
