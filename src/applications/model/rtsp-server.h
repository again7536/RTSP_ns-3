/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

/*

IPv6는 지원하지 않습니다.

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
#include <fstream>
#include <vector>

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
        MODIFY,
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
    
    std::vector<int> m_frameSizes;          //전송 frame Size 배열
    int m_count;                            //전송할 총 packet 개수

    std::string m_fileName;                 //전송 파일 이름
    std::ifstream m_fileStream;             //전송 파일 스트림
    
    //RTSP variables
    //----------------
    uint32_t m_seqNum;                      //현재 전송된 시퀀스 넘버
    State_t m_state = INIT;                 //현재 서버 상태
                                            //서버 상태에 따라서 전송 / 전송 중지
    
    const static int FRAME_PERIOD = 32;     // 프레임 간격 (1초 / 동영상의 프레임 레이트)

    //RTCP variables
    //----------------
    const double MAX_CONGESTION_LEVEL = 16;
    const double MIN_CONGESTION_LEVEL = 1;
    double m_congestionLevel;               //congestion이 있을 경우 영상 압축하여 프레임 축소
                                            //여기에서는 프레임 전송 바이트에 해당 변수를 나누는 식
    double m_congestionThreshold;           //로스가 일어난 최소 레벨 기록 후에 그 레벨을 못넘게함
    bool m_useCongestionThreshold;          //컨제스쳔 기준을 설정할지 말지
    int32_t m_upscale;

    //RTP variables
    //----------------
    EventId         m_sendEvent;            //RTP 전송 타이머 이벤트
    uint64_t        m_sendDelay;            //RTP 패킷 전송 딜레이

    ns3::TracedCallback<double &> m_congestionLevelTrace; // trace callback
};

}

#endif
