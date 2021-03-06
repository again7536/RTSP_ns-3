/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
#include "rtsp-server.h"
#include "seq-ts-header.h"

#include <string>
#include <fstream>
#include <vector>

#include <sstream>
#include <ns3/core-module.h>
#include <ns3/log.h>
#include <ns3/simulator.h>
#include <ns3/callback.h>
#include <ns3/config.h>
#include <ns3/pointer.h>
#include <ns3/uinteger.h>
#include <ns3/three-gpp-http-variables.h>
#include <ns3/packet.h>
#include <ns3/socket.h>
#include <ns3/tcp-socket.h>
#include <ns3/tcp-socket-factory.h>
#include <ns3/udp-socket-factory.h>
#include <ns3/inet-socket-address.h>
#include <ns3/inet6-socket-address.h>
#include <ns3/unused.h>

NS_LOG_COMPONENT_DEFINE("RtspServer");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(RtspServer);

TypeId
RtspServer::GetTypeId (void) 
{
    static TypeId tid = TypeId("ns3::RtspServer")
        .SetParent<Application> ()
        .SetGroupName("Applications")
        .AddConstructor<RtspServer>()
        .AddAttribute ("LocalAddress",
                    "The local address of the server, "
                    "i.e., the address on which to bind the Rx socket.",
                    AddressValue (),
                    MakeAddressAccessor (&RtspServer::m_localAddress),
                    MakeAddressChecker ())
        .AddAttribute ("RtspPort",
                    "Rtsp socket port.",
                    UintegerValue (9),
                    MakeUintegerAccessor (&RtspServer::m_rtspPort),
                    MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("RtcpPort",
                    "Rtcp socket port.",
                    UintegerValue (10),
                    MakeUintegerAccessor (&RtspServer::m_rtcpPort),
                    MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("RtpPort",
                    "Rtp socket port.",
                    UintegerValue (11),
                    MakeUintegerAccessor (&RtspServer::m_rtpPort),
                    MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("SendDelay",
                    "Frame send delay.",
                    UintegerValue (RtspServer::FRAME_PERIOD),
                    MakeUintegerAccessor (&RtspServer::m_sendDelay),
                    MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("UseCongestionThreshold",
                    "Enable or Disable congestion threshold.",
                    BooleanValue(&RtspServer::m_useCongestionThreshold),
                    MakeBooleanAccessor (&RtspServer::m_useCongestionThreshold),
                    MakeBooleanChecker ())
        .AddTraceSource ("CongestionLevel",
                    "Congestion Level",
                    MakeTraceSourceAccessor (&RtspServer::m_congestionLevelTrace),
                    "ns3::RtspServer::TracedCallback"
        )
    ;
    return tid;
}

RtspServer::RtspServer ()
{
    NS_LOG_FUNCTION (this);
    m_rtspPort = 9;
    m_rtcpPort = 10;
    m_rtpPort = 11;
    m_sendDelay = FRAME_PERIOD;

    m_congestionLevel = MAX_CONGESTION_LEVEL;
    m_congestionThreshold = MAX_CONGESTION_LEVEL + 1;
    m_useCongestionThreshold = true;
    
    m_frameSizes.resize(0);
    m_count = 0;
    m_seqNum = 0;
    m_upscale = 0;
}

RtspServer::~RtspServer ()
{
    NS_LOG_FUNCTION (this);
}

void
RtspServer::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  if (!Simulator::IsFinished ())
    {
      StopApplication ();
    }

  Application::DoDispose (); // Chain up.
}

void
RtspServer::StartApplication ()
{
    NS_LOG_FUNCTION (this);

    /*
      RTSP ?????? ?????????
    */
    if (m_rtspSocket == 0)
    {
      // Creating a TCP socket to connect to the server.
      m_rtspSocket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
      m_rtspSocket->SetAttribute("SegmentSize", UintegerValue(1500));
      if (Ipv4Address::IsMatchingType (m_localAddress))
      {   
          const Ipv4Address ipv4 = Ipv4Address::ConvertFrom (m_localAddress);
          const InetSocketAddress inetSocket = InetSocketAddress (ipv4, m_rtspPort);
          if (m_rtspSocket->Bind (inetSocket) == -1)
          {
            NS_FATAL_ERROR ("Failed to bind socket");
          }
      }
      m_rtspSocket->Listen ();
    }
    NS_ASSERT_MSG (m_rtspSocket != 0, "Failed creating RTSP socket.");
    m_rtspSocket->SetRecvCallback (MakeCallback (&RtspServer::HandleRtspReceive, this));
    m_rtspSocket->SetAcceptCallback (
      MakeCallback (&RtspServer::ConnectionRequestCallback, this),
      MakeCallback (&RtspServer::NewConnectionCreatedCallback, this)
    );

    /* 
      RTP ?????? ?????????
    */
    if (m_rtpSocket == 0)
    {
      m_rtpSocket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      const Ipv4Address ipv4 = Ipv4Address::ConvertFrom (m_localAddress);
      InetSocketAddress inetSocket = InetSocketAddress (ipv4, m_rtpPort);
      if (m_rtpSocket->Bind (inetSocket) == -1)
      {
        NS_FATAL_ERROR ("Failed to bind RTP socket");
      }
    }
    NS_ASSERT_MSG (m_rtpSocket != 0, "Failed creating RTP socket.");

    /* 
      RTCP ?????? ?????????
    */
    if (m_rtcpSocket == 0)
    {
      m_rtcpSocket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      const Ipv4Address ipv4 = Ipv4Address::ConvertFrom (m_localAddress);
      InetSocketAddress inetSocket = InetSocketAddress (ipv4, m_rtcpPort);
      if (m_rtcpSocket->Bind (inetSocket) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind RTCP socket");
        }
    }
    NS_ASSERT_MSG (m_rtcpSocket != 0, "Failed creating RTCP socket.");
    m_rtcpSocket->SetRecvCallback (MakeCallback (&RtspServer::HandleRtcpReceive, this));
} // end of `void StartApplication ()`

void
RtspServer::StopApplication ()
{
  NS_LOG_FUNCTION (this);
  Simulator::Cancel (m_sendEvent);
  m_state = INIT;

  // Stop listening.
  if (m_rtspSocket != 0)
    {
      m_rtspSocket->Close ();
      m_rtspSocket->SetAcceptCallback (MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
                                          MakeNullCallback<void, Ptr<Socket>, const Address &> ());
      m_rtspSocket->SetCloseCallbacks (MakeNullCallback<void, Ptr<Socket> > (),
                                          MakeNullCallback<void, Ptr<Socket> > ());
      m_rtspSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_rtspSocket->SetSendCallback (MakeNullCallback<void, Ptr<Socket>, uint32_t > ());
    }
  if (m_rtpSocket != 0) 
  {
    m_rtpSocket->Close();
    m_rtpSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
  }
}

bool
RtspServer::ConnectionRequestCallback (Ptr<Socket> socket, const Address &address)
{
  NS_LOG_FUNCTION (this << socket << address);
  return true; // Unconditionally accept the connection request.
}

void
RtspServer::NewConnectionCreatedCallback (Ptr<Socket> socket, const Address &address)
{
  NS_LOG_FUNCTION (this << socket << address);

  // socket->SetCloseCallbacks (MakeCallback (&ThreeGppHttpServer::NormalCloseCallback, this),
  //                            MakeCallback (&ThreeGppHttpServer::ErrorCloseCallback, this));
  socket->SetRecvCallback (MakeCallback (&RtspServer::HandleRtspReceive, this));

  InetSocketAddress inetSocket = InetSocketAddress::ConvertFrom(address);
  Ipv4Address ipv4 = Ipv4Address::ConvertFrom(inetSocket.GetIpv4());
  m_clientAddress = ipv4;
  inetSocket = InetSocketAddress(ipv4, m_rtpPort);
  m_rtpSocket->Connect (inetSocket);

  m_sendEvent = Simulator::Schedule(Seconds(0), &RtspServer::ScheduleRtpSend, this);
  /*
   * A typical connection is established after receiving an empty (i.e., no
   * data) TCP packet with ACK flag. The actual data will follow in a separate
   * packet after that and will be received by ReceivedDataCallback().
   *
   * However, that empty ACK packet might get lost. In this case, we may
   * receive the first data packet right here already, because it also counts
   * as a new connection. The statement below attempts to fetch the data from
   * that packet, if any.
   */
  HandleRtspReceive (socket);
}

//RTSP handler
void
RtspServer::HandleRtspReceive (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);

  Ptr<Packet> packet;
  while((packet = socket->Recv()))
  {
    if (packet->GetSize () == 0)
    {
      break;
    }
    //?????? ??????
    uint8_t* msg = new uint8_t[packet->GetSize()+1];
    packet->CopyData(msg, packet->GetSize());
    std::istringstream req((char*)msg);

    //?????? ??????
    std::ostringstream res;

    std::string method;
    req >> method;

    res << method << '\n';
    res << 200 << '\n';

    //SETUP??? ????????? ?????? ????????? ????????? ??????
    if (method.compare("SETUP") == 0) {
      m_state = READY;

      //?????? ???????????? ?????? ?????? ???????????? ?????? ?????? ???
      if(m_fileStream.is_open())
      {
        m_fileStream.close();
      }
      std::string fileName;
      req >> fileName;
      m_fileStream.open(fileName);

      res << FRAME_PERIOD << '\n';

      NS_LOG_INFO ("File open: " << m_fileStream.is_open () ); 
    }
    else if (method.compare("PLAY") == 0)
    {
      m_state = PLAYING;
    }
    else if (method.compare("PAUSE") == 0)
    {
      m_state = READY;
    }
    else if (method.compare("MODIFY"))
    {
      if(m_congestionLevel > MIN_CONGESTION_LEVEL)
      {
        NS_LOG_INFO("Server Congestion Modified to "<<m_congestionLevel);
        m_congestionLevel /= 2;
        m_congestionLevelTrace(m_congestionLevel);
        m_congestionThreshold = MAX_CONGESTION_LEVEL + 1;
      }
    }
    //TEARDOWN??? ????????? ?????? ????????? ??????
    else if (method.compare("TEARDOWN") == 0)
    {
      m_state = INIT;
      m_fileStream.close();

      NS_LOG_INFO ("File close: " << !m_fileStream.is_open () ); 
    }
    else
    {
      NS_LOG_ERROR("Server Rtsp: Parsing Error");
    }
    Ptr<Packet> response = Create<Packet>((uint8_t*)res.str().c_str(), 100);
    socket->Send(response);

    delete msg;
  }
}

//RTCP handler
void
RtspServer::HandleRtcpReceive(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  while ((packet = socket->RecvFrom (from)))
  {
    socket->GetSockName (localAddress);

    uint8_t* msg = new uint8_t[packet->GetSize()+1];
    packet->CopyData(msg, packet->GetSize());
    std::istringstream req((char*)msg);

    float fractionLost;
    req >> fractionLost;

    if(m_state == PLAYING) {
      if(fractionLost >= 0 && fractionLost <= 0.05)
      {
        if(
          m_upscale == int(MAX_CONGESTION_LEVEL + 2 - m_congestionLevel)
          && m_congestionLevel > MIN_CONGESTION_LEVEL
          && ( 
              !m_useCongestionThreshold || 
              (m_congestionThreshold > MAX_CONGESTION_LEVEL || m_congestionLevel > m_congestionThreshold)
          )
        ) 
        {
          m_congestionLevel /= 2;
          m_upscale = 0;
          m_congestionLevelTrace(m_congestionLevel);
        }
        else m_upscale++;
      }
      else if(fractionLost > 0.2) 
      {
        if(m_congestionLevel < MAX_CONGESTION_LEVEL) {
          m_congestionLevelTrace(m_congestionLevel);
          m_congestionLevel *= 2;
        }
        if(m_congestionThreshold > m_congestionLevel) {
          m_congestionThreshold = m_congestionLevel;
        }
        m_upscale = 0;
      }
    }

    NS_LOG_INFO("Server FractionLost : " << fractionLost << " with congestion " << m_congestionLevel);
  }
}

//Rtp ????????? m_sendDelay ?????? ???????????? ??????
void
RtspServer::ScheduleRtpSend()
{
    NS_LOG_FUNCTION(this);

    NS_ASSERT (m_sendEvent.IsExpired ());

    if(!m_fileStream.eof() && m_state == PLAYING) {
      //header seqTs??? ?????? seqNum ??????
      SeqTsHeader seqTs;
      seqTs.SetSeq (m_seqNum);

      // congestionLevel??? ?????? frame ?????? ??????
      uint32_t frameSize;
      m_fileStream >> frameSize;
      uint32_t frameSizeCongestion = frameSize / m_congestionLevel;

      Ptr<Packet> packet = Create<Packet>(frameSizeCongestion);
      packet->AddHeader (seqTs);
      
      m_rtpSocket->Send(packet);
      NS_LOG_INFO("Server Rtp Send: "<< frameSizeCongestion << " bytes in "<< m_seqNum);
      m_seqNum++;
    }

    m_sendEvent = Simulator::Schedule(MilliSeconds(m_sendDelay), &RtspServer::ScheduleRtpSend, this);
}
   

/* ???????????? ?????? */

}
