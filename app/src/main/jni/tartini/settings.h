#ifndef __SETTINGS_H__
#define __SETTINGS_H__

enum AnalysisMode { MPM, AUTOCORRELATION, MPM_MODIFIED_CEPSTRUM };

enum AmplitudeModes { AMPLITUDE_RMS, AMPLITUDE_MAX_INTENSITY, AMPLITUDE_CORRELATION, FREQ_CHANGENESS, DELTA_FREQ_CENTROID, NOTE_SCORE, NOTE_CHANGE_SCORE };
#define NUM_AMP_MODES 7

class Settings
{

public:
  Settings();
  double getFreqA ();
  void setFreqA (double x);

  void setAnalysisThreshold(int t);
  int getAnalysisThreshold();

  void setAnalysisMode (AnalysisMode m);
  AnalysisMode getAnalysisMode () { return analysis_mode; };

  void setAnalysisBufferMS (int b);
  int getAnalysisBufferMS () { return analysis_buffer_ms; };

  void setStepSizeMS (int s);
  int getStepSizeMS () { return step_size_ms; };

  void      setAmpThreshold(int mode, int index, double value);
  double    ampThreshold(int mode, int index);

  void      setAmpWeight(int mode, double value);
  double    ampWeight(int mode);

  double    dBFloor() { return _dBFloor; }
  void      setDBFloor(double dBFloor_) { _dBFloor = dBFloor_; }
  double&   rmsFloor() { return amp_thresholds[AMPLITUDE_RMS][0]; } //in dB
  double&   rmsCeiling() { return amp_thresholds[AMPLITUDE_RMS][1]; } //in dB

  double    topPitch() { return _topPitch; }

  float     getVolumeThreshold() { return volume_threshold; }
  float     getClarityThreshold() { return clarity_threshold; }
  int       getYellowLimit() { return yellow_limit; }
  int       getRedLimit() { return red_limit; }
  int       getSamplesThreshold() { return samples_threshold; }

  int       getBasePitch() { return base_pitch; }

private:
  int analysis_threshold;
  double freq_a;
  AnalysisMode analysis_mode;
  int analysis_buffer_ms;
  int step_size_ms;

  double _dBFloor;
  double amp_thresholds[NUM_AMP_MODES][2];
  double amp_weights[NUM_AMP_MODES];

  double _topPitch; /**< The highest possible note pitch allowed (lowest possible is 0) */

  float volume_threshold;
  float clarity_threshold;
  int yellow_limit;
  int red_limit;
  int samples_threshold;

  int base_pitch;
};

#endif 
