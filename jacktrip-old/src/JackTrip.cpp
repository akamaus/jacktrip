/*
  JackTrip: A Multimachine System for High-Quality Audio 
  Network Performance over the Internet

  Copyright (c) 2008 Chris Chafe, Juan-Pablo Caceres,
  SoundWIRE group at CCRMA.
  
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

/*
 * JackTrip.cpp
 */

//#include "JackTrip.h"
#include "AudioDevice.h"

using namespace std;

extern QString *
IPv4Addr (char *namebuf);


//---------------------------------------------------------------------------------------------
/*! \brief Returns fifo priority for streambd.
 *
 */
//---------------------------------------------------------------------------------------------
int
get_fifo_priority (bool half)
{
  int min, max, priority;

  min = sched_get_priority_min (SCHED_FIFO);
  max = sched_get_priority_max (SCHED_FIFO);
  if (half) priority = (max  - (max - min) / 2);
  else
    priority = max; 
  //priority=min;
  return priority;
}


//---------------------------------------------------------------------------------------------
/*! \brief Set fifo priority (if user has sufficient privileges).
 *
 */
//---------------------------------------------------------------------------------------------
int
set_fifo_priority (bool half)
{
  struct sched_param p;
  int priority;
  //  scheduling priority


  if (true) // (!getuid () || !geteuid ())
    {
      priority = get_fifo_priority (half);
      p.sched_priority = priority;

      if (sched_setscheduler (0, SCHED_FIFO, &p) == -1)
	{
	  fprintf (stderr,
		   "\ncould not activate scheduling with priority %d\n",
		   priority);
	  return -1;
	}
      seteuid (getuid ());
      fprintf (stderr,
	       "\nset scheduling priority to %d (SCHED_FIFO)\n",
	       priority);
    }
  else
    {
      fprintf (stderr,
	       "\ninsufficient privileges to set scheduling priority\n");
      priority = 0;
    }
  return priority;
}


//---------------------------------------------------------------------------------------------
/*! \brief
 */
//---------------------------------------------------------------------------------------------
JackTrip::JackTrip()
{
  args = new cmdLineArgs;

}


//---------------------------------------------------------------------------------------------
/*! \brief
 */
//---------------------------------------------------------------------------------------------
JackTrip::~JackTrip()
{
  // TODO: put destructor code here
}


//---------------------------------------------------------------------------------------------
/*! \brief
 */
//---------------------------------------------------------------------------------------------
int
JackTrip::cmd (MainDialog* eventThread)
{
  // Get the local host address
  QString *localhostName = GetLocalHostName ();

  clock_t *lastTickTime = NULL;
  if (false)
    {			// change to cmd line option
      lastTickTime = new clock_t ();
      *(lastTickTime) = clock ();
    }

  AudioInfoT audioInfo = new AudioInfo (args->sampleRate, sizeof (INT16),
					args->framesPerAudioBuffer,
					args->audioChannels,
					args->netHarpStrings,
					args->secondsBetweenPlucks,
					args->jack, lastTickTime,
					args->jack_alsa_readable_offset );

  NetworkInfoT netInfo = new NetworkInfo (*localhostName,
					  audioInfo->
					  getBytesPerBuffer (),
					  UDP_IN_PORT +
					  args->networkPortOffset,
					  UDP_OUT_PORT +
					  args->networkPortOffset,
					  args->redundancy,
					  args->networkInputQueueLengthInPackets);

  //   AudioDeviceT audioDevice;

  cout << "Creating Streams................";
  int networkInputQueueLengthInAudioBuffers = 
    args->networkInputQueueLengthInPackets * args->redundancy;
  t.streamin =
    new Stream (audioInfo, netInfo,
		networkInputQueueLengthInAudioBuffers,
		false);
  t.streamout =
    new Stream (audioInfo, netInfo, DEFAULT_OUTPUT_STREAM_QUEUE_LENGTH,
		true);

  if (t.streamin != NULL && t.streamout != NULL)
    {
      cout << "SUCCESS!" << endl;
    }
  else
    {
      cout << "FAILED!" << endl;
      return -1;
    }

  switch (args->runMode)
    {
    case AUDIOINFO:
      audioDevice = new AudioDevice (true);
      exit (0);
      
    default:
      audioDevice = new AudioDevice (args->audioDeviceID,
				     args->
				     audioInputQueueLengthInAudioBuffers,
				     AudioDevice::DUPLEX,
				     audioInfo);

      t.audioin = new AudioInput (audioDevice, audioInfo);
      t.audioout = new AudioOutput (audioDevice, audioInfo);
	
      audioDevice->setThreads(t);

      t.netin = new UDPInput (netInfo, audioInfo);
      //t.netin->setGUI((QObject *)eventThread);
      t.netout = new UDPOutput (netInfo, audioInfo);
      //t.netout->setGUI((QObject *)eventThread);

      ConnectStreamPlugins (t.audioin, t.netout, t.streamout);
      ConnectStreamPlugins (t.netin, t.audioout, t.streamin);

    }

  if (args->runFifo)
    set_fifo_priority (true);

  cout << "Running 0" << endl;
  // TRANSMIT, RECEIVE, and HARPT modes all use streamin
  // The other modes only use the unbuffered streamout.
  if (args->runMode == TRANSMIT || args->runMode == RECEIVE
      || args->runMode == HARPT)
    {
      t.streamin->startThreads ();
    } 
  cout << "Running 1" << endl;

  if (args->runMode == NETMIRROR || args->runMode == HARPR)
    t.netin->start ();	// needs to run to
  // listen for peer address
  cout << "Running 2" << endl;

  // All modes except for SNDMIRROR use netin and netout
  // and therefore need to have a network connection established.
  if (args->runMode != SNDMIRROR)
    EstablishConnection (args->runMode, args->remoteHostname,
			 t.netout, t.netin);
  cout << "Running 3" << endl;

  if (args->runMode != HARPT)
    t.streamout->startThreads ();
  cout << "Running 4" << endl;

  if (args->jack && (args->runMode != NETMIRROR || args->runMode == HARPR))
    audioDevice->jackStart ();

  cout << "Running " << endl;
  return 0;
}


