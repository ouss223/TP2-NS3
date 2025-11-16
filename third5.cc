#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MimoAnalysis");

int main(int argc, char *argv[])
{
    uint32_t spatialStreams = 1;
    double simulationTime = 10.0;
    bool enableAnimation = false;
    double distance = 10.0;
    uint32_t channelWidth = 20; // Default: 20 MHz

    CommandLine cmd(__FILE__);
    cmd.AddValue("spatialStreams", "Number of spatial streams (1 or 2)", spatialStreams);
    cmd.AddValue("time", "Simulation time in seconds", simulationTime);
    cmd.AddValue("animation", "Enable NetAnim", enableAnimation);
    cmd.AddValue("distance", "Distance between STA and AP in meters", distance);
    cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
    cmd.Parse(argc, argv);

    // Validate channel width
    if (channelWidth != 20 && channelWidth != 40) {
        std::cout << "ERROR: Channel width must be 20 or 40 MHz. Using default 20 MHz." << std::endl;
        channelWidth = 20;
    }

    std::cout << "=== ANALYSE MIMO 802.11n ===" << std::endl;
    std::cout << "Flux spatiaux: " << spatialStreams << std::endl;
    std::cout << "Distance STA-AP: " << distance << " m" << std::endl;
    std::cout << "Largeur de canal: " << channelWidth << " MHz" << std::endl;

    // Création des nœuds
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configuration WiFi avec modèle de perte réaliste
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    
    // Modèle de perte réaliste avec shadowing
    channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                              "Exponent", DoubleValue(3.5),
                              "ReferenceLoss", DoubleValue(40.0));
    
    // Ajouter un modèle de fading
    channel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    
    // Configuration réaliste de la puissance
    phy.Set("TxPowerStart", DoubleValue(20.0));
    phy.Set("TxPowerEnd", DoubleValue(20.0));
    phy.Set("RxSensitivity", DoubleValue(-82.0));
    phy.Set("CcaEdThreshold", DoubleValue(-62.0));
    phy.Set("TxGain", DoubleValue(2.0));
    phy.Set("RxGain", DoubleValue(2.0));

    // Configuration de la largeur de canal
    phy.Set("ChannelSettings", StringValue("{0, " + std::to_string(channelWidth) + ", BAND_5GHZ, 0}"));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    
    // Utiliser un gestionnaire adaptatif
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    // Configuration MIMO explicite
    if (spatialStreams == 2) {
        phy.Set("Antennas", UintegerValue(2));
        phy.Set("MaxSupportedTxSpatialStreams", UintegerValue(2));
        phy.Set("MaxSupportedRxSpatialStreams", UintegerValue(2));
        std::cout << "Configuration: 2x2 MIMO" << std::endl;
    } else {
        phy.Set("Antennas", UintegerValue(1));
        phy.Set("MaxSupportedTxSpatialStreams", UintegerValue(1));
        phy.Set("MaxSupportedRxSpatialStreams", UintegerValue(1));
        std::cout << "Configuration: 1x1 MIMO" << std::endl;
    }

    WifiMacHelper mac;
    mac.SetType("ns3::StaWifiMac", "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);
    mac.SetType("ns3::ApWifiMac");
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobilité
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(distance, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    // Pile Internet
    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Configuration des applications
    uint16_t port = 5000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    UdpClientHelper client(apInterface.GetAddress(0), port);
    
    // CALCUL DU DÉBIT APPROPRIÉ - AJUSTÉ POUR CHANNEL BONDING
    double targetDataRate;
    if (channelWidth == 40) {
        // Débits plus élevés avec 40MHz
        if (spatialStreams == 1) {
            if (distance <= 20) targetDataRate = 90.0;   // ~2x 20MHz
            else if (distance <= 50) targetDataRate = 60.0;
            else targetDataRate = 30.0;
        } else {
            if (distance <= 20) targetDataRate = 180.0;  // ~2x 20MHz
            else if (distance <= 50) targetDataRate = 120.0;
            else targetDataRate = 60.0;
        }
    } else {
        // 20MHz (défaut)
        if (spatialStreams == 1) {
            if (distance <= 20) targetDataRate = 45.0;
            else if (distance <= 50) targetDataRate = 30.0;
            else targetDataRate = 15.0;
        } else {
            if (distance <= 20) targetDataRate = 90.0;
            else if (distance <= 50) targetDataRate = 60.0;
            else targetDataRate = 30.0;
        }
    }
    
    // Conversion du débit en intervalle entre paquets
    uint32_t packetSize = 1470; // bytes
    double interval = (packetSize * 8.0) / (targetDataRate * 1e6);
    
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    
    ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime - 1.0));

    // Animation optionnelle
    if (enableAnimation) {
        AnimationInterface anim("mimo_animation.xml");
        anim.SetConstantPosition(wifiStaNode.Get(0), 0, 0);
        anim.SetConstantPosition(wifiApNode.Get(0), distance, 0);
    }

    // Métriques avec FlowMonitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Analyse des résultats
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    double throughput = 0.0;
    double packetLoss = 100.0;
    uint64_t totalRxPackets = 0;
    uint64_t totalTxPackets = 0;
    uint64_t totalRxBytes = 0;
    
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        auto flowStats = it->second;
        totalRxPackets += flowStats.rxPackets;
        totalTxPackets += flowStats.txPackets;
        totalRxBytes += flowStats.rxBytes;
        
        if (flowStats.rxPackets > 0) {
            double duration = (flowStats.timeLastRxPacket - flowStats.timeFirstTxPacket).GetSeconds();
            if (duration > 0) {
                throughput = (flowStats.rxBytes * 8.0) / duration / 1e6;
            }
            packetLoss = (flowStats.lostPackets * 100.0) / flowStats.txPackets;
        }
    }

    // Débit théorique réaliste - AJUSTÉ POUR CHANNEL BONDING
    double theoreticalThroughput;
    if (channelWidth == 40) {
        // 40MHz: débits théoriques environ doublés
        if (spatialStreams == 1) {
            if (distance <= 20) theoreticalThroughput = 135.0;  // ~2x 65Mbps
            else if (distance <= 50) theoreticalThroughput = 78.0;   // ~2x 39Mbps
            else theoreticalThroughput = 39.0;   // ~2x 19.5Mbps
        } else {
            if (distance <= 20) theoreticalThroughput = 270.0;  // ~2x 130Mbps
            else if (distance <= 50) theoreticalThroughput = 150.0;  // ~2x 72.2Mbps
            else theoreticalThroughput = 57.8;   // ~2x 28.9Mbps
        }
    } else {
        // 20MHz (défaut)
        if (spatialStreams == 1) {
            if (distance <= 20) theoreticalThroughput = 65.0;
            else if (distance <= 50) theoreticalThroughput = 39.0;
            else theoreticalThroughput = 19.5;
        } else {
            if (distance <= 20) theoreticalThroughput = 130.0;
            else if (distance <= 50) theoreticalThroughput = 72.2;
            else theoreticalThroughput = 28.9;
        }
    }
    
    double efficiency = (throughput > 0) ? (throughput / theoreticalThroughput) * 100 : 0.0;

    std::cout << "\n=== RÉSULTATS ===" << std::endl;
    std::cout << "Distance: " << distance << " m" << std::endl;
    std::cout << "Largeur canal: " << channelWidth << " MHz" << std::endl;
    std::cout << "Débit théorique: " << theoreticalThroughput << " Mbps" << std::endl;
    std::cout << "Débit mesuré: " << throughput << " Mbps" << std::endl;
    std::cout << "Efficacité: " << efficiency << "%" << std::endl;
    std::cout << "Paquets reçus: " << totalRxPackets << std::endl;
    std::cout << "Paquets envoyés: " << totalTxPackets << std::endl;
    std::cout << "Taux de perte: " << packetLoss << "%" << std::endl;
    
    // Analyse de la qualité du lien
    if (packetLoss < 5.0) {
        std::cout << "✅ Lien excellent" << std::endl;
    } else if (packetLoss < 20.0) {
        std::cout << "⚠️  Lien acceptable" << std::endl;
    } else {
        std::cout << "❌ Lien critique" << std::endl;
    }
    
    // Analyse channel bonding
    if (channelWidth == 40) {
        std::cout << "🔊 Channel Bonding 40MHz activé" << std::endl;
    }
    
    if (spatialStreams == 2 && throughput > 0) {
        double gain = (throughput / theoreticalThroughput) * 100;
        std::cout << "📈 Gain MIMO: " << gain << "% d'efficacité" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}