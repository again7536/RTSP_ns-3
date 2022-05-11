/*

three-gpp-http-Client.h랑 Client.java 참고해서 만들었습니다.
각자 해야하는 역할 (노션에 적은 것) 주석 달아놨으니 서치해서 
함수/클래스 구현하면 될 것 같습니다.
변수랑 메소드 전부 적어 놓은게 아니라서 필요하면 추가해서 쓰면 됩니다!!

*/

#ifndef RTSP_CLIENT_H
#define RTSP_CLIENT_H

#include <ns3/ptr.h>
#include <ns3/simple-ref-count.h>
#include <ns3/nstime.h>
#include <ns3/event-id.h>
#include <ns3/three-gpp-http-header.h>
#include <ns3/application.h>
#include <ns3/address.h>
#include <ns3/traced-callback.h>
#include <ns3/socket.h>
#include <ostream>

namespace ns3 {

class RtspClient : public Application
{
public:
    static TypeId GetTypeId (void);
    RtspClient();
    ~RtspClient();

private:
    /**************************************************
    *                   소켓 콜백
    **************************************************/
    //Handle RTSP Request
    void HandleRtspReceive (Ptr<Socket> socket);
    //Send RTSP Response
    void SendCallback (Ptr<Socket> socket, uint32_t availableBufferSize);
    //Handle RTP Request
    void HandleRtpReceive (Ptr<Socket> socket);
    //Handle RTCP Request
    void HandleRtcpReceive (Ptr<Socket> socket);

    /**************************************************
    *                    메소드
    ***************************************************/
    void DoDispose();
    void StartApplication();
    void StopApplication();

    void ScheduleRtpSend();

    /**************************************************
    *                      변수
    ***************************************************/
    Ptr<Socket> m_rtspSocket;
    Ptr<Socket> m_rtpSocket;
    Ptr<Socket> m_rtcpSocket;
    Address m_localAddress;          //Client IP address
    Address m_clientAddress;         //Client IP address
    uint16_t m_rtpPort = 0;          //destination port for RTP packets  (given by the RTSP Client)
    uint16_t m_rtcpPort = 0;
    uint16_t m_rtspPort = 0;
};

}

#endif