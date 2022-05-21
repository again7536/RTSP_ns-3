/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

#include "rtsp-client.h"

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

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RtspClient");

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
        .AddAttribute ("RtspPort",
                    "Port on which the application listen for incoming packets.",
                    UintegerValue (9), // the default HTTP port
                    MakeUintegerAccessor (&RtspClient::m_rtspPort),
                    MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("RtcpPort",
                    "Port on which the application listen for incoming packets.",
                    UintegerValue (10), // the default HTTP port
                    MakeUintegerAccessor (&RtspClient::m_rtcpPort),
                    MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("RtpPort",
                    "Port on which the application listen for incoming packets.",
                    UintegerValue (11), // the default HTTP port
                    MakeUintegerAccessor (&RtspClient::m_rtpPort),
                    MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("FileName",
                   "Name of file.",
                   StringValue ("sample.txt"),
                   MakeStringAccessor (&RtspClient::m_fileName),
                   MakeStringChecker ())
    ;
    return tid;
}

RtspClient::RtspClient ()
{
    NS_LOG_FUNCTION (this);
    m_rtspPort = 9;
    m_rtcpPort = 10;
    m_rtpPort = 11;

    m_frameBuf = new uint8_t[50000];
    memset(m_frameBuf, 0, 50000);
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
        // Find the current default MTU value of TCP sockets.
        Ptr<const ns3::AttributeValue> previousSocketMtu;
        const TypeId tcpSocketTid = TcpSocket::GetTypeId ();
        for (uint32_t i = 0; i < tcpSocketTid.GetAttributeN (); i++)
        {
            struct TypeId::AttributeInformation attrInfo = tcpSocketTid.GetAttribute (i);
            if (attrInfo.name == "SegmentSize")
            {
                previousSocketMtu = attrInfo.initialValue;
            }
        }

        // Creating a TCP socket to connect to the Client.
        m_rtspSocket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());

        if (Ipv4Address::IsMatchingType (m_remoteAddress))
        {   
            m_rtspSocket->Bind();
            const Ipv4Address ipv4 = Ipv4Address::ConvertFrom (m_remoteAddress);
            const InetSocketAddress inetSocket = InetSocketAddress (ipv4, m_rtspPort);
            m_rtspSocket->Connect (inetSocket);
        }
       
    } // end of `if (m_rtspSocket == 0)`

    NS_ASSERT_MSG (m_rtspSocket != 0, "Failed creating RTSP socket.");

    m_rtspSocket->SetRecvCallback (MakeCallback (&RtspClient::HandleRtspReceive, this));

    /* RTP 소켓 초기화 */
    if (m_rtpSocket == 0)
    {
        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        m_rtpSocket = Socket::CreateSocket (GetNode (), tid);
        if (m_rtpSocket->Bind () == -1)
        {
            NS_FATAL_ERROR ("Failed to bind socket");
        }

        const Ipv4Address ipv4 = Ipv4Address::ConvertFrom (m_remoteAddress);
        const InetSocketAddress inetSocket = InetSocketAddress (ipv4, m_rtpPort);
        m_rtpSocket->Connect (inetSocket);
    }
    NS_ASSERT_MSG (m_rtpSocket != 0, "Failed creating RTP socket.");
    m_rtpSocket->SetRecvCallback (MakeCallback (&RtspClient::HandleRtpReceive, this));

    /* RTCP 소켓 초기화 (보내는 용이라서 바인딩 안함) */
    if (m_rtcpSocket == 0)
    {
        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        m_rtcpSocket = Socket::CreateSocket (GetNode (), tid);
    }
    NS_ASSERT_MSG (m_rtcpSocket != 0, "Failed creating RTP socket.");

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
    std::istringstream strin(std::string((char*)msg), std::ios::in);

    //strin.getline();
    
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

  uint8_t buf[100];

  //SETUP일 경우 파일명 붙임
  if(requestMethod == SETUP)
  {
    strcpy((char *)buf,"SETUP\n");
    strcat((char *)buf, m_fileName.c_str());
  }
  else if(requestMethod == PLAY)
  {
    strcpy((char *)buf,"PLAY\n");
  }
  else if(requestMethod == PAUSE)
  {
    strcpy((char *)buf,"PAUSE\n");
  }
  else if(requestMethod == TEARDOWN)
  {
    strcpy((char *)buf,"TEARDOWN\n");
  }
  else if(requestMethod == DESCRIBE)
  {
    strcpy((char *)buf,"DESCRIBE\n");
  }
  else
  {
    strcpy((char *)buf,"PLAY\n");
  }
  
  Ptr<Packet> packet = Create<Packet>(buf, 100);
  int ret = m_rtspSocket->Send(packet);

  if (Ipv4Address::IsMatchingType (m_remoteAddress))
  {
    NS_LOG_INFO ("Client sent " << ret << " bytes to " <<
                  Ipv4Address::ConvertFrom (m_remoteAddress) << " port " << m_rtspPort);
  }
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

    packet->CopyData(m_frameBuf, packet->GetSize());
    NS_LOG_INFO("Client Rtp Recv: " << packet->GetSize());
  }
}

}