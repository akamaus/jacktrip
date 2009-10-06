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
 * \file JackTripWorker.cpp
 * \author Juan-Pablo Caceres
 * \date September 2008
 */

#include <iostream>
#include <unistd.h>

#include <QTimer>
#include <QMutexLocker>
#include <QWaitCondition>

#include "JackTripWorker.h"
#include "JackTrip.h"
#include "UdpMasterListener.h"
#include "NetKS.h"
#include "LoopBack.h"
#ifdef __JAMTEST__
#include "JamTest.h"
#endif

using std::cout; using std::endl;

//*******************************************************************************
JackTripWorker::JackTripWorker(UdpMasterListener* udpmasterlistener) :
  mUdpMasterListener(NULL),
  mSpawning(false),
  mID(0),
  mNumChans(1)
{
  /* From the QT Documentation:
  QThreadPool supports executing the same QRunnable more than once 
  by calling tryStart(this) from within QRunnable::run(). If autoDelete is 
  enabled the QRunnable will be deleted when the last thread exits the 
  run function. Calling start() multiple times with the same QRunnable 
  when autoDelete is enabled creates a race condition and is not recommended.
  */
  mUdpMasterListener = udpmasterlistener;
  setAutoDelete(false); // stick around after calling run()
  //mNetks = new NetKS;
  //mNetks->play();
}


//*******************************************************************************
JackTripWorker::~JackTripWorker()
{
  //delete mUdpMasterListener;
}


//*******************************************************************************
void JackTripWorker::setJackTrip(int id, uint32_t client_address,
				 uint16_t server_port, uint16_t client_port,
				 int num_channels)
{
  { //Start Spawning, so lock mSpawning
    QMutexLocker locker(&mMutex);
    mSpawning = true;
  }
  mID = id;
  // Set the jacktrip address and ports
  //mClientAddress.setAddress(client_address);
  mClientAddress = client_address;
  mServerPort = server_port;
  mClientPort = client_port;
  mNumChans = num_channels;
}


//*******************************************************************************
void JackTripWorker::run()
{
  /* NOTE: This is the message that qt prints when an exception is thrown:
    'Qt Concurrent has caught an exception thrown from a worker thread.
    This is not supported, exceptions thrown in worker threads must be
    caught before control returns to Qt Concurrent.'*/

  QHostAddress ClientAddress;

  // Try catching any exceptions that come from JackTrip
  try 
  {
    // Local event loop. this is necesary because QRunnables don't have their own as QThreads
    QEventLoop event_loop;

    // Create and setup JackTrip Object
    //JackTrip jacktrip(JackTrip::SERVER, JackTrip::UDP, mNumChans, 2);
#ifndef __JAMTEST__
    JackTrip jacktrip(JackTrip::SERVER, JackTrip::UDP, mNumChans, 2);
#endif
#ifdef __JAMTEST__
    //JackTrip jacktrip(JackTrip::SERVER, JackTrip::UDP, mNumChans, 2);
    JamTest jacktrip(JackTrip::SERVER); // ########### JamTest #################
#endif

    ClientAddress.setAddress(mClientAddress);
    // If I don't type this line, I get a bus error in the next line.
    // I still haven't figure out why
    ClientAddress.toString().toLatin1().constData();
    jacktrip.setPeerAddress(ClientAddress.toString().toLatin1().constData());
    jacktrip.setBindPorts(mServerPort);
    //jacktrip.setPeerPorts(mClientPort);

    int PeerConnectionMode = setJackTripFromClientHeader(jacktrip);

    // Connect signals and slots
    // -------------------------
    // Connection to terminate JackTrip when packets haven't arrive for
    // a certain amount of time
    QObject::connect(&jacktrip, SIGNAL(signalNoUdpPacketsForSeconds()),
                     &jacktrip, SLOT(slotStopProcesses()), Qt::QueuedConnection);
    // Connection to terminate the local eventloop when jacktrip is done
    QObject::connect(&jacktrip, SIGNAL(signalProcessesStopped()),
                     &event_loop, SLOT(quit()), Qt::QueuedConnection);
    QObject::connect(this, SIGNAL(signalRemoveThread()),
                     &jacktrip, SLOT(slotStopProcesses()), Qt::QueuedConnection);

    // Start Threads and event loop
    jacktrip.startProcess();
    //jacktrip.start(); // ########### JamTest Only #################

    { // Thread is already spawning, so release the lock
      QMutexLocker locker(&mMutex);
      mSpawning = false;
    }

    event_loop.exec(); // Excecution will block here until exit() the QEventLoop
    //--------------------------------------------------------------------------

    // wait for jacktrip to be done before exiting the Worker Thread
    jacktrip.wait();

  }
  catch ( const std::exception & e )
  {
    std::cerr << "Couldn't send thread to the Pool" << endl;
    std::cerr << e.what() << endl;
    std::cerr << gPrintSeparator << endl;
  }
  
  mUdpMasterListener->releaseThread(mID);

  cout << "JackTrip ID = " << mID << " released from the THREAD POOL" << endl;
  cout << gPrintSeparator << endl;
  { 
    // Thread is already spawning, so release the lock
    QMutexLocker locker(&mMutex);
    mSpawning = false;
  }
}


//*******************************************************************************
int JackTripWorker::setJackTripFromClientHeader(JackTrip& jacktrip)
{
  //QHostAddress peerHostAddress;
  //uint16_t peer_port;
  QUdpSocket UdpSockTemp;// Create socket to wait for client

  // Bind the socket
  if ( !UdpSockTemp.bind(QHostAddress::Any, mServerPort,
                         QUdpSocket::DefaultForPlatform) )
  {
    throw std::runtime_error("Could not bind UDP socket. It may be already binded.");
  }

  // Listen to client
  QWaitCondition sleep;
  QMutex mutex; mutex.lock();
  while ( !UdpSockTemp.hasPendingDatagrams() ) { sleep.wait(&mutex,100); }
  mutex.unlock();
  int packet_size = UdpSockTemp.pendingDatagramSize();
  char packet[packet_size];
  UdpSockTemp.readDatagram(packet, packet_size);
  UdpSockTemp.close(); // close the socket
  int8_t* full_packet = reinterpret_cast<int8_t*>(packet);

  int PeerBufferSize = jacktrip.getPeerBufferSize(full_packet);
  int PeerSamplingRate = jacktrip.getPeerSamplingRate(full_packet);
  int PeerBitResolution = jacktrip.getPeerBitResolution(full_packet);
  int PeerNumChannels = jacktrip.getPeerNumChannels(full_packet);
  int PeerConnectionMode = jacktrip.getPeerConnectionMode(full_packet);

  cout << "getPeerBufferSize = " << PeerBufferSize << endl;
  cout << "getPeerSamplingRate = " << PeerSamplingRate << endl;
  cout << "getPeerBitResolution = " << PeerBitResolution << endl;
  cout << "getPeerNumChannels = " << PeerNumChannels << endl;
  cout << "getPeerConnectionMode = " << PeerConnectionMode << endl;

  jacktrip.setNumChannels(PeerNumChannels);
  return PeerConnectionMode;
}


//*******************************************************************************
bool JackTripWorker::isSpawning()
{
  QMutexLocker locker(&mMutex);
  return mSpawning;
}


//*******************************************************************************
void JackTripWorker::stopThread()
{
  emit signalRemoveThread();
}
