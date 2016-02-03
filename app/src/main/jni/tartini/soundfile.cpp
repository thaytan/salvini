 /***************************************************************************
                           soundfile.cpp  -  description
                              -------------------
     begin                : Sat Jul 10 2004
     copyright            : (C) 2004-2005 by Philip McLeod
     email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
  ***************************************************************************/
#include <stdio.h>

#include "myassert.h"
#include "soundfile.h"
#include "mystring.h"
#include "array1d.h"
#include "useful.h"
//#include "gdata.h"
#include <algorithm>
#include "channel.h"

#include "audio_stream.h"

//const double v8=0x7F, v16=0x7FFF, v32=0x7FFFFFFF;

typedef unsigned char byte;

SoundFile::SoundFile(GlobalData *gdata_) : gdata (gdata_), myTransforms (gdata_)
{
  stream = NULL;
  // TJS filteredStream = NULL;
  _framesPerChunk = 0;
  tempWindowBuffer = NULL;
  tempWindowBufferDouble = NULL;
  tempWindowBufferFiltered = NULL;
  tempWindowBufferFilteredDouble = NULL;
  _startTime = 0.0;
  _chunkNum = 0;
  _offset = 0; //Number of frame to read into file to get to time 0 (half buffer size).
  _saved = true;
  firstTimeThrough = true;
  _doingDetailedPitch = false;
}

/*
SoundFile::SoundFile(const char *filename_)
{
  filename = NULL;
  stream = NULL;
  openRead(filename_);
  _framesPerChunk = 0;
  //channelInsertPtrs = NULL;
  tempChunkBuffer = NULL;
  tempWindowBuffer = NULL;
  _startTime = 0.0;
  _chunkNum = 0;
}
*/

SoundFile::~SoundFile()
{
  uninit();
}

//free up all the memory of everything used
void SoundFile::uninit()
{
  //
  //if(channelInsertPtrs) { delete channelInsertPtrs; channelInsertPtrs = NULL; }
  for(int j=0; j<numChannels(); j++) {
    delete channels(j);
    delete[] (tempWindowBuffer[j]-16);
    delete[] (tempWindowBufferDouble[j]-16);
    delete[] (tempWindowBufferFiltered[j]-16);
    delete[] (tempWindowBufferFilteredDouble[j]-16);
  }
  channels.resize(0);
  delete[] tempWindowBuffer; tempWindowBuffer = NULL;
  delete[] tempWindowBufferDouble; tempWindowBufferDouble = NULL;
  delete[] tempWindowBufferFiltered; tempWindowBufferFiltered = NULL;
  delete[] tempWindowBufferFilteredDouble; tempWindowBufferFilteredDouble = NULL;

  setFramesPerChunk(0);
  _startTime = 0.0;
  _chunkNum = 0;
  _offset = 0;
}

bool SoundFile::configure(int rate_, int channels_, int bits_)
{
  uninit();
  setSaved(false);

  int windowSize_ = gdata->getAnalysisBufferSize(rate_);
  int stepSize_ = gdata->getAnalysisStepSize(rate_);

  stream = gdata->audio_stream;

  //_offset = (windowSize_ / stepSize_) / 2;
  _offset = windowSize_ / 2;
#if 0
  fprintf(stderr, "--------Recording------------\n");
  fprintf(stderr, "rate = %d\n", rate_);
  fprintf(stderr, "channels = %d\n", channels_);
  fprintf(stderr, "bits = %d\n", bits_);
  fprintf(stderr, "windowSize = %d\n", windowSize_);
  fprintf(stderr, "stepSize = %d\n", stepSize_);
  fprintf(stderr, "-----------------------------\n");
#endif

  setFramesPerChunk(stepSize_);

  channels.resize(channels_);
  //fprintf(stderr, "channels = %d\n", numChannels());
  for(int j=0; j<numChannels(); j++) {
    channels(j) = new Channel(gdata, this, windowSize_);
    //fprintf(stderr, "channel size = %d\n", channels(j)->size());
  }

  myTransforms.init(windowSize_, 0, rate_, gdata->doingEqualLoudness());

  tempWindowBuffer = new float*[numChannels()];
  tempWindowBufferDouble = new double*[numChannels()];
  tempWindowBufferFiltered = new float*[numChannels()];
  tempWindowBufferFilteredDouble = new double*[numChannels()];

  for(int c=0; c<numChannels(); c++) {
    tempWindowBuffer[c] = (new float[bufferSize()+16]) + 16; //array ranges from -16 to bufferSize()
    tempWindowBufferDouble[c] = (new double[bufferSize()+16]) + 16; //array ranges from -16 to bufferSize()
    tempWindowBufferFiltered[c] = (new float[bufferSize()+16]) + 16; //array ranges from -16 to bufferSize()
    tempWindowBufferFilteredDouble[c] = (new double[bufferSize()+16]) + 16; //array ranges from -16 to bufferSize()
  }

  _doingDetailedPitch = gdata->doingDetailedPitch();

  return true;

}

void SoundFile::close()
{
  //stream->close();
  uninit();
}
  
void SoundFile::applyEqualLoudnessFilter(int n)
{
  int c, j;
  for(c=0; c<numChannels(); c++) {
    channels(c)->highPassFilter->filter(tempWindowBuffer[c], tempWindowBufferFiltered[c], n);
    for(j=0; j<n; j++) tempWindowBufferFiltered[c][j] = bound(tempWindowBufferFiltered[c][j], -1.0f, 1.0f);
  }
}

//void SoundFile::initRecordingChunk()
void SoundFile::recordChunk(int n)
{
  //int n = offset();
  int ret = blockingRead(gdata->audio_stream, tempWindowBuffer, n);
  if(ret < n) {
    fprintf(stderr, "Data lost in reading from audio device\n");
  }
  
  finishRecordChunk(n);
}

void SoundFile::finishRecordChunk(int n)
{
  if(equalLoudness()) applyEqualLoudnessFilter(n);

  FilterState filterState;

  for(int c=0; c<numChannels(); c++) {
    channels(c)->shift_left(n);
    //std::copy(tempWindowBuffer[c], tempWindowBuffer[c]+n, channels(c)->end() - n);
    //channels(c)->highPassFilter->getState(&filterState);
    toChannelBuffer(c, n);
    //if(channels(c) == gdata->getActiveChannel())
    channels(c)->processNewChunk(&filterState);
  }

  //int ret = blockingWrite(stream, tempWindowBuffer, n);
  //if(ret < n) fprintf(stderr, "Error writing to disk\n");

  //setCurrentChunk(currentStreamChunk());
  incrementChunkNum();
}

bool SoundFile::setupPlayChunk() {
  int c;
  int n = framesPerChunk();
  int ret = blockingRead(stream, tempWindowBuffer, n);
  if(ret < n) return false;

  for(c=0; c<numChannels(); c++) {
    channels(c)->shift_left(n);
    toChannelBuffer(c, n);
    if(gdata->doingActive() && channels(c) == gdata->getActiveChannel()) {
      channels(c)->processChunk(currentChunk()+1);
    }
  }
  //setCurrentChunk(currentStreamChunk());
  return true;
}

#if 0
/** Plays one chunk of sound to the associated stream
  @return true on success. false for end of file or error
*/
//Note: static
bool SoundFile::playRecordChunk(SoundFile *play, SoundFile *rec)
{
  int n = rec->framesPerChunk();
  myassert(n == play->framesPerChunk());
  //int ch = rec->numChannels();
  myassert(rec->numChannels() == play->numChannels());

  play->setupPlayChunk();
  //if(!play->setupPlayChunk()) return false;

  int ret = blockingWriteRead(gdata->audio_stream, play->tempWindowBuffer, play->numChannels(), rec->tempWindowBuffer, rec->numChannels(), n);
  if(ret < n) {
    fprintf(stderr, "Error writing/reading to audio device\n");
  }

  rec->finishRecordChunk(n);
  return true;
}
#endif

/** Reads framesPerChunk frames from the SoundStream s
    into the end of the channel buffers. I.e losing framesPerChunk
    frames from the beginning of the channel buffers.
    If s is NULL (the defult) the the file stream is used
  @param s The SoundStream to read from or NULL (the default) the current file stream is used    
  @return The number of frames actually read ( <= framesPerChunk() )
*/
//int SoundFile::readChunk(int n, SoundStream *s)
int SoundFile::readChunk(int n)
{
  int c;
  //if(!s) s = stream;
  
  int ret = blockingRead(stream, tempWindowBuffer, n);
  if(equalLoudness()) {
    applyEqualLoudnessFilter(n);
  }
  if(ret < n) return ret;
  
  FilterState filterState;
  

  //nextChunk();
  for(c=0; c<numChannels(); c++) {
    channels(c)->shift_left(n);
    //channels(c)->highPassFilter->getState(&filterState);
    toChannelBuffer(c, n);
    channels(c)->processNewChunk(&filterState);
  }
  //incrementChunkNum();
  //setCurrentChunk(currentStreamChunk());
  return ret;
}

