//*****************************************************************
/*
  PaulTrip: A System for High-Quality Audio Network Performance
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
 * \file JackAudioInterface.cpp
 * \author Juan-Pablo Caceres
 * \date June 2008
 */


#include "JackAudioInterface.h"
#include "globals.h"
#include <QTextStream>
#include <cstdlib>
#include <cstring>
#include <cmath>

using std::cout; using std::cout;
/// \todo Check that the RingBuffer Pointer have indeed been initialized before
/// computing anything


//*******************************************************************************
JackAudioInterface::JackAudioInterface(int NumInChans, int NumOutChans,
				       audioBitResolutionT AudioBitResolution)
  : mNumInChans(NumInChans), mNumOutChans(NumOutChans), 
    mAudioBitResolution(AudioBitResolution*8), mBitResolutionMode(AudioBitResolution)
{
  setupClient();
  setProcessCallback();
}


//*******************************************************************************
JackAudioInterface::~JackAudioInterface()
{
  delete[] mInputPacket;
  delete[] mOutputPacket;
}


//*******************************************************************************
void JackAudioInterface::setupClient()
{
  // \todo Get this name from global variable
  const char* client_name = "PaulTrip";//APP_NAME;
  const char* server_name = NULL;
  jack_options_t options = JackNoStartServer;
  jack_status_t status;

  // Try to connect to the server
  /// \todo Write better warning messages. This following line displays very
  /// verbose message, check how to desable them.
  mClient = jack_client_open (client_name, options, &status, server_name);
  if (mClient == NULL) {
    fprintf (stderr, "jack_client_open() failed, "
    	     "status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      fprintf (stderr, "Unable to connect to JACK server\n");
      std::cerr << "ERROR: Maybe the JACK server is not running?" << std::endl;
      std::cerr << SEPARATOR << std::endl;
    }
    std::exit(1);
  }
  if (status & JackServerStarted) {
    fprintf (stderr, "JACK server started\n");
  }
  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(mClient);
    fprintf (stderr, "unique name `%s' assigned\n", client_name);
  }

  // Set function to call if Jack shuts down
  jack_on_shutdown (mClient, this->jackShutdown, 0);

  // Create input and output channels
  createChannels();

  // Allocate buffer memory to read and write
  mSizeInBytesPerChannel = getSizeInBytesPerChannel();
  int size_input  = mSizeInBytesPerChannel * getNumInputChannels();
  int size_output = mSizeInBytesPerChannel * getNumOutputChannels();
  mInputPacket = new int8_t[size_input];
  mOutputPacket = new int8_t[size_output];

  /// \todo inizialize this in a better place
  mNumFrames = getBufferSize(); 
}


//*******************************************************************************
void JackAudioInterface::createChannels()
{
  //Create Input Ports
  mInPorts.resize(mNumInChans);
  for (int i = 0; i < mNumInChans; i++) 
    {
      QString inName;
      QTextStream (&inName) << "send_" << i+1;
      mInPorts[i] = jack_port_register (mClient, inName.toLatin1(),
					JACK_DEFAULT_AUDIO_TYPE,
					JackPortIsInput, 0);
    }

  //Create Output Ports
  mOutPorts.resize(mNumOutChans);
  for (int i = 0; i < mNumInChans; i++) 
    {
      QString outName;
      QTextStream (&outName) << "receive_" << i+1;
      mOutPorts[i] = jack_port_register (mClient, outName.toLatin1(),
					JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsOutput, 0);
    }
  
  /// \todo Put this in a better place
  mInBuffer.resize(mNumInChans);
  mOutBuffer.resize(mNumOutChans);
}


//*******************************************************************************
uint32_t JackAudioInterface::getSampleRate() const 
{
  return jack_get_sample_rate(mClient);
}


//*******************************************************************************
uint32_t JackAudioInterface::getBufferSize() const 
{
  return jack_get_buffer_size(mClient);
}


//*******************************************************************************
int JackAudioInterface::getAudioBitResolution() const
{
  return mAudioBitResolution;
}


//*******************************************************************************
int JackAudioInterface::getNumInputChannels() const
{
  return mNumInChans;
}


//*******************************************************************************
int JackAudioInterface::getNumOutputChannels() const
{
  return mNumOutChans;
}


//*******************************************************************************
size_t JackAudioInterface::getSizeInBytesPerChannel() const
{
  return (getBufferSize() * getAudioBitResolution()/8);
}

//*******************************************************************************
int JackAudioInterface::setProcessCallback()
{
  std::cout << "Setting JACK Process Callback..." << std::endl;
  if ( int code = 
       jack_set_process_callback(mClient, JackAudioInterface::wrapperProcessCallback, this)
       )
    {
      std::cerr << "Could not set the process callback" << std::endl;
      return(code);
      std::exit(1);
    }
  std::cout << "SUCCESS" << std::endl;
  std::cout << SEPARATOR << std::endl;
  return(0);
}


