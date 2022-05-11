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
#include <ns3/udp-socket-factory.h>
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
    m_rtspPort = 9;
    m_rtcpPort = 10;
    m_rtpPort = 11;
    m_sendDelay = FRAME_PERIOD;
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

    if (m_state == INIT)
    {
        /*
          RTSP 소켓 초기화
        */
        if (m_rtspSocket == 0)
        {
          // Creating a TCP socket to connect to the server.
          m_rtspSocket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
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
        m_state = READY;

        /* 
          RTP 소켓 초기화 (보내는 용이라서 바인딩 안함)
        */
        if (m_rtpSocket == 0)
        {
          m_rtpSocket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        }
        NS_ASSERT_MSG (m_rtpSocket != 0, "Failed creating RTP socket.");

        /* 
          RTCP 소켓 초기화
        */
        if (m_rtcpSocket == 0)
        {
          m_rtcpSocket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
          
          const Ipv4Address ipv4 = Ipv4Address::ConvertFrom (m_localAddress);
          InetSocketAddress local = InetSocketAddress (ipv4, m_rtcpPort);
          if (m_rtcpSocket->Bind (local) == -1)
            {
              NS_FATAL_ERROR ("Failed to bind RTCP socket");
            }
        }
        NS_ASSERT_MSG (m_rtcpSocket != 0, "Failed creating RTCP socket.");
        m_rtcpSocket->SetRecvCallback (MakeCallback (&RtspServer::HandleRtcpReceive, this));
    }
    else
    {
      NS_FATAL_ERROR ("Invalid state for StartApplication().");
    }
    
    // m_sendEvent = Simulator::Schedule(Seconds(0), &RtspServer::ScheduleRtpSend, this);
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
    uint8_t* msg = new uint8_t[packet->GetSize()+1];
    packet->CopyData(msg, packet->GetSize());
    NS_LOG_INFO("Server Recv: " << msg);
    std::istringstream strin(std::string((char*)msg), std::ios::in);

    State_t request_type;
    char line[200];
    strin.getline(line, 200);

    if (strncmp(line, "SETUP", 5) == 0)
        request_type = SETUP;
    else if (strncmp(line, "PLAY", 3) == 0)
        request_type = PLAY;
    else if (strncmp(line, "PAUSE", 5) == 0)
        request_type = PAUSE;
    else if (strncmp(line, "TEARDOWN", 8) == 0)
        request_type = TEARDOWN;
    else if (strncmp(line, "DESCRIBE", 8) == 0)
        request_type = DESCRIBE;

    NS_LOG_INFO("Server request type: " << request_type);

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
  NS_LOG_FUNCTION(this);

  NS_ASSERT (m_sendEvent.IsExpired ());

  Ptr<Packet> packet = Create<Packet>(1500);
  m_rtpSocket->SendTo(packet, 0, m_clientAddress);
  
  m_sendEvent = Simulator::Schedule(Seconds(m_sendDelay), &RtspServer::ScheduleRtpSend, this);
}

/* 자유롭게 추가 */

}
