/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
#include <ns3/rtsp-server.h>

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

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RtspServer");

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
                    "Port on which the application listen for incoming packets.",
                    UintegerValue (80), // the default HTTP port
                    MakeUintegerAccessor (&RtspServer::m_rtspPort),
                    MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("RtpPort",
                    "Port on which the application listen for incoming packets.",
                    UintegerValue (80), // the default HTTP port
                    MakeUintegerAccessor (&RtspServer::m_rtpPort),
                    MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("RtcpPort",
                    "Port on which the application listen for incoming packets.",
                    UintegerValue (80), // the default HTTP port
                    MakeUintegerAccessor (&RtspServer::m_rtcpPort),
                    MakeUintegerChecker<uint16_t> ())
    ;
    return tid;
}

RtspServer::RtspServer ()
{
    NS_LOG_FUNCTION (this);
    
    m_sendDelay = FRAME_PERIOD;
}

RtspServer::~RtspServer ()
{
    NS_LOG_FUNCTION (this);
    
    m_sendDelay = FRAME_PERIOD;
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

    if (m_state == INIT)
    {
        /*
          RTSP 소켓 초기화
        */
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

          // Creating a TCP socket to connect to the server.
          m_rtspSocket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());

          if (Ipv4Address::IsMatchingType (m_localAddress))
          {
              const Ipv4Address ipv4 = Ipv4Address::ConvertFrom (m_localAddress);
              const InetSocketAddress inetSocket = InetSocketAddress (ipv4, m_rtspPort);
              if (m_rtpSocket->Bind (inetSocket) == -1)
              {
                NS_FATAL_ERROR ("Failed to bind socket");
              }
          }
          m_rtspSocket->Listen ();
        }
        NS_ASSERT_MSG (m_rtspSocket != 0, "Failed creating RTSP socket.");
        m_rtspSocket->SetRecvCallback (MakeCallback (&RtspServer::HandleRtspReceive, this));
        m_state = READY;

        /* 
          RTP 소켓 초기화 
        */
        if (m_rtpSocket == 0)
        {
          TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
          m_rtpSocket = Socket::CreateSocket (GetNode (), tid);
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_rtpPort);
          if (m_rtpSocket->Bind (local) == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
        }
        NS_ASSERT_MSG (m_rtpSocket != 0, "Failed creating RTP socket.");
        m_rtpSocket->SetRecvCallback (MakeCallback (&RtspServer::HandleRtpReceive, this));

        /* 
          RTCP 소켓 초기화 
        */
        if (m_rtcpSocket == 0)
        {
          TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
          m_rtcpSocket = Socket::CreateSocket (GetNode (), tid);
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_rtcpPort);
          if (m_rtcpSocket->Bind (local) == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
        }
        NS_ASSERT_MSG (m_rtpSocket != 0, "Failed creating RTP socket.");
        m_rtpSocket->SetRecvCallback (MakeCallback (&RtspServer::HandleRtcpReceive, this));
    }
    else
    {
      NS_FATAL_ERROR ("Invalid state for StartApplication().");
    }
} // end of `void StartApplication ()`

void
RtspServer::StopApplication ()
{
  NS_LOG_FUNCTION (this);

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

//RTSP handler
void
RtspServer::HandleRtspReceive (Ptr<Socket> socket)
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

    State_t request_type;
    char line[200];
    strin.getline(line, 200);
    // convert to request_type structure:
    if (strncmp(line, "SETUP", 5) == 0)
        request_type = SETUP;
    else if (strncmp(line, "PLAY", 4) == 0)
        request_type = PLAY;
    else if (strncmp(line, "PLAY", 5) == 0)
        request_type = PAUSE;
    else if (strncmp(line, "TEARDOWN", 8) == 0)
        request_type = TEARDOWN;
    else if (strncmp(line, "DESCRIBE", 8) == 0)
        request_type = DESCRIBE;

    std::cout<<request_type<<'\n';

    delete msg;
  }
}

//RTP handler
void
RtspServer::HandleRtpReceive(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  while ((packet = socket->RecvFrom (from)))
  {
    socket->GetSockName (localAddress);

    if(m_clientAddress != from)
    {
      m_clientAddress = from;
    }
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

      if(m_clientAddress != from)
      {
        m_clientAddress = from;
      }
      uint8_t* msg = new uint8_t[packet->GetSize()+1];
      packet->CopyData(msg, packet->GetSize());
      //float fractionLost = msg[0] / 255.0f;
      //int32_t cumLost = ((msg[1] << 24) + (msg[2] << 16) + (msg[3] << 8)) >> 8;
      //uint32_t highSeqNb = msg[4] + (msg[5] << 8) + (msg[6] << 16) + (msg[7] << 24);
    }
}

//Rtp 패킷을 m_sendDelay 마다 반복해서 보냄
void
RtspServer::ScheduleRtpSend()
{
  if(m_rtpSocket == 0) return;

  Ptr<Packet> packet = Create<Packet>(1500);
  m_rtpSocket->SendTo(packet, 0, m_clientAddress);
  
  Simulator::Schedule(Seconds(m_sendDelay), &RtspServer::ScheduleRtpSend, this);
}

/* 자유롭게 추가 */

}
