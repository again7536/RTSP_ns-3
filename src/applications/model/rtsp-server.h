/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

/*

three-gpp-http-server.h랑 server.java 참고해서 만들었습니다.
각자 해야하는 역할 (노션에 적은 것) 주석 달아놨으니 서치해서 
함수/클래스 구현하면 될 것 같습니다.
변수랑 메소드 전부 적어 놓은게 아니라서 필요하면 추가해서 쓰면 됩니다!!

*/

#ifndef RTSP_SERVER_H
#define RTSP_SERVER_H

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
#include <sstream>

namespace ns3 {

class RtspServer : public Application
{
public:
    static TypeId GetTypeId (void);
    RtspServer();
    virtual ~RtspServer();

    enum State_t
    {
        //rtsp states
        INIT,
        READY,
        PLAYING,
        //rtsp message types
        SETUP,
        PLAY,
        PAUSE,
        TEARDOWN,
        DESCRIBE
    };

private:
    /**************************************************
    *                   소켓 콜백
    **************************************************/
    bool ConnectionRequestCallback (Ptr<Socket> socket, const Address &address);
    void NewConnectionCreatedCallback (Ptr<Socket> socket, const Address &address);
    //Handle RTSP Request
    void HandleRtspReceive (Ptr<Socket> socket);
    //Send RTSP Response
    void SendCallback (Ptr<Socket> socket, uint32_t availableBufferSize);
    //Handle RTCP Request
    void HandleRtcpReceive (Ptr<Socket> socket);

    /**************************************************
    *                    메소드
    ***************************************************/
    virtual void DoDispose();
    virtual void StartApplication();
    virtual void StopApplication();

    void ScheduleRtpSend();

    //구현 대상 #2
    void OpenFileStream();

    /**************************************************
    *                      변수
    ***************************************************/
    Ptr<Socket> m_rtspSocket;
    Ptr<Socket> m_rtpSocket;
    Ptr<Socket> m_rtcpSocket;
    
    Address     m_localAddress;             //서버 IP 주소
    Address     m_clientAddress;            //클라이언트 IP 주소
    uint16_t    m_rtpPort;                  //RTP 소켓 포트
    uint16_t    m_rtcpPort;                 //RTCP 소켓 포트
    uint16_t    m_rtspPort;                 //RTSP 소켓 포트

    std::string m_fileName;                 //전송 파일 이름
    std::stringstream m_fileStream;         //전송 파일 스트림
    
    //RTSP variables
    //----------------
    int m_seqNum;                           //현재 전송된 시퀀스 넘버
    State_t m_state = INIT;                 //현재 서버 상태
                                            //서버 상태에 따라서 전송 / 전송 중지
    
    const static int FRAME_PERIOD = 100;    // 프레임 간격 (1초 / 동영상의 프레임 레이트)

    //RTCP variables
    //----------------
    int m_congestionLevel;                  //congestion이 있을 경우 영상 압축하여 프레임 축소
                                            //여기에서는 프레임 전송 바이트에 해당 변수를 나누는 식

    //RTP variables
    //----------------
    EventId         m_sendEvent;            //RTP 전송 타이머 이벤트
    uint8_t*        m_frameBuf;             //RTP 전송 버퍼
    uint64_t        m_sendDelay;            //RTP 패킷 전송 딜레이
};

}

#endif