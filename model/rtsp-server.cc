#include "rtsp-server.h"

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
                    MakeAddressAccessor (&ThreeGppHttpServer::m_localAddress),
                    MakeAddressChecker ())
        .AddAttribute ("LocalPort",
                    "Port on which the application listen for incoming packets.",
                    UintegerValue (80), // the default HTTP port
                    MakeUintegerAccessor (&ThreeGppHttpServer::m_localPort),
                    MakeUintegerChecker<uint16_t> ())
    ;
    return tid;
}

RtspServer::RtspServer () : m_lossCounter (0) 
{
    NS_LOG_FUNCTION (this);
    
    m_sendDelay = FRAME_PERIOD;
    m_received = 0;

    buf = new char[20000];
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
        if (m_initialSocket == 0)
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
            m_initialSocket = Socket::CreateSocket (GetNode (),
                                                    TcpSocketFactory::GetTypeId ());
            m_initialSocket->SetAttribute ("SegmentSize", UintegerValue (m_mtuSize));

            int ret;

            if (Ipv4Address::IsMatchingType (m_localAddress))
            {
                const Ipv4Address ipv4 = Ipv4Address::ConvertFrom (m_localAddress);
                const InetSocketAddress inetSocket = InetSocketAddress (ipv4,
                                                                        m_rtspPort);
                NS_LOG_INFO (this << " Binding on " << ipv4
                                    << " port " << m_rtspPort
                                    << " / " << inetSocket << ".");
                ret = m_initialSocket->Bind (inetSocket);
                NS_LOG_DEBUG (this << " Bind() return value= " << ret
                                    << " GetErrNo= "
                                    << m_initialSocket->GetErrno () << ".");
            }
            else if (Ipv6Address::IsMatchingType (m_localAddress))
            {
                const Ipv6Address ipv6 = Ipv6Address::ConvertFrom (m_localAddress);
                const Inet6SocketAddress inet6Socket = Inet6SocketAddress (ipv6,
                                                                            m_rtspPort);
                NS_LOG_INFO (this << " Binding on " << ipv6
                                    << " port " << m_localPort
                                    << " / " << inet6Socket << ".");
                ret = m_initialSocket->Bind (inet6Socket);
                NS_LOG_DEBUG (this << " Bind() return value= " << ret
                                    << " GetErrNo= "
                                    << m_initialSocket->GetErrno () << ".");
            }

            ret = m_initialSocket->Listen ();
            NS_LOG_DEBUG (this << " Listen () return value= " << ret
                                << " GetErrNo= " << m_initialSocket->GetErrno ()
                                << ".");

            NS_UNUSED (ret);

        } // end of `if (m_initialSocket == 0)`

        NS_ASSERT_MSG (m_initialSocket != 0, "Failed creating socket.");

        // 여기서부터 소켓 콜백 작성 필요
        // 아래는 http 예시

        // m_initialSocket->SetAcceptCallback (MakeCallback (&ThreeGppHttpServer::ConnectionRequestCallback,
        //                                                     this),
        //                                     MakeCallback (&ThreeGppHttpServer::NewConnectionCreatedCallback,
        //                                                     this));
        // m_initialSocket->SetCloseCallbacks (MakeCallback (&ThreeGppHttpServer::NormalCloseCallback,
        //                                                     this),
        //                                     MakeCallback (&ThreeGppHttpServer::ErrorCloseCallback,
        //                                                     this));
        // m_initialSocket->SetRecvCallback (MakeCallback (&ThreeGppHttpServer::ReceivedDataCallback,
        //                                                 this));
        // m_initialSocket->SetSendCallback (MakeCallback (&ThreeGppHttpServer::SendCallback,
        //                                                 this));
        m_state = READY;

    } // end of `if (m_state == NOT_STARTED)`
    else
    {
      NS_FATAL_ERROR ("Invalid state " << GetStateString ()
                                       << " for StartApplication().");
    }

} // end of `void StartApplication ()`

void
RtspServer::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  m_state = INIT;

  // Close all accepted sockets.
  // m_txBuffer->CloseAllSockets ();

  // Stop listening.
  if (m_initialSocket != 0)
    {
      m_initialSocket->Close ();
      m_initialSocket->SetAcceptCallback (MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
                                          MakeNullCallback<void, Ptr<Socket>, const Address &> ());
      m_initialSocket->SetCloseCallbacks (MakeNullCallback<void, Ptr<Socket> > (),
                                          MakeNullCallback<void, Ptr<Socket> > ());
      m_initialSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_initialSocket->SetSendCallback (MakeNullCallback<void, Ptr<Socket>, uint32_t > ());
    }
}

/* 자유롭게 추가 */

}