//*******************************************************************************
int JackAudioInterface::startProcess() const
{
  //Tell the JACK server that we are ready to roll.  Our
  //process() callback will start running now.
  if ( int code = (jack_activate(mClient)) ) 
    {
      std::cerr << "Cannot activate client" << std::endl;
    return(code);
    }
  return(0);
}


//*******************************************************************************
int JackAudioInterface::stopProcess() const
{
  if ( int code = (jack_client_close(mClient)) )
    {
      std::cerr << "Cannot disconnect client" << std::endl;
      return(code);
    }
  return(0);
}


//*******************************************************************************
void JackAudioInterface::jackShutdown (void*)
{
  std::cout << "The Jack Server was shut down!" << std::endl;
  std::cout << "Exiting program..." << std::endl;
  std::exit(1);
}


//*******************************************************************************
void JackAudioInterface::setRingBuffers
(const std::tr1::shared_ptr<RingBuffer> InRingBuffer,
 const std::tr1::shared_ptr<RingBuffer> OutRingBuffer)
{
  mInRingBuffer = InRingBuffer;
  mOutRingBuffer = OutRingBuffer;
}


//*******************************************************************************
// Before sending and reading to Jack, we have to round to the sample resolution
// that the program is using. Jack uses 32 bits (gJackBitResolution in globals.h)
// by default
void JackAudioInterface::computeNetworkProcessFromNetwork()
{
  /// \todo cast *mInBuffer[i] to the bit resolution
  //cout << mNumFrames << endl;
  // Output Process (from NETWORK to JACK)
  // ----------------------------------------------------------------
  // Read Audio buffer from RingBuffer (read from incoming packets)
  mOutRingBuffer->readSlotNonBlocking(  mOutputPacket );
  // Extract separate channels to send to Jack
  for (int i = 0; i < mNumOutChans; i++) {
    //--------
    // This should be faster for 32 bits
    //std::memcpy(mOutBuffer[i], &mOutputPacket[i*mSizeInBytesPerChannel],
    //		mSizeInBytesPerChannel);
    //--------
    sample_t* tmp_sample = mOutBuffer[i]; //sample buffer for channel i
    for (int j = 0; j < mNumFrames; j++) {
      //std::memcpy(&tmp_sample[j], &mOutputPacket[(i*mSizeInBytesPerChannel) + (j*4)], 4);
      // Change the bit resolution on each sample
      //cout << tmp_sample[j] << endl;
      fromBitToSampleConversion(&mOutputPacket[(i*mSizeInBytesPerChannel) 
					       + (j*mBitResolutionMode)],
				&tmp_sample[j],
				mBitResolutionMode);
    }
  }
}


//*******************************************************************************
void JackAudioInterface::computeNetworkProcessToNetwork()
{
  // Input Process (from JACK to NETWORK)
  // ----------------------------------------------------------------
  // Concatenate  all the channels from jack to form packet
  for (int i = 0; i < mNumInChans; i++) {  
    //--------
    // This should be faster for 32 bits
    //std::memcpy(&mInputPacket[i*mSizeInBytesPerChannel], mInBuffer[i],
    //		mSizeInBytesPerChannel);
    //--------
    sample_t* tmp_sample = mInBuffer[i]; //sample buffer for channel i
    for (int j = 0; j < mNumFrames; j++) {
      //std::memcpy(&tmp_sample[j], &mOutputPacket[(i*mSizeInBytesPerChannel) + (j*4)], 4);
      // Change the bit resolution on each sample
      fromSampleToBitConversion(&tmp_sample[j],
				&mInputPacket[(i*mSizeInBytesPerChannel)
					      + (j*mBitResolutionMode)],
				mBitResolutionMode);
    }
  }
  // Send Audio buffer to RingBuffer (these goes out as outgoing packets)
  mInRingBuffer->insertSlotNonBlocking( mInputPacket );
}


//*******************************************************************************
int JackAudioInterface::processCallback(jack_nframes_t nframes)
{
  // Get input and output buffers from JACK
  //-------------------------------------------------------------------
  for (int i = 0; i < mNumInChans; i++) {
    mInBuffer[i] = (sample_t*) jack_port_get_buffer(mInPorts[i], nframes);
  }
  for (int i = 0; i < mNumOutChans; i++) {
    mOutBuffer[i] = (sample_t*) jack_port_get_buffer(mOutPorts[i], nframes);
  }
  //-------------------------------------------------------------------
  // TEST: Loopback
  // To test, uncomment and send audio to client input. The same audio
  // should come out as output in the first channel
  //memcpy (mOutBuffer[0], mInBuffer[0], sizeof(sample_t) * nframes);
  //-------------------------------------------------------------------

  // Allocate the Process Callback
  //-------------------------------------------------------------------
  // 1) First, process incoming packets
  computeNetworkProcessFromNetwork();

  // 2) Dynamically allocate ProcessPlugin processes
  // The processing will be done in order of allocation
  for (int i = 0; i < mProcessPlugins.size(); ++i) {
    mProcessPlugins[i]->compute(nframes, mOutBuffer.data(), mInBuffer.data());
  }

  // 3) Finally, send packets to peer
  computeNetworkProcessToNetwork();

  return 0;
}


