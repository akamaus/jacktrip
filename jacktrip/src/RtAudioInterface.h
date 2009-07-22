//*****************************************************************
/*
  JackTrip: A System for High-Quality Audio Network Performance
  over the Internet

  Copyright (c) 2008 Juan-Pablo Caceres, Chris Chafe.
  SoundWIRE group at CCRMA, Stanford University.

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation
  files (the "Software"), to deal in the Software without
  restriction, including without limitation the rights to use,
  copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following
  conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.
*/
//*****************************************************************

/**
 * \file RtAudioInterface.h
 * \author Juan-Pablo Caceres
 * \date July 2009
 */

#ifndef __RTAUDIOINTERFACE_H__
#define __RTAUDIOINTERFACE_H__

#include "RtAudio.h"

#include "AudioInterface.h"


class JackTrip; // Forward declaration

class RtAudioInterface : public AudioInterface
{
public:

  /** \brief The class constructor
   * \param jacktrip Pointer to the JackTrip class that connects all classes (mediator)
   * \param NumInChans Number of Input Channels
   * \param NumOutChans Number of Output Channels
   * \param AudioBitResolution Audio Sample Resolutions in bits
   * \param ClientName Client name in Jack
   */
  RtAudioInterface(JackTrip* jacktrip,
                   int NumInChans = 2, int NumOutChans = 2,
                   audioBitResolutionT AudioBitResolution = BIT16);

  virtual ~RtAudioInterface();

  virtual void listAllInterfaces();

  virtual void setup();
  /// \todo IMPLEMENT
  virtual void close() {}
  virtual int startProcess() const;
  /// \todo IMPLEMENT
  virtual int stopProcess() const { return 0; }
  // This has no effect in RtAudio
  virtual void connectDefaultPorts() {}

  //--------------SETTERS---------------------------------------------
  // This has no effect in RtAudio
  virtual void setClientName(const char* /*ClientName*/) {}
  //------------------------------------------------------------------

  //--------------GETTERS---------------------------------------------
  virtual uint32_t getSampleRate() const { return mSamplingRate; }
  virtual uint32_t getBufferSizeInSamples() const { return mBufferSize; }
  //------------------------------------------------------------------


private:
  int RtAudioCallback(void *outputBuffer, void *inputBuffer, unsigned int nFrames,
                      double streamTime, RtAudioStreamStatus status);
  static int wrapperRtAudioCallback(void *outputBuffer, void *inputBuffer, unsigned int nFrames,
                                    double streamTime, RtAudioStreamStatus status, void *userData);

  void printDeviceInfo(unsigned int deviceId);

  JackTrip* mJackTrip;

  int8_t* mInputPacket; ///< Packet containing all the channels to read from the RingBuffer
  int8_t* mOutputPacket;  ///< Packet containing all the channels to send to the RingBuffer

  QVarLengthArray<float*> mInBuffer; ///< Vector of Input buffers/channel read from JACK
  QVarLengthArray<float*> mOutBuffer; ///< Vector of Output buffer/channel to write to JACK

  uint32_t mSamplingRate;
  uint32_t mBufferSize;
  RtAudio* mRtAudio; ///< RtAudio class

  //QVarLengthArray<sample_t*> mInProcessBuffer;///< Vector of Input buffers/channel for ProcessPlugin
  //QVarLengthArray<sample_t*> mOutProcessBuffer;///< Vector of Output buffers/channel for ProcessPlugin
};

#endif // __RTAUDIOINTERFACE_H__
