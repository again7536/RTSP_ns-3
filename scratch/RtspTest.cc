/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Network topology
//
//       n0    n1
//       |     |
//       =======
//         LAN
//
// - UDP flows from n0 to n1

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/rtsp-client-server-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/object-factory.h"
#include "ns3/error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RtspTest");

void CalculateThroughput(const Ptr<RtspClient> client)
{
  if(Simulator::IsFinished() || Simulator::Now().GetSeconds() > 21) return;

  static uint64_t lastRx = 0;
  uint64_t curRx = client->GetRxSize();

  NS_LOG_INFO(Simulator::Now().GetSeconds() <<' '<<(curRx - lastRx) * 80.0 / 1000000 );
  lastRx = curRx;
  Simulator::Schedule(MilliSeconds(100), CalculateThroughput, client);
}

// void OnChangeFractionLoss(float& fractionLoss)
// {
//   NS_LOG_INFO(Simulator::Now().GetSeconds() <<' '<<fractionLoss * 100);
// }

void OnChangeCongestionLevel(double& congestionLevel)
{
  NS_LOG_INFO(Simulator::Now().GetSeconds() <<' '<< congestionLevel);
}

int
main (int argc, char *argv[])
{
  // LogComponentEnable ("RtspServer", LOG_LEVEL_INFO);
  // LogComponentEnable ("RtspClient", LOG_LEVEL_INFO);
  LogComponentEnable ("RtspTest", LOG_LEVEL_INFO);

  Address serverAddress;
  Address clientAddress;
  NodeContainer n;
  n.Create (2);

  InternetStackHelper internet;
  internet.Install (n);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10us"));
  NetDeviceContainer d = p2p.Install (n);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (d);
  clientAddress = Address (i.GetAddress (0));
  serverAddress = Address (i.GetAddress (1));

  RtspServerHelper server(serverAddress);
  ApplicationContainer apps = server.Install (n.Get (1));
  server.SetAttribute("UseCongestionThreshold", BooleanValue(true));

  Ptr<RtspServer> rtspServer = DynamicCast<RtspServer>(apps.Get(0));
  //rtspServer->TraceConnectWithoutContext("CongestionLevel", MakeCallback(&OnChangeCongestionLevel));

  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (20.0));

  RtspClientHelper client(serverAddress, clientAddress);
  client.SetAttribute ("FileName", StringValue ("./scratch/frame.txt")); // set File name
  apps = client.Install (n.Get (0));

  //manually set message
  Ptr<RtspClient> rtspClient = DynamicCast<RtspClient>(apps.Get(0));
  rtspClient->ScheduleMessage(Seconds(2), RtspClient::SETUP);
  rtspClient->ScheduleMessage(Seconds(3), RtspClient::PLAY);
  // rtspClient->ScheduleMessage(Seconds(4), RtspClient::PAUSE);
  // rtspClient->ScheduleMessage(Seconds(5), RtspClient::PLAY);
  // rtspClient->ScheduleMessage(Seconds(7), RtspClient::MODIFY);

  //rtspClient->TraceConnectWithoutContext("FractionLoss", MakeCallback(&OnChangeFractionLoss));

  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (20.0));

  //Error Model
  ObjectFactory factory;
  factory.SetTypeId (RateErrorModel::GetTypeId());
  Ptr<RateErrorModel> em = factory.Create<RateErrorModel> ();
  em->SetAttribute("ErrorRate", DoubleValue(0.1));
  em->SetUnit(RateErrorModel::ERROR_UNIT_PACKET);
  d.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  Simulator::Schedule(MilliSeconds(100), CalculateThroughput, rtspClient);

  // Now, do the actual simulation.
  Simulator::Run ();
  Simulator::Stop(Seconds(30.0));
  Simulator::Destroy ();
}
