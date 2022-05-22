/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

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
#include <map>
#include <queue>

namespace ns3 {

class RtspClient : public Application
{
public:
    static TypeId GetTypeId (void);
    RtspClient();
    virtual ~RtspClient();

    enum State_t
    {
        //rtsp states
        INIT,
        READY,
        PLAYING,
    };

    enum Method_t
    {
        //rtsp message types
        SETUP,
        PLAY,
        PAUSE,
        TEARDOWN,
    };

    void ScheduleMessage (Time time, Method_t requestMethod);
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

    /**************************************************
    *                    메소드
    ***************************************************/
    virtual void DoDispose();
    virtual void StartApplication();
    virtual void StopApplication();

    void SendRtspPacket(Method_t requestMethod, int64_t idx);
    void SendRtcpPacket();
    void ConsumeBuffer();

    /**************************************************
    *                      변수
    ***************************************************/
    Ptr<Socket> m_rtspSocket;
    Ptr<Socket> m_rtpSocket;
    Ptr<Socket> m_rtcpSocket;
    
    Address m_localAddress;                  // 클라이언트 IP
    Address m_remoteAddress;                 // 서버 IP
    uint16_t m_rtpPort;                      // RTP 포트
    uint16_t m_rtcpPort;                     // RTCP 포트
    uint16_t m_rtspPort;                     // RTSP 포트

    State_t m_state;                         // 클라이언트 상태

    std::map<uint32_t, uint8_t*> m_frameMap; // RTP 프레임 버퍼

    float m_lastFractionLost;                // 마지막 lost 비율
    uint32_t m_lastSeq;                      // RTCP를 마지막으로 보냈을 때의 RTP 시퀀스
    uint32_t m_lastLost;                     // RTCP를 마지막으로 보냈을 때의 RTP 누적 loss

    uint32_t m_cumLost;                      // 누적 loss 시퀀스

    uint32_t m_framePeriod;                  // 1초 / 프레임 레이트
    uint32_t m_frameCnt;                     // 시간을 프레임 단위로 나타냄  
    uint32_t m_frameExp;                     // 재생 해야할 프레임
    uint32_t m_frame;                        // 재생 중 프레임

    const static int RTCP_PERIOD = 400;      // RTCP 전송 주기

    std::string m_fileName;                  // video file name
    
    std::map<Time, Method_t> m_preSchedule;     
    std::vector<EventId> m_rtspSendEvents;   // RTSP 전송 예약 이벤트
    EventId m_consumeEvent;                  // 프레임 소모 이벤트
    EventId m_rtcpSendEvent;                 // RTCP 전송 이벤트
};

}

#endif