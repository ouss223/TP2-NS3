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

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor.h"
#include "ns3/ipv4-flow-classifier.h"
#include <fstream>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThirdScriptExample");

class PacketDelayTracker
{
public:
    struct PacketInfo
    {
        Time sendTime;
        Time receiveTime;
        uint32_t packetId;
        bool received;
    };

    std::map<uint32_t, PacketInfo> packetMap;
    std::vector<std::pair<uint32_t, Time>> delays;

    void RecordSendTime(uint32_t packetId, Time sendTime)
    {
        packetMap[packetId].sendTime = sendTime;
        packetMap[packetId].packetId = packetId;
        packetMap[packetId].received = false;
    }

    void RecordReceiveTime(uint32_t packetId, Time receiveTime)
    {
        if (packetMap.find(packetId) != packetMap.end())
        {
            packetMap[packetId].receiveTime = receiveTime;
            packetMap[packetId].received = true;
            
            Time delay = receiveTime - packetMap[packetId].sendTime;
            delays.push_back(std::make_pair(packetId, delay));
        }
    }

    void ExportDelays(const std::string& filename)
    {
        std::ofstream outFile(filename);
        outFile << "PacketNumber,DelayMs\n";
        
        for (const auto& delay : delays)
        {
            outFile << delay.first << "," << delay.second.GetMilliSeconds() << "\n";
        }
        outFile.close();
    }
};

PacketDelayTracker clientTracker;

void ClientTxTrace(Ptr<const Packet> packet)
{
    uint32_t packetId = packet->GetUid();
    Time sendTime = Simulator::Now();
    clientTracker.RecordSendTime(packetId, sendTime);
}

void ClientRxTrace(Ptr<const Packet> packet)
{
    uint32_t packetId = packet->GetUid();
    Time receiveTime = Simulator::Now();
    clientTracker.RecordReceiveTime(packetId, receiveTime);
}