//---------------------------------------------------------------------------------------------
/*! \brief
 */
//---------------------------------------------------------------------------------------------
void
JackTrip::start ()
{
}


//---------------------------------------------------------------------------------------------
/*! \brief
 */
//---------------------------------------------------------------------------------------------
void
JackTrip::finish ()
{
  cout << "Finishing" << endl;
  if (args->jack)
    audioDevice->jackStop ();
  if (t.streamin->threadsRunning)
    t.streamin->stopThreads ();
  cout << "stopped streamin threads" << endl;
  if (t.streamout->threadsRunning)
    t.streamout->stopThreads ();
  cout << "stopped streamout threads" << endl;	
}


//---------------------------------------------------------------------------------------------
/*! \brief
 */
//---------------------------------------------------------------------------------------------
void
JackTrip::go ()
/* cause run loop to start */
{
  start ();
}


//---------------------------------------------------------------------------------------------
/*! \brief
 */
//---------------------------------------------------------------------------------------------
void
JackTrip::stop ()
/* cause run loop to finish */
{
  //	loop = false; not a thread
  finish();
}


//---------------------------------------------------------------------------------------------
/*! \brief
 */
//---------------------------------------------------------------------------------------------
void
JackTrip::join ()
/* wait for thread to exit */
{
  //	wait (); not a thread
}


//---------------------------------------------------------------------------------------------
/*! \brief PrintUsage()
 *
 * Print all of the command line arguments and their default values
 */
//---------------------------------------------------------------------------------------------
void
JackTrip::PrintUsage ()
{
  cout << "===========================================================================" << endl;
  cout << "   jacktrip	A project of the SoundWIRE group at CCRMA, Stanford." << endl;
  cout << " 		http://ccrma.stanford.edu/groups/soundwire/software/" << endl << endl;
  cout << "===========================================================================" << endl;
  cout << " Needs to be run in one of the following modes:" << endl;
  cout << "	-r		Receiver	(opens audio channels and waits" << endl;
  cout << "				 	for a connection from a transmitter)" << endl;
  cout << "	-t HOSTNAME	Transmitter 	(connects to a receiver on HOSTNAME)" << endl;
  //cout << "	-n		NetMirror 	(A receiver that reflects back all" << endl;
  //cout << "				 	audio sent to it by a transmitter." << endl;
  //cout << "					No local audio.)" <<
  //endl;
  //cout << "	-s		SndMirror 	(Routes sound input to output." << endl;
  //cout << "					No networking.)" <<
  //  endl;
  //cout << "	-h HOSTNAME	HarpTransmitter	(uses network as harp strings.)" << endl;
  //cout << "	-p		HarpReceiver	(uses network as harp strings.)" << endl;
  cout << "	-i		Info		(Get audio device info.)" << endl;
  cout << "---------------------------------------------------------------------------" << endl;
  cout << " Optional parameters are as follows:				(default)" << endl;
  cout << "	-z 	Number of audio frames per audio buffer.	(" << args->framesPerAudioBuffer << ")" << endl;
  cout << "	-b 	Number of audio buffers in network packet (redundancy).	(" << args->redundancy << ")" << endl;
  cout << "	-q 	Number of packets in input queue.	(" << args->networkInputQueueLengthInPackets << ")" << endl;
  cout << "	-B 	Number of audio buffers in audio input queue.	(" << args->audioInputQueueLengthInAudioBuffers << ")" << endl;
  //cout << "	-c 	Number of NetHarp strings.			(" << args->netHarpStrings << ")" << endl;
  cout << "	-a 	Number of audio channels to use.		(" << args->audioChannels << ")" << endl;
  //cout << "	-D 	Audio device to use if not default.		(" << args->audioDeviceID << ")" << endl;
  cout << "	-o 	Network port offset.				(" << args->networkPortOffset << ")" << endl;
  cout << "	-f 	Run fifo (scheduling priority).			(" << args->runFifo << ")" << endl;
  //cout << "	-P 	Seconds between Netharp plucks.			(" << args->secondsBetweenPlucks << ")" << endl;
  //cout << "	-l 	Integer % low pass filter coefficient.		(" << args->lowPassFilterCoeff << ") 0 = off" << endl;
  //cout << "	-d 	Delay increment between strings (msec.) 	(" << args->delayIncrementBetweenStrings << ") 0 = off" << endl;
  cout << "	-j		jack 	(" << args->jack << ")" << endl;
  cout << "	-J		jack alsa readable offset	(" << args->jack_alsa_readable_offset << ")" << endl;
  //cout << "	-g		gui	(" << args->gui << ")" << endl;
  cout << endl;
  cout << "	run './jacktrip' to see these instructions." <<
    endl;
}


