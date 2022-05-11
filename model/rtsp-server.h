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
    virtual void DoDispose();
    virtual void StartApplication();
    virtual void StopApplication();

    void ScheduleRtpSend();

    /**************************************************
    *                      변수
    ***************************************************/
    Ptr<Socket> m_rtspSocket;
    Ptr<Socket> m_rtpSocket;
    Ptr<Socket> m_rtcpSocket;
    Address m_localAddress;          //Server IP address
    Address m_clientAddress;         //Client IP address
    uint16_t m_rtpPort = 0;          //destination port for RTP packets  (given by the RTSP Client)
    uint16_t m_rtcpPort = 0;
    uint16_t m_rtspPort = 0;

    uint32_t m_imagenb = 0;               //image nb of the image currently transmitted
    //VideoStream video;             //VideoStream object used to access video frames

    //Timer timer;                   //timer used to send the images at the video frame rate
    //byte[] buf;                    //buffer used to store the images to send to the client 
    uint64_t m_sendDelay;                 //the delay to send images over the wire. Ideally should be
                                     //equal to the frame rate of the video file, but may be 
                                     //adjusted when congestion is detected.

    //RTSP variables
    //----------------
    int m_seqNum;
    State_t m_state = INIT;
    
    const static int FRAME_PERIOD = 100; //Frame period of the video to stream, in ms
    const static int MJPEG_TYPE = 26;    //RTP payload type for MJPEG video
    const static int VIDEO_LENGTH = 500; //length of the video in frames

    //RTCP variables
    //----------------
    int m_congestionLevel;
 
    const static int RTCP_RCV_PORT = 19001; //port where the client will receive the RTP packets
    const static int RTCP_PERIOD = 400;     //How often to check for control events
};

}

#endif