int
main(int argc, char* argv[])
{
    bool verbose = false;
    uint32_t nWifi = 4;
    uint32_t nPackets = 10;
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifi", "Number of wifi STA devices per network", nWifi);
    cmd.AddValue("nPackets", "Number of packets to send", nPackets);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    if (nWifi > 9)
    {
        std::cout << "nWifi should be 9 or less (total nodes = 2 * nWifi)" << std::endl;
        return 1;
    }

    if (nPackets > 20)
    {
        std::cout << "nPackets should be 20 or less" << std::endl;
        return 1;
    }

    NodeContainer p2pNodes;
    p2pNodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install(p2pNodes);

    NodeContainer wifiStaNodes1;
    wifiStaNodes1.Create(nWifi);
    NodeContainer wifiApNode1 = p2pNodes.Get(0);

    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy1;
    phy1.SetChannel(channel1.Create());

    WifiMacHelper mac1;
    Ssid ssid1 = Ssid("ns-3-ssid-1");

    WifiHelper wifi1;
    wifi1.SetStandard(WIFI_STANDARD_80211n);

    NetDeviceContainer staDevices1;
    mac1.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    staDevices1 = wifi1.Install(phy1, mac1, wifiStaNodes1);

    NetDeviceContainer apDevices1;
    mac1.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices1 = wifi1.Install(phy1, mac1, wifiApNode1);

    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(nWifi);
    NodeContainer wifiApNode2 = p2pNodes.Get(1);

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy2;
    phy2.SetChannel(channel2.Create());

    WifiMacHelper mac2;
    Ssid ssid2 = Ssid("ns-3-ssid-2");

    WifiHelper wifi2;
    wifi2.SetStandard(WIFI_STANDARD_80211n);

    NetDeviceContainer staDevices2;
    mac2.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi2.Install(phy2, mac2, wifiStaNodes2);

    NetDeviceContainer apDevices2;
    mac2.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices2 = wifi2.Install(phy2, mac2, wifiApNode2);

    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(5.0),
                                  "DeltaY",
                                  DoubleValue(10.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds",
                              RectangleValue(Rectangle(-50, 50, -50, 50)),
                              "Speed",
                              StringValue("ns3::ConstantRandomVariable[Constant=5.0]"));
    mobility.Install(wifiStaNodes1);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode1);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(20.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(5.0),
                                  "DeltaY",
                                  DoubleValue(10.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds",
                              RectangleValue(Rectangle(20, 70, -50, 50)),
                              "Speed",
                              StringValue("ns3::ConstantRandomVariable[Constant=5.0]"));
    mobility.Install(wifiStaNodes2);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode2);

    InternetStackHelper stack;
    stack.Install(wifiApNode1);
    stack.Install(wifiStaNodes1);
    stack.Install(wifiApNode2);
    stack.Install(wifiStaNodes2);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(p2pDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer wifi2Interfaces;
    wifi2Interfaces = address.Assign(staDevices2);
    address.Assign(apDevices2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer wifi1Interfaces;
    wifi1Interfaces = address.Assign(staDevices1);
    address.Assign(apDevices1);

    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes2.Get(nWifi - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(wifi2Interfaces.GetAddress(nWifi - 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes1.Get(nWifi - 1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ptr<UdpEchoClient> client = DynamicCast<UdpEchoClient>(clientApps.Get(0));
    
    client->TraceConnectWithoutContext("Tx", MakeCallback(&ClientTxTrace));
    client->TraceConnectWithoutContext("Rx", MakeCallback(&ClientRxTrace));

    std::system("mkdir -p tp2");
    AnimationInterface anim("tp2/anim1.xml");
    
    for (uint32_t i = 0; i < wifiStaNodes1.GetN(); ++i)
    {
        anim.UpdateNodeColor(wifiStaNodes1.Get(i)->GetId(), 255, 0, 0);
    }
    for (uint32_t i = 0; i < wifiStaNodes2.GetN(); ++i)
    {
        anim.UpdateNodeColor(wifiStaNodes2.Get(i)->GetId(), 0, 0, 255);
    }
    anim.UpdateNodeColor(wifiApNode1.Get(0)->GetId(), 255, 128, 0);
    anim.UpdateNodeColor(wifiApNode2.Get(0)->GetId(), 0, 128, 255);

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(20.0));

    if (tracing)
    {
        std::system("mkdir -p tp2");

        AsciiTraceHelper ascii;
        Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("tp2/tracemetrics");

        phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy2.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

        pointToPoint.EnablePcapAll("tp2/third-p2p");
        pointToPoint.EnableAsciiAll(stream);

        if (apDevices1.GetN() > 0)
        {
            phy1.EnablePcap("tp2/third-wifi1-ap", apDevices1.Get(0));
        }
        for (uint32_t i = 0; i < staDevices1.GetN(); ++i)
        {
            phy1.EnablePcap("tp2/third-wifi1-sta", staDevices1.Get(i));
        }
        phy1.EnableAsciiAll(stream);

        if (apDevices2.GetN() > 0)
        {
            phy2.EnablePcap("tp2/third-wifi2-ap", apDevices2.Get(0));
        }
        for (uint32_t i = 0; i < staDevices2.GetN(); ++i)
        {
            phy2.EnablePcap("tp2/third-wifi2-sta", staDevices2.Get(i));
        }
        phy2.EnableAsciiAll(stream);
    }

    std::cout << "Démarrage de la simulation..." << std::endl;
    std::cout << "Configuration: " << nWifi << " STA par réseau, " << nPackets << " paquets" << std::endl;

    Simulator::Run();

    clientTracker.ExportDelays("tp2/client_delays.csv");

    std::ofstream paramsFile("tp2/plot_params.txt");
    paramsFile << nWifi << "\n" << nPackets;
    paramsFile.close();

    std::cout << "Génération des graphiques..." << std::endl;
    std::cout << "Exécutez: python3 tp2/plot_delays.py pour générer les graphiques" << std::endl;

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::cout << "\n=== STATISTIQUES FLOW MONITOR ===" << std::endl;
    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        std::cout << "Flow " << it->first << ":" << std::endl;
        std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << it->second.lostPackets << std::endl;
        if (it->second.rxPackets > 0)
        {
            std::cout << "  Mean Delay: " << it->second.delaySum.GetMilliSeconds() / it->second.rxPackets << " ms" << std::endl;
            std::cout << "  Throughput: " << it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket - it->second.timeFirstTxPacket).GetSeconds() / 1000.0 << " kbps" << std::endl;
        }
    }

    if (!clientTracker.delays.empty())
    {
        std::cout << "\n=== STATISTIQUES DES DÉLAIS ===" << std::endl;
        double totalDelay = 0;
        double minDelay = clientTracker.delays[0].second.GetMilliSeconds();
        double maxDelay = clientTracker.delays[0].second.GetMilliSeconds();
        
        for (const auto& delay : clientTracker.delays)
        {
            double d = delay.second.GetMilliSeconds();
            totalDelay += d;
            if (d < minDelay) minDelay = d;
            if (d > maxDelay) maxDelay = d;
        }
        
        std::cout << "Délai moyen: " << totalDelay / clientTracker.delays.size() << " ms" << std::endl;
        std::cout << "Délai minimum: " << minDelay << " ms" << std::endl;
        std::cout << "Délai maximum: " << maxDelay << " ms" << std::endl;
        std::cout << "Nombre de paquets mesurés: " << clientTracker.delays.size() << std::endl;
    }

    Simulator::Destroy();
    
    std::cout << "\n=== SIMULATION TERMINÉE ===" << std::endl;
    std::cout << "Données des délais: tp2/client_delays.csv" << std::endl;
    std::cout << "Exécutez: python3 tp2/plot_delays.py pour les graphiques" << std::endl;
    
    return 0;
}