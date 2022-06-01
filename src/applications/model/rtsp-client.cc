/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

#include "rtsp-client.h"
#include "seq-ts-header.h"

#include <sstream>
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
#include <ns3/inet-socket-address.h>
#include <ns3/inet6-socket-address.h>
#include <ns3/unused.h>
#include <ns3/string.h>

NS_LOG_COMPONENT_DEFINE("RtspClient");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(RtspClient);

TypeId
RtspClient::GetTypeId (void) 
{
    static TypeId tid = TypeId("ns3::RtspClient")
        .SetParent<Application> ()
        .SetGroupName("Applications")
        .AddConstructor<RtspClient>()
        .AddAttribute ("RemoteAddress",
                    "The local address of the server, "
                    "i.e., the address on which to bind the Rx socket.",
                    AddressValue (),
                    MakeAddressAccessor (&RtspClient::m_remoteAddress),
                    MakeAddressChecker ())
        .AddAttribute ("LocalAddress",
                    "The local address of the client, "
                    "i.e., the address on which to bind the Rx socket.",
                    AddressValue (),
                    MakeAddressAccessor (&RtspClient::m_localAddress),
                    MakeAddressChecker ())
        .AddAttribute ("RtspPort",
                    "Rtsp Socket Port.",
                    UintegerValue (9),
                    MakeUintegerAccessor (&RtspClient::m_rtspPort),
                    MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("RtcpPort",
                    "Rtcp Socket Port.",
                    UintegerValue (10), 
                    MakeUintegerAccessor (&RtspClient::m_rtcpPort),
                    MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("RtpPort",
                    "Rtp Socket Port.",
                    UintegerValue (11),
                    MakeUintegerAccessor (&RtspClient::m_rtpPort),
                    MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("FileName",
                   "Name of file.",
                   StringValue ("sample.txt"),
                   MakeStringAccessor (&RtspClient::m_fileName),
                   MakeStringChecker ())
        .AddTraceSource ("FractionLoss",
                    "Rtsp Fraction Loss",
                    MakeTraceSourceAccessor (&RtspClient::m_fractionLossTrace),
                    "ns3::RtspClient::TracedCallback")
    ;
    return tid;
}

RtspClient::RtspClient ()
{
    NS_LOG_FUNCTION (this);
    m_rtspPort = 9;
    m_rtcpPort = 10;
    m_rtpPort = 11;

    m_state = INIT;
    m_framePeriod = 10000;

    m_frame = 0;
    m_frameCnt = 0;
    m_cumLost = 0;
    
    m_lastSeq = 0;
    m_lastLost = 0; 
    m_lastFractionLost = 0;
    m_curFractionLost = 0;

    m_rxSize = 0;
}

RtspClient::~RtspClient ()
{
    NS_LOG_FUNCTION (this);
}

void
RtspClient::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  if (!Simulator::IsFinished ())
    {
      StopApplication ();
    }

  Application::DoDispose (); // Chain up.
}

void
RtspClient::StartApplication ()
{
    NS_LOG_FUNCTION (this);

    /*RTSP 소켓 초기화*/
    if (m_rtspSocket == 0)
    {
        // Creating a TCP socket to connect to the Client.
        m_rtspSocket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
        m_rtspSocket->SetAttribute("SegmentSize", UintegerValue(1500));

        const Ipv4Address localIpv4 = Ipv4Address::ConvertFrom (m_localAddress);
        const InetSocketAddress localInetSocket = InetSocketAddress (localIpv4, m_rtspPort);
        m_rtspSocket->Bind(localInetSocket);

        const Ipv4Address ipv4 = Ipv4Address::ConvertFrom (m_remoteAddress);
        const InetSocketAddress inetSocket = InetSocketAddress (ipv4, m_rtspPort);
        m_rtspSocket->Connect (inetSocket);
    } // end of `if (m_rtspSocket == 0)`

    NS_ASSERT_MSG (m_rtspSocket != 0, "Failed creating RTSP socket.");

    m_rtspSocket->SetRecvCallback (MakeCallback (&RtspClient::HandleRtspReceive, this));

    /* RTP 소켓 초기화 */
    if (m_rtpSocket == 0)
    {
        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        m_rtpSocket = Socket::CreateSocket (GetNode (), tid);

        const Ipv4Address ipv4 = Ipv4Address::ConvertFrom (m_localAddress);
        const InetSocketAddress inetSocket = InetSocketAddress (ipv4, m_rtpPort);
        if (m_rtpSocket->Bind (inetSocket) == -1)
        {
            NS_FATAL_ERROR ("Failed to bind socket");
        }
    }
    NS_ASSERT_MSG (m_rtpSocket != 0, "Failed creating RTP socket.");
    m_rtpSocket->SetRecvCallback (MakeCallback (&RtspClient::HandleRtpReceive, this));

    /* RTCP 소켓 초기화 */
    if (m_rtcpSocket == 0)
    {
        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        m_rtcpSocket = Socket::CreateSocket (GetNode (), tid);

        const Ipv4Address localIpv4 = Ipv4Address::ConvertFrom (m_localAddress);
        const InetSocketAddress localInetSocket = InetSocketAddress (localIpv4, m_rtcpPort);
        if (m_rtcpSocket->Bind (localInetSocket) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
        const Ipv4Address ipv4 = Ipv4Address::ConvertFrom (m_remoteAddress);
        const InetSocketAddress inetSocket = InetSocketAddress (ipv4, m_rtcpPort);
        m_rtcpSocket->Connect (inetSocket);
    }
    NS_ASSERT_MSG (m_rtcpSocket != 0, "Failed creating RTCP socket.");

    for(auto item: m_preSchedule) 
    {
        m_rtspSendEvents.push_back(Simulator::Schedule(
          item.first, 
          &RtspClient::SendRtspPacket, 
          this,
          item.second,
          m_rtspSendEvents.size()
        ));
    }
} // end of `void StartApplication ()`

void
RtspClient::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  for(auto event: m_rtspSendEvents)
  {
      event.Cancel();
  }
  m_consumeEvent.Cancel();
  m_rtcpSendEvent.Cancel();

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

//Schedule RTSP Message in prior
void
RtspClient::ScheduleMessage (Time time, RtspClient::Method_t requestMethod)
{
  m_preSchedule[time] = requestMethod;
}

uint64_t
RtspClient::GetRxSize()
{
  return m_rxSize;
}

double
RtspClient::GetFractionLost()
{
  return m_curFractionLost;
}

//RTSP handler
void
RtspClient::HandleRtspReceive (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  while((packet = socket->Recv()))
  {
    if (packet->GetSize () == 0)
    {
      break;
    }
    uint8_t* msg = new uint8_t[packet->GetSize()+1];
    packet->CopyData(msg, packet->GetSize());
    std::istringstream res((char*)msg);

    std::string method;
    uint32_t responseStat;

    res >> method >> responseStat;

    if(responseStat == 200) 
    {
      if(method.compare("SETUP") == 0)
      {
        m_state = READY;
        res >> m_framePeriod;
        m_rtcpSendEvent = Simulator::Schedule(MilliSeconds(RtspClient::RTCP_PERIOD), &RtspClient::SendRtcpPacket, this);
      }
      else if(method.compare("PLAY") == 0)
      {
        m_state = PLAYING;

        if(m_consumeEvent.IsExpired())
        {
          m_consumeEvent = Simulator::Schedule(MilliSeconds(m_framePeriod*2), &RtspClient::ConsumeBuffer, this);
        }
      }
      else if(method.compare("PAUSE") == 0)
      {
        m_state = READY;
        Simulator::Cancel(m_consumeEvent);
      }
      else if(method.compare("TEARDOWN") == 0)
      {
        m_state = INIT;
        m_consumeEvent.Cancel();
        m_rtcpSendEvent.Cancel();
      }
    }
    else
    {
      NS_LOG_ERROR("Client Rtsp: Parsing Error " << responseStat);
    }
    
    delete msg;
  }
}

//RTSP Sender
void
RtspClient::SendRtspPacket (RtspClient::Method_t requestMethod, int64_t idx)
{
  NS_LOG_FUNCTION(this);

  auto event = m_rtspSendEvents[idx];

  NS_ASSERT (event.IsExpired ());
  NS_ASSERT (!Simulator::IsFinished());

  std::ostringstream req;

  //SETUP일 경우 파일명 붙임
  if(requestMethod == SETUP)
  {
    m_state = READY;
    req << "SETUP\n";
    req << m_fileName;
  }
  else if(requestMethod == PLAY)
  {
    req << "PLAY\n";
  }
  else if(requestMethod == PAUSE)
  {
    m_state = READY;
    req << "PAUSE\n";
  }
  else if(requestMethod == TEARDOWN)
  {
    m_state = INIT;
    req << "TEARDOWN\n";
  }
  else if(requestMethod == MODIFY)
  {
    req << "MODIFY\n";
  }
  else
  {
    NS_LOG_ERROR("Invalid method");
  }

  Ptr<Packet> packet = Create<Packet>((uint8_t*)req.str().c_str(), 100);
  int ret = m_rtspSocket->Send(packet);

  if (Ipv4Address::IsMatchingType (m_remoteAddress))
  {
    NS_LOG_INFO ("Client sent " << ret << " bytes to " <<
                  Ipv4Address::ConvertFrom (m_remoteAddress) << " port " << m_rtspPort);
  }
}

//RTCP Sender
void
RtspClient::SendRtcpPacket()
{
  NS_LOG_FUNCTION(this);

  NS_ASSERT(m_rtcpSendEvent.IsExpired());

  std::ostringstream req;

  if(m_state == PLAYING) {
    if(m_frameCnt != m_lastSeq)
      m_curFractionLost = ((float)m_cumLost - m_lastLost) / ((float)m_frameCnt - m_lastSeq);
    m_curFractionLost = (m_curFractionLost * 0.85) + (m_lastFractionLost * 0.15);
    req << m_curFractionLost;
    m_fractionLossTrace(m_curFractionLost);

    m_lastFractionLost = m_curFractionLost;
  }
  else {
    req << 0;
  }
  m_lastLost = m_cumLost;
  m_lastSeq = m_frameCnt;

  Ptr<Packet> packet = Create<Packet>((uint8_t*)req.str().c_str(), 100);
  m_rtcpSocket->Send(packet);

  NS_LOG_INFO("Client Rtcp Send: " << req.str());
  m_rtcpSendEvent = Simulator::Schedule(MilliSeconds(RtspClient::RTCP_PERIOD), &RtspClient::SendRtcpPacket, this);
}

//RTP handler
void
RtspClient::HandleRtpReceive(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  while ((packet = socket->RecvFrom (from)))
  {
    socket->GetSockName (localAddress);

    SeqTsHeader header;
    packet->RemoveHeader(header);

    uint32_t seq = header.GetSeq();

    std::ostringstream strout;
    packet->CopyData(&strout, packet->GetSize());
    m_rxSize += packet->GetSize();

    m_frameMap[seq] = strout.str();
    NS_LOG_INFO("client seq: "<<seq);
    NS_LOG_INFO("Client Rtp Recv: " << packet->GetSize());
  }
}

//일정한 간격에 맞게 프레임 소비
void
RtspClient::ConsumeBuffer()
{
  NS_LOG_FUNCTION(this);

  NS_ASSERT(m_consumeEvent.IsExpired());

  auto frame = m_frameMap.begin();
  if(m_state == PLAYING)
  {
    //다음 프레임이 있다면 다음 프레임을 바로 재생함
    if((frame = m_frameMap.lower_bound(m_frame)) == m_frameMap.end())
    {
      NS_LOG_INFO("Buffering occurs at: " << m_frame);
      m_cumLost++;
    }
    else
    {
      m_frame = frame->first;
      NS_LOG_INFO("Consumed Frame: " << m_frame);
      
      //모든 이전프레임 삭제
      auto begin = m_frameMap.begin();
      for(;frame != begin; frame--)
        m_frameMap.erase(frame);
      m_frameMap.erase(begin);
      m_frame++;
    }
    m_frameCnt++;
  }
  m_consumeEvent = Simulator::Schedule( MilliSeconds(m_framePeriod), &RtspClient::ConsumeBuffer, this );
}

}