/** Waits until there is n frames of data to read from s,
  Then puts the data in buffer.
  @param s The SoundStream to read from
  @param buffer The chunk buffer to put the data into. (s->channels of at least n length)
    This is padded with zeros (up to n) if there is less than n frames read.
  @param n The number of frames to read
  @return The number of frames read.
*/
int SoundFile::blockingRead(SoundStream *s, float **buffer, int n)
{ 
  int c;
  myassert(s);
  myassert(n >= 0 && n <= bufferSize());
  int ret = s->readFloats(buffer, n, numChannels());
  if(ret < n) { //if not all bytes are read
    for(c=0; c<numChannels(); c++) { //set remaining bytes to zeros
      //memset(buffer[c]+ret, 0, (n-ret) * sizeof(float)); 
      std::fill(buffer[c]+ret, buffer[c]+n, 0.0f);
    }
  }
  return ret;
}

/** Writes n frames of data to s from buffer.
  @param s The SoundStream to write to
  @param buffer The chunk buffer data to use. (s->channels of at least n length)
  @param n The number of frames to write
  @return The number of frames written.
*/
int SoundFile::blockingWrite(SoundStream *s, float **buffer, int n)
{
  if(s == NULL) return n;
  return s->writeFloats(buffer, n, numChannels());
}

/** Writes n frames of data to s and reads n frames of data from s
If less than n frams are read, the remaining buffer is filled with zeros
@param s The SoundStream to write to then read from
@param writeBuffer The data that is to be written to the stream
@param readBuffer The data that is read back from the stream
@param n The amount of data in each of the buffers. Note: They must be the same size
@param ch The number of channels
*/
//Note: static
int SoundFile::blockingWriteRead(SoundStream *s, float **writeBuffer, int writeCh, float **readBuffer, int readCh, int n)
{
  int c;
  myassert(s);
  //myassert(n >= 0 && n <= bufferSize());
  int ret = s->writeReadFloats(writeBuffer, writeCh, readBuffer, readCh, n);
  if(ret < n) { //if not all bytes are read
    for(c=0; c<readCh; c++) { //set remaining bytes to zeros
      //memset(buffer[c]+ret, 0, (n-ret) * sizeof(float)); 
      std::fill(readBuffer[c]+ret, readBuffer[c]+n, 0.0f);
    }
  }
  return ret;
}

/**
  @param c Channel number
  @param n Number of frames to copy from tempWindowBuffer into channel
*/
void SoundFile::toChannelBuffer(int c, int n)
{
  std::copy(tempWindowBuffer[c], tempWindowBuffer[c]+n, channels(c)->end() - n);
  if(equalLoudness()) {
    std::copy(tempWindowBufferFiltered[c], tempWindowBufferFiltered[c]+n, channels(c)->filteredInput.end() - n);
  
/*
    for(int j=channels(c)->size()-n; j<channels(c)->size(); j++) {
      channels(c)->filteredInput.at(j) = bound(channels(c)->highPassFilter->apply(channels(c)->at(j)), -1.0, 1.0);
    }
    //channels(c)->filteredData.push_back(channels(c)->filteredInput.end()-n, n);
*/
  }
}

void SoundFile::toChannelBuffers(int n)
{
  int c;
  myassert(n >= 0 && n <= bufferSize());
  for(c=0; c<numChannels(); c++) {
    channels(c)->shift_left(n);
    toChannelBuffer(c, n);
  }
}

//int SoundFile::readN(SoundStream *s,int n)
int SoundFile::readN(int n)
{
  myassert(n >= 0 && n <= bufferSize());
  int ret = blockingRead(stream, tempWindowBuffer, n);
  // TJS if(equalLoudness()) ret = blockingRead(filteredStream, tempWindowBufferFiltered, n);
  toChannelBuffers(n);
  return ret;
}

