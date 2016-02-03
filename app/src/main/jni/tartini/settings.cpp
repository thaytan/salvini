#include "settings.h"

Settings::Settings() : analysis_threshold(93),
  freq_a (0.0), analysis_mode(MPM), analysis_buffer_ms (48),
  step_size_ms (24), _dBFloor(-150.0), _topPitch(128.0),
  volume_threshold(-100.0), clarity_threshold(25),
  yellow_limit(20), red_limit(30),
  samples_threshold(2), base_pitch (440)
{
  amp_thresholds[AMPLITUDE_RMS][0]           = -85.0; amp_thresholds[AMPLITUDE_RMS][1]           = -0.0;
  amp_thresholds[AMPLITUDE_MAX_INTENSITY][0] = -30.0; amp_thresholds[AMPLITUDE_MAX_INTENSITY][1] = -20.0;
  amp_thresholds[AMPLITUDE_CORRELATION][0]   =  0.40; amp_thresholds[AMPLITUDE_CORRELATION][1]   =  1.00;
  amp_thresholds[FREQ_CHANGENESS][0]         =  0.50; amp_thresholds[FREQ_CHANGENESS][1]         =  0.02;
  amp_thresholds[DELTA_FREQ_CENTROID][0]     =  0.00; amp_thresholds[DELTA_FREQ_CENTROID][1]     =  0.10;
  amp_thresholds[NOTE_SCORE][0]              =  0.03; amp_thresholds[NOTE_SCORE][1]              =  0.20;
  amp_thresholds[NOTE_CHANGE_SCORE][0]       =  0.12; amp_thresholds[NOTE_CHANGE_SCORE][1]       =  0.30;

  amp_weights[0] = 0.2;
  amp_weights[1] = 0.2;
  amp_weights[2] = 0.2;
  amp_weights[3] = 0.2;
  amp_weights[4] = 0.2;
}

double Settings::getFreqA ()
{
  return freq_a;
}

void Settings::setFreqA (double x)
{
  freq_a = x;
}

void Settings::setAnalysisThreshold(int t)
{
  analysis_threshold = t;
}

int Settings::getAnalysisThreshold()
{
  return analysis_threshold;
}

void Settings::setAnalysisMode (AnalysisMode m)
{
  analysis_mode = m;
}

void Settings::setAnalysisBufferMS (int b)
{
  analysis_buffer_ms = b;
}

void Settings::setStepSizeMS (int s)
{
  step_size_ms = s;
}

void Settings::setAmpThreshold(int mode, int index, double value)
{
  amp_thresholds[mode][index] = value;
}

double Settings::ampThreshold(int mode, int index)
{
  return amp_thresholds[mode][index];
}

void Settings::setAmpWeight(int mode, double value)
{
  amp_weights[mode] = value;
}

double Settings::ampWeight(int mode)
{
  return amp_weights[mode];
}