//*******************************************************************************
int JackAudioInterface::wrapperProcessCallback(jack_nframes_t nframes, void *arg) 
{
  return static_cast<JackAudioInterface*>(arg)->processCallback(nframes);
}


//*******************************************************************************
void JackAudioInterface::fromSampleToBitConversion(const sample_t* const input,
						   int8_t* output,
						   const audioBitResolutionT targetBitResolution)
{
  int8_t tmp_8;
  uint8_t tmp_u8;
  int16_t tmp_16;
  sample_t tmp_sample;
  sample_t tmp_sample16;
  sample_t tmp_sample8;
  switch (targetBitResolution)
    {
    case BIT8 : 
      tmp_sample = floor( (*input) * 128.0 ); // 2^7 = 128.0
      tmp_8 = static_cast<int8_t>(tmp_sample);
      std::memcpy(output, &tmp_8, 1); // 8bits = 1 bytes
      break;
    case BIT16 :
      tmp_sample = floor( (*input) * 32768.0 ); // 2^15 = 32768.0
      tmp_16 = static_cast<int16_t>(tmp_sample);
      std::memcpy(output, &tmp_16, 2); // 16bits = 2 bytes
      break;
    case BIT24 :
      tmp_sample  = floor( (*input) * 8388608.0 ); // 2^23 = 8388608.0 24bit number
      tmp_sample16 = tmp_sample / 256.0;   // tmp_sample/(2^8) = 2^15
      tmp_16 = static_cast<int16_t>(tmp_sample16);


      tmp_sample8 = tmp_sample / tmp_sample16;  // tmp_sample/(2^15) = 32768



      tmp_8 = static_cast<int8_t>(tmp_sample8);
      std::memcpy(output, &tmp_16, 2); // 16bits = 2 bytes
      std::memcpy(output+2, &tmp_8, 1); // 16bits = 2 bytes
      break;
    case BIT32 :
      std::memcpy(output, input, 4); // 32bit = 4 bytes
      break;
    }
}


//*******************************************************************************
void JackAudioInterface::fromBitToSampleConversion(const int8_t* const input,
						   sample_t* output,
						   const audioBitResolutionT sourceBitResolution)
{
  int8_t tmp_8;
  uint8_t tmp_u8;
  int16_t tmp_16;
  sample_t tmp_sample;
  sample_t tmp_sample16;
  sample_t tmp_sample8;
  switch (sourceBitResolution)
    {
    case BIT8 : 
      tmp_8 = *input;
      tmp_sample = static_cast<sample_t>(tmp_8) / 128.0;
      std::memcpy(output, &tmp_sample, 4); // 4 bytes
      break;
    case BIT16 :
      tmp_16 = *( reinterpret_cast<const int16_t*>(input) ); // *((int16_t*) input);
      tmp_sample = static_cast<sample_t>(tmp_16) / 32768.0;
      std::memcpy(output, &tmp_sample, 4); // 4 bytes
      break;
    case BIT24 :
      tmp_16 = *( reinterpret_cast<const int16_t*>(input) );
      tmp_8 = *(input+2);
      //std::memcpy(&tmp_16, input, 2);
      //std::memcpy(&tmp_8, input+2, 1);
      tmp_sample16 = static_cast<sample_t>(tmp_16) / 32768.0;
      tmp_sample8 = static_cast<sample_t>(tmp_8) / 128.0;
      tmp_sample = tmp_sample16 * tmp_sample8;
      //tmp_sample = ( (static_cast<sample_t>(tmp_16)) * (static_cast<sample_t>(tmp_8)) ) /
      //8388608.0;
      //cout << tmp_sample << endl;
       std::memcpy(output, &tmp_sample, 4); // 4 bytes
      break;
    case BIT32 :
      std::memcpy(output, input, 4); // 4 bytes
      break;
    }
}


//*******************************************************************************
void JackAudioInterface::appendProcessPlugin(const std::tr1::shared_ptr<ProcessPlugin> plugin)
{
  /// \todo check that channels in ProcessPlugins are less or same that jack channels
  if ( plugin->getNumInputs() ) {
  }
  mProcessPlugins.append(plugin);
}