/** Writes framesPerChunk frames into the SoundStream s
    from end of the channel buffers.
    If s is NULL (the defult) then the file stream is used
  @param s The SoundStream to write to, or NULL (the default) the current file stream is used    
  @return The number of frames actually written ( <= framesPerChunk() )
*/
/*
int SoundFile::writeChunk(SoundStream *s)
{
  if(!s) s = stream;
  //myassert(numChannels() == s->channels);
  lock();
  for(int c=0; c<numChannels(); c++)
    std::copy(channels(c)->end() - framesPerChunk(), channels(c)->end(), tempWindowBuffer[c]);
  unlock();
  int ret = s->writeFloats(tempWindowBuffer, framesPerChunk(), numChannels());
  return ret;
}
*/

/** Process the chunks for all the channels
   Increment the chunkNum.
*/

void SoundFile::processNewChunk()
{
  FilterState filterState;
  for(int j=0; j<numChannels(); j++) {
    //channels(j)->highPassFilter->getState(&filterState);
    channels(j)->processNewChunk(&filterState);
  }
}


/** Preprocess the whole sound file,
   by looping through and processing every chunk in the file.
   A progress bar is displayed in the toolbar, because this can
   be time consuming.
*/
void SoundFile::preProcess()
{
  //jumpToChunk(0);
  gdata->setDoingActiveAnalysis(true);
  myassert(firstTimeThrough == true);
  readChunk(bufferSize() - offset());
  //readChunk(framesPerChunk());
  //processNewChunk();
  //printf("preProcessing\n");
  //for(int j=0; j<numChannels(); j++)
  //  channels(j)->setframesPerChunk(toRead);

  int frameCount = 1;

  //while(read_n(toRead, stream) == toRead) { // put data in channels
  while(readChunk(framesPerChunk()) == framesPerChunk()) { // put data in channels
    //printf("pos = %d\n", stream->pos);
    //processNewChunk();
    //incrementChunkNum();
    frameCount++;
  }
  //printf("totalChunks=%d\n", totalChunks());
  //printf("currentChunks=%d\n", currentChunk());
#if 0
  filteredStream->close();
  filteredStream->open_read(filteredFilename);
#endif
  //jumpToChunk(0);

  gdata->setDoingActiveAnalysis(false);
  firstTimeThrough = false;
  //printf("freqLookup.size()=%d\n", channels(0)->freqLookup.size());
}

//shift all the channels data left by n frames
void SoundFile::shift_left(int n)
{
  for(int j=0; j<numChannels(); j++) {
    channels(j)->shift_left(n);
  }
}

/** Resets the file back to the start and
    clearing the channel buffers */
/*
void SoundFile::beginning()
{
  jumpToChunk(0);
}
*/

/*
void SoundFile::nextChunk()
{
  shift_left(framesPerChunk());
  incrementChunkNum();
}
*/

#if 0
void SoundFile::jumpToChunk(int chunk)
{
  return;

  //if(chunk == currentChunk()) return;
  int c;
  int pos = chunk * framesPerChunk() - offset();
  if(pos < 0) {
    //stream->jump_to_frame(0);
    //if(equalLoudness()) filteredStream->jump_to_frame(0);
    for(c=0; c<numChannels(); c++)
      channels(c)->reset();
    //readN(stream, bufferSize() + pos);
    readN(bufferSize() + pos);
  } else {
    //stream->jump_to_frame(pos);
    //if(equalLoudness()) filteredStream->jump_to_frame(pos);
    //readN(stream, bufferSize());
    readN(bufferSize());
  }
/*  int n = bufferSize() / framesPerChunk();
  if(chunk+offset() < n) {
    stream->jump_to_frame(0);
    for(c=0; c<numChannels(); c++)
      channels(c)->reset();
    if(chunk+offset() >= 0) {
      for(int j=0; j<chunk+offset(); j++)
        readChunk();
    }
  } else {
    //if(chunk == currentChunk()+1) { readChunk(); return; }
    stream->jump_to_frame((chunk+offset()-n) * framesPerChunk());
    for(int j=0; j<n; j++)
      readChunk();
  }
*/
  //setCurrentChunk(chunk);
  
  //for(c=0; c<numChannels(); c++) {
  //  if(channels(c) == gdata->getActiveChannel()) channels(c)->processChunk(currentChunk());
  //}
}
#endif

int SoundFile::bufferSize() {
  myassert(!channels.isEmpty());
  return channels(0)->size();
}

int SoundFile::totalChunks() {
  myassert(!channels.isEmpty());
  return channels(0)->totalChunks();
}

bool SoundFile::inFile()
{
  int c = currentChunk();
  return (c >= 0 && c < totalChunks());
}