//---------------------------------------------------------------------------------------------
/*! \brief Parse the command line.
 *
 * Use default values for all cmdLineArgs entries
 * unless explicitly assigned values on the commandline.
 */
//---------------------------------------------------------------------------------------------
int
JackTrip::ParseCommandLine (int argc, char *argv[])
{

  // Set Default Values
  args->sampleRate = DEFAULT_SAMPLE_RATE;
  args->netHarpStrings = DEFAULT_NETHARP_STRINGS;
  args->audioChannels = DEFAULT_AUDIO_CHANNELS;
  args->runFifo = DEFAULT_FIFO;
  args->audioDeviceID = DEFAULT_AUDIO_DEVICE;
  args->networkInputQueueLengthInPackets =
    DEFAULT_NETWORK_QUEUE_LENGTH;
  args->audioInputQueueLengthInAudioBuffers =
    DEFAULT_AUDIO_QUEUE_LENGTH;
  args->framesPerAudioBuffer = DEFAULT_AUDIO_BUFFER_SIZE;
  args->networkPortOffset = DEFAULT_NETWORK_PORT_OFFSET;
  args->runMode = RECEIVE;
  args->secondsBetweenPlucks = DEFAULT_SECONDS_BETWEEN_PLUCKS;
  args->lowPassFilterCoeff = DEFAULT_LOW_PASS_FILTER_COEFFICIENT;
  args->delayIncrementBetweenStrings = DEFAULT_DELAY_LINE_COEFFICIENT;
  args->jack = DEFAULT_JACK;
  args->gui = DEFAULT_GUI;
  args->redundancy = DEFAULT_PACKET_REDUNDANCY;
  args->jack_alsa_readable_offset = DEFAULT_JACK_ALSA_READABLE_OFFSET;

  // If no command arguments are given, print instructions
  if (argc <= 1)
    {
      PrintUsage ();
      return 0;
    }

  // Override defaults if expicitly specified on the command line
  char ch;
  while ((ch =
	  getopt (argc, argv,
		  "trnsjghpifd:o:c:z:b:B:D:l:e:a:P:q:J:")) != EOF)
    switch (ch)
      {
      case 'd':
	args->delayIncrementBetweenStrings = atoi (optarg);
	break;
      case 'l':
	args->lowPassFilterCoeff = (float) atoi (optarg);
	break;
      case 'z':
	args->framesPerAudioBuffer = atoi (optarg);
	break;
      case 'q':
	args->networkInputQueueLengthInPackets =
	  atoi (optarg);
	break;
      case 'b':
	args->redundancy =
	  atoi (optarg);
	break;
      case 'B':
	args->audioInputQueueLengthInAudioBuffers =
	  atoi (optarg);
	break;
      case 'c':
	args->netHarpStrings = atoi (optarg);
	break;
      case 'a':
	args->audioChannels = atoi (optarg);
	break;
      case 'D':
	args->audioDeviceID = atoi (optarg);
	break;
      case 'o':
	args->networkPortOffset = atoi (optarg);
	break;
      case 's':
	args->runMode = SNDMIRROR;
	break;
      case 'j':
	args->jack = true;
	break;
      case 'J':
	args->jack_alsa_readable_offset = atoi (optarg);
	break;
      case 'g':
	args->gui = true;
	break;
      case 'n':
	args->runMode = NETMIRROR;
	break;
      case 't':
	args->runMode = TRANSMIT;
	break;
      case 'r':
	args->runMode = RECEIVE;
	break;
      case 'h':
	args->runMode = HARPT;
	break;
      case 'p':
	args->runMode = HARPR;
	break;
      case 'i':
	args->runMode = AUDIOINFO;
	break;
      case 'f':
	args->runFifo = 1;
	break;
      case 'P':
	args->secondsBetweenPlucks = atoi (optarg);
	break;
      default:
	PrintUsage ();
	return 0;
      }
  argc -= optind;
  argv += optind;

  // In harp mode, the default number of netHarpStrings is audioChannels
  if (args->runMode == HARPR || args->runMode == HARPT)
    {
      if (args->netHarpStrings <= 0)
	args->netHarpStrings = args->audioChannels;
      cerr << "Running a NetHarp with " << args->
	netHarpStrings << " strings, plucked every " << args->
	secondsBetweenPlucks << " seconds." << endl;
    }

  // If we're in transmit or harpt mode, make sure we have
  // a hostname to connect to.
  if (args->runMode == TRANSMIT || args->runMode == HARPT)
    {
      if (argc > 0)
	{		/* Copy remote hostname */
	  strncpy (args->remoteHostname, argv[0], 99);
	  cout << "remote hostname is " << args->
	    remoteHostname << endl;
	}
      else
	{
	  cerr << "You must specify a remote hostname to run in transmit mode." << endl;
	  cerr << "Run streambd with no arguments for command line instructions." << endl;
	  return 0;
	}
    }
  return 1;
}


