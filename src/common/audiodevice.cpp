// audiodevice.cpp
//
// Abstract base class for audio output sources.
//
//   (C) Copyright 2014-2016 Fred Gleason <fredg@paravelsystems.com>
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License version 2 as
//   published by the Free Software Foundation.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public
//   License along with this program; if not, write to the Free Software
//   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <samplerate.h>

#include "audiodevice.h"
#include "logging.h"

AudioDevice::AudioDevice(Codec *codec,QObject *parent)
  : QObject(parent)
{
  audio_codec=codec;

  connect(codec,SIGNAL(audioWritten(unsigned)),
	  this,SLOT(synchronousWrite(unsigned)));
}


AudioDevice::~AudioDevice()
{
}


bool AudioDevice::isAvailable() const
{
  return true;
}


void AudioDevice::stop()
{
}


void AudioDevice::meterLevels(int *lvls) const
{
  for(unsigned i=0;i<MAX_AUDIO_CHANNELS;i++) {
    lvls[i]=audio_meter_levels[i];
  }
}


QString AudioDevice::typeText(AudioDevice::Type type)
{
  QString ret=tr("Unknown Device");

  switch(type) {
  case AudioDevice::Stdout:
    ret=tr("Standard Output");
    break;

  case AudioDevice::Alsa:
    ret=tr("Advanced Linux Sound Architecture (ALSA)");
    break;

  case AudioDevice::AsiHpi:
    ret=tr("AudioScience HPI");
    break;

  case AudioDevice::File:
    ret=tr("File Streaming");
    break;

  case AudioDevice::Jack:
    ret=tr("JACK Audio Connection Kit");
    break;

  case AudioDevice::LastType:
    break;
  }

  return ret;
}


QString AudioDevice::optionKeyword(AudioDevice::Type type)
{
  QString ret;

  switch(type) {
  case AudioDevice::Stdout:
    ret="stdout";
    break;

  case AudioDevice::Alsa:
    ret="alsa";
    break;

  case AudioDevice::AsiHpi:
    ret="asihpi";
    break;

  case AudioDevice::File:
    ret="file";
    break;

  case AudioDevice::Jack:
    ret="jack";
    break;

  case AudioDevice::LastType:
    break;
  }

  return ret;
}


AudioDevice::Type AudioDevice::type(const QString &key)
{
  AudioDevice::Type ret=AudioDevice::LastType;

  for(int i=0;i<AudioDevice::LastType;i++) {
    if(optionKeyword((AudioDevice::Type)i)==key.toLower()) {
      ret=(AudioDevice::Type)i;
    }
  }

  return ret;
}


QString AudioDevice::formatString(AudioDevice::Format fmt)
{
  QString ret="UNKNOWN";

  switch(fmt) {
  case AudioDevice::FLOAT:
    ret="FLOAT";
    break;

  case AudioDevice::S16_LE:
    ret="S16_LE";
    break;

  case AudioDevice::S32_LE:
    ret="S32_LE";
    break;

  case AudioDevice::LastFormat:
    break;
  }

  return ret;
}


void AudioDevice::synchronousWrite(unsigned frames)
{
}


void AudioDevice::setMeterLevels(float *lvls)
{
  for(unsigned i=0;i<MAX_AUDIO_CHANNELS;i++) {
    if(lvls[i]==0) {
      audio_meter_levels[i]=10000;
    }
    else {
      audio_meter_levels[i]=(int)(-2000.0*log10f(lvls[i]));
    }
  }
}


void AudioDevice::setMeterLevels(int *lvls)
{
  for(unsigned i=0;i<MAX_AUDIO_CHANNELS;i++) {
    audio_meter_levels[i]=lvls[i];
  }
}


void AudioDevice::updateMeterLevels(int *lvls)
{
  for(unsigned i=0;i<MAX_AUDIO_CHANNELS;i++) {
    if(lvls[i]>audio_meter_levels[i]) {
      audio_meter_levels[i]=lvls[i];
    }
  }
}


Codec *AudioDevice::codec()
{
  return audio_codec;
}


void AudioDevice::remixChannels(float *pcm_out,unsigned chans_out,float *pcm_in,
				unsigned chans_in,unsigned nframes)
{
  if(chans_out==chans_in) {
    memcpy(pcm_out,pcm_in,nframes*chans_in*sizeof(float));
    return;
  }
  if((chans_in==1)&&(chans_out==2)) {
    for(unsigned i=0;i<nframes;i++) {
      pcm_out[2*i]=pcm_in[i];
      pcm_out[2*i+1]=pcm_in[i];
    }
    return;
  }
  if((chans_in==2)&&(chans_out==1)) {
    for(unsigned i=0;i<nframes;i++) {
      pcm_out[i]=(pcm_in[2*i]+pcm_in[2*i+1])/2.0;
    }
    return;
  }
  Log(LOG_ERR,
      QString().sprintf("invalid channel remix: chans_in: %d  chans_out: %d",
			chans_in,chans_out));
  exit(256);
}


void AudioDevice::convertFromFloat(int16_t *pcm_out,const float *pcm_in,
				   unsigned nframes,unsigned chans)
{
  src_float_to_short_array(pcm_in,(short *)pcm_out,nframes*chans);
}


void AudioDevice::convertFromFloat(int32_t *pcm_out,const float *pcm_in,
				   unsigned nframes,unsigned chans)
{
  src_float_to_int_array(pcm_in,(int *)pcm_out,nframes*chans);
}


void AudioDevice::peakLevels(float *lvls,const float *pcm,unsigned nframes,
			     unsigned chans)
{
  for(unsigned i=0;i<chans;i++) {
    lvls[i]=0.0;
  }

  for(unsigned i=0;i<nframes;i+=chans) {
    for(unsigned j=0;j<chans;j++) {
      if(pcm[i+j]>lvls[j]) {
	lvls[j]=pcm[i+j];
      }
    }
  }
}


void AudioDevice::peakLevels(int *lvls,const float *pcm,unsigned nframes,
			     unsigned chans)
{
  float levels[chans];

  peakLevels(levels,pcm,nframes,chans);

  for(unsigned i=0;i<chans;i++) {
    if(levels[i]==0) {
      lvls[i]=10000;
    }
    else {
      lvls[i]=(int)(-2000.0*log10f(levels[i]));
    }
  }
}