/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
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
 *
 * Author: Mohamed Amine Ismail <amine.ismail@sophia.inria.fr>
 */
#include "rtsp-client-server-helper.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"

namespace ns3 {

RtspServerHelper::RtspServerHelper ()
{
  m_factory.SetTypeId (RtspServer::GetTypeId ());
}

RtspServerHelper::RtspServerHelper (Address serverAddress)
{
  m_factory.SetTypeId (RtspServer::GetTypeId ());
  SetAttribute ("LocalAddress", AddressValue (serverAddress));
}

RtspServerHelper::RtspServerHelper (uint16_t port)
{
  m_factory.SetTypeId (RtspServer::GetTypeId ());
  SetAttribute ("Port", UintegerValue (port));
}

void
RtspServerHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
RtspServerHelper::Install (NodeContainer c)
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;

      m_server = m_factory.Create<RtspServer> ();
      node->AddApplication (m_server);
      apps.Add (m_server);

    }
  return apps;
}

Ptr<RtspServer>
RtspServerHelper::GetServer (void)
{
  return m_server;
}

RtspClientHelper::RtspClientHelper ()
{
  m_factory.SetTypeId (RtspClient::GetTypeId ());
}

RtspClientHelper::RtspClientHelper (Address serverAddr, Address localAddr)
{
  m_factory.SetTypeId (RtspClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (serverAddr));
  SetAttribute("LocalAddress", AddressValue(localAddr));
}

void
RtspClientHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
RtspClientHelper::Install (NodeContainer c)
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;
      m_client = m_factory.Create<RtspClient> ();
      node->AddApplication (m_client);
      apps.Add (m_client);
    }
  return apps;
}

} // namespace ns3
