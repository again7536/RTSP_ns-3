//rtsp-server header

#ifndef RTSP_SERVER_H
#define RTSP_SERVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"

namespace ns3 {

class RtspServer : public Application
{
public:
    static TypeId GetTypeId (void);
    
    RtspServer();

    ~RtspServer();
    

private:
    Address m_peerAddress;   //Client IP address
    int m_rtp_port = 0;      //destination port for RTP packets  (given by the RTSP Client)
    int m_rtsp_dest_port = 0;

    int m_imagenb = 0;   //image nb of the image currently transmitted
    //VideoStream video; //VideoStream object used to access video frames

    //Timer timer;    //timer used to send the images at the video frame rate
    //byte[] buf;     //buffer used to store the images to send to the client 
    int m_sendDelay;  //the delay to send images over the wire. Ideally should be
                      //equal to the frame rate of the video file, but may be 
                      //adjusted when congestion is detected.

    //RTSP variables
    //----------------
    const static enum = {
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
    int m_seqNum;
    int m_state = 0;
    
    const static int FRAME_PERIOD = 100; //Frame period of the video to stream, in ms
    const static int MJPEG_TYPE = 26;    //RTP payload type for MJPEG video
    const static int VIDEO_LENGTH = 500; //length of the video in frames

    //RTCP variables
    //----------------
    int m_congestionLevel;
 
    const static int RTCP_RCV_PORT = 19001; //port where the client will receive the RTP packets
    const static int RTCP_PERIOD = 400;     //How often to check for control events
    const static String CRLF = "\r\n";
}

}