//---------------------------------------------------------------------------------------------
/*! \brief
 */
//---------------------------------------------------------------------------------------------
void
JackTrip::ConnectStreamPlugins (InputStreamPlugin * from, OutputStreamPlugin * to, Stream * through)
{
  from->setStream (through);
  to->setStream (through);

  through->addInput (from);
  through->addOutput (to);
}


//---------------------------------------------------------------------------------------------
/*! \brief
 */
//---------------------------------------------------------------------------------------------
void
JackTrip::addStreamPlugin (InputStreamPlugin * from, Stream * str)
{
  from->setStream (str);
  str->addInput (from);
}


//---------------------------------------------------------------------------------------------
/*! \brief
 */
//---------------------------------------------------------------------------------------------
void
JackTrip::addStreamPlugin (OutputStreamPlugin * to, Stream * str)
{
  to->setStream (str);
  str->addOutput (to);
}


//---------------------------------------------------------------------------------------------
/*! \brief Connects, or waits for connection depending on runMode.
 *
 * If transmit or harpt, connect to hostname.
 * If any other mode, wait for a peer, then connect back to that peer.
 */
//---------------------------------------------------------------------------------------------
void
JackTrip::EstablishConnection (runModeT runMode, char *hostname, UDPOutput * netout,
			       UDPInput * netin)
{
  if (runMode == TRANSMIT || runMode == HARPT)
    {
      cout << "Requesting Connection...........";
      QHostAddress *ha = new QHostAddress ();
      QString *s = IPv4Addr (hostname);	// dotted integer from name
      ha->setAddress (*s);
      //netout->connect (*ha);
      netout->setPeerAddress (*ha);
    }
  else
    {
      cout << endl << "Waiting for incoming connection." << endl;
      while (!netin->hasPeer ())
	{
	  usleep (10000);
	  //cout << ".";
	}
      cout << endl << "Connection received from: " <<
	netin->peer().toString().toStdString() << endl;
      cout << "Requesting return connection....";
      //netout->connect (netin->peer ());
      netout->setPeerAddress (netin->peer ());
      cout << "SUCCESS!" << endl;
    }
}



//---------------------------------------------------------------------------------------------
/*! \brief Lookup the hostname of the local machine.
 *
 */
//---------------------------------------------------------------------------------------------
QString *
JackTrip::GetLocalHostName ()
{
  char localhostbuf[100];
  if (gethostname (localhostbuf, 99))
    {
      perror ("gethostname");
      exit (-1);
    }
  QString *s = new QString (localhostbuf);
  return (s);
}

#include <sched.h>
int
set_realtime_priority (void)
{
  struct sched_param schp;

  memset (&schp, 0, sizeof (schp));
  schp.sched_priority = sched_get_priority_max (SCHED_FIFO);
  if (sched_setscheduler (0, SCHED_FIFO, &schp) != 0)
    {
      perror ("set_scheduler");
      return -1;
    }
  return 0;
}
