
#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpTcpVariantsComparison");

static uint32_t countTcp =0;
static uint32_t countUdp = 0;
static int udp_packetloss_cnt =0;
static int tcp_packetloss_cnt =0;
static uint32_t udpTxPacketNum=0;
static uint32_t tcpTxPacketNum=0;
static uint32_t udpRxPacketNum=0;
static uint32_t tcpRxPacketNum=0;
double pastTimeTcp = 0;
double pastTimeUdp = 0;
std::ofstream tcp_rxFile;
std::ofstream udp_rxFile;
std::ofstream udp_packetlossFile;
std::ofstream tcp_packetlossFile;
std::ofstream tcp_cwndFile;

class MyApp : public Application
{
  public:
    static TypeId GetTypeId (void);
    MyApp();
    virtual ~MyApp();
    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
    uint32_t GetTxNum(void);
  private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void ScheduleTx(void);
    void SendPacket(void);
    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
    TracedCallback<Ptr<const Packet> > m_txTrace;
    TracedCallback<Ptr<const Packet> > m_rxTrace;
};
TypeId
MyApp::GetTypeId (void) {
  static TypeId tid = TypeId ("ns3::MyApp")
    .SetParent<Application> ()
    .AddConstructor<MyApp> () 
    .AddAttribute ("NPackets", "The total number of packets to send",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&MyApp::m_nPackets),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("DataRate", "The data rate",
                   DataRateValue (DataRate ("500kb/s")),
                   MakeDataRateAccessor (&MyApp::m_dataRate),
                   MakeDataRateChecker ())
		.AddTraceSource("Tx", "A new packet is created and is sent", MakeTraceSourceAccessor(&MyApp::m_txTrace),"ns3::Packet::TracedCallback")
		.AddTraceSource("Rx", "A packet has been received", MakeTraceSourceAccessor (&MyApp::m_rxTrace),"ns3::Packet::TracedCallback")
 ;
  return tid;
}
MyApp::MyApp()
  : m_socket(0),
    m_peer(),
    m_packetSize(0),
    m_nPackets(0),
    m_dataRate(0),
    m_sendEvent(),
    m_running(false),
    m_packetsSent(0)
{
}

MyApp::~MyApp()
{
   m_socket =0;
}

void MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
   m_socket = socket;
   m_peer = address;
   m_packetSize = packetSize;
   m_nPackets = nPackets;
   m_dataRate = dataRate;
}

void MyApp::StartApplication(void)
{
   m_running = true;
   m_packetsSent = 0;
   m_socket->Bind();
   m_socket->Connect(m_peer);
   SendPacket();
}
void MyApp::StopApplication(void)
{
   m_running = false;
   if(m_sendEvent.IsRunning())
     {
       Simulator::Cancel(m_sendEvent);
     }
   if(m_socket)
     {
       m_socket->Close();
     }
}

void MyApp::SendPacket(void)
{
   Ptr<Packet> packet = Create<Packet> (m_packetSize);
   m_socket->Send(packet);
   m_txTrace(packet);
   if(++m_packetsSent <m_nPackets)
   {
     ScheduleTx();
   }
}
uint32_t MyApp::GetTxNum(void)
{
	return m_packetsSent;
}
void MyApp::ScheduleTx(void)
{
   if(m_running)
   {
     Time tNext(Seconds(m_packetSize*8/static_cast <double> (m_dataRate.GetBitRate())));
     m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
   }
}
static void
RxTcp (Ptr <const Packet> packet, const Address &address)
{
  double throughput = 0;
  countTcp = countTcp+1;
  tcpRxPacketNum = tcpRxPacketNum+1;
  tcp_packetlossFile<<Simulator::Now().GetSeconds()<<"\t"<<tcpTxPacketNum<<"\t"<< tcpRxPacketNum<<"\n";
  if(countTcp == 100){
	  throughput = countTcp*400*8/(Simulator::Now().GetSeconds()-pastTimeTcp);
	  tcp_rxFile<<Simulator::Now().GetSeconds() <<"\t"<<throughput/1000<<"\n";
	  countTcp = 0;
	  pastTimeTcp = Simulator::Now().GetSeconds();
  }
}

static void
RxUdp (Ptr <const Packet> packet, const Address &address)
{
  double throughput = 0;
  countUdp = countUdp+1;
  udpRxPacketNum = udpRxPacketNum+1;
  udp_packetlossFile <<Simulator::Now().GetSeconds() <<"\t"<<udpTxPacketNum<<"\t"<< udpRxPacketNum<<"\n";
  if(countUdp == 100){
	  throughput = countUdp*400*8/(Simulator::Now().GetSeconds()-pastTimeUdp);
	  udp_rxFile<<Simulator::Now().GetSeconds() <<"\t"<<throughput/1000<<"\n";
	  countUdp = 0;
	  pastTimeUdp = Simulator::Now().GetSeconds();
  }
}

static void
RxDrop(Ptr <const Packet> p)
{
  udp_packetloss_cnt = udp_packetloss_cnt+1;
//  udp_packetlossFile <<Simulator::Now().GetSeconds() <<"\t"<<udp_packetloss_cnt<<"\n";
}

static void
RxDropTcp(Ptr <const Packet> p)
{
	tcp_packetloss_cnt = tcp_packetloss_cnt+1;
//	tcp_packetlossFile << Simulator::Now().GetSeconds() <<"\t"<<udp_packetloss_cnt<<"\n";
}

static void
CwndChange(uint32_t oldCwnd, uint32_t newCwnd)
{
	tcp_cwndFile << Simulator::Now().GetSeconds()<<"\t"<<newCwnd<<"\n";
}

static void
PacketTxUdp(Ptr<const Packet> p)
{
  udpTxPacketNum ++;
//NS_LOG_UNCOND (Simulator::Now().GetSeconds() << "\t" << "A new packet is sent at Node 0");
}
static void
PacketTxTcp(Ptr<const Packet> p)
{
	tcpTxPacketNum++;
}	
int main (int argc, char *argv[])
{
  //Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue (100));
  //LogComponentEnable ("PacketLossCounter", LOG_LEVEL_INFO);
  //LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
  //LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  tcp_rxFile.open("tcp_rx.txt");
  udp_rxFile.open("udp_rx.txt");
  udp_packetlossFile.open("udp_packetloss.txt");
  tcp_packetlossFile.open("tcp_packetloss.txt");
  tcp_cwndFile.open("tcp_cwnd.txt");
  double error_p = 0.00001;
  std::string bandwidth = "1Mbps";
  std::string delay = "1ms";
  std::string delay_core = "1ms";
  std::string transport_prot = "TcpNewReno";
  transport_prot = std::string ("ns3::") + transport_prot;
  double startTime= 0.0;
  double endTime = 30.0;
  CommandLine cmd;
  cmd.AddValue ("transport_prot", "Transport protocol to use: TcpNewReno, TcpVegas", transport_prot);
  cmd.AddValue ("endTime", "Time to allow flows to run in seconds", endTime);
 
  cmd.Parse(argc, argv);
  
  TypeId tcpTid;
  NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (transport_prot, &tcpTid), "TypeId " << transport_prot << " not found");
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (transport_prot)));
  
  NodeContainer nodes;
  nodes.Create(6);

  //Node 1, Node 3, Node 4, Node 5
  NodeContainer tcpLeftNodes;
  tcpLeftNodes.Add(nodes.Get(0));
  tcpLeftNodes.Add(nodes.Get(2));
  NodeContainer tcpRightNodes;
  tcpRightNodes.Add(nodes.Get(3));
  tcpRightNodes.Add(nodes.Get(4));


  //Node 2, Node 3, Node 4, Node 6
  NodeContainer udpLeftNodes;
  udpLeftNodes.Add(nodes.Get(1));
  udpLeftNodes.Add(nodes.Get(2));
  NodeContainer udpRightNodes;
  udpRightNodes.Add(nodes.Get(3));
  udpRightNodes.Add(nodes.Get(5));
  
  NodeContainer routerNodes;
  routerNodes.Add(nodes.Get(2));
  routerNodes.Add(nodes.Get(3));
   


  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth));
  p2p.SetChannelAttribute("Delay",StringValue(delay));

  PointToPointHelper p2p_router;
  p2p_router.SetDeviceAttribute("DataRate",StringValue(bandwidth));
  p2p_router.SetChannelAttribute("Delay",StringValue(delay_core));

   Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
   em->SetAttribute ("ErrorRate", DoubleValue (error_p));
  
  NetDeviceContainer routerDevices;
  routerDevices = p2p_router.Install(routerNodes);
  NetDeviceContainer tcpLeftDevices;
  tcpLeftDevices = p2p.Install(tcpLeftNodes);
  NetDeviceContainer tcpRightDevices;
  tcpRightDevices = p2p.Install(tcpRightNodes);
  NetDeviceContainer udpLeftDevices;
  udpLeftDevices = p2p.Install(udpLeftNodes);
  NetDeviceContainer udpRightDevices;
  udpRightDevices = p2p.Install(udpRightNodes);
  
  tcpRightDevices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
  udpRightDevices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0","255.255.255.0");

  Ipv4InterfaceContainer tcpLeftInterface;
  tcpLeftInterface = address.Assign(tcpLeftDevices);
  address.SetBase("10.1.2.0","255.255.255.0");
  Ipv4InterfaceContainer tcpRightInterface;
  tcpRightInterface = address.Assign(tcpRightDevices);

  address.SetBase("10.2.1.0","255.255.255.0");
  Ipv4InterfaceContainer udpLeftInterface;
  udpLeftInterface = address.Assign(udpLeftDevices);

  address.SetBase("10.2.2.0","255.255.255.0");
  Ipv4InterfaceContainer udpRightInterface;
  udpRightInterface = address.Assign(udpRightDevices);

  address.SetBase("10.3.1.0","255.255.255.0");
  Ipv4InterfaceContainer coreInterface;
  coreInterface = address.Assign(routerDevices);


  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  //Tcp sink app for Node 5
  uint16_t sinkPortTcp = 8080;
  Address sinkAddressTcp(InetSocketAddress(tcpRightInterface.GetAddress(1),sinkPortTcp));
  PacketSinkHelper packetSinkHelperTcp("ns3::TcpSocketFactory",InetSocketAddress(Ipv4Address::GetAny(), sinkPortTcp));
  ApplicationContainer sinkAppTcp = packetSinkHelperTcp.Install(tcpRightNodes.Get(1));
  sinkAppTcp.Start(Seconds(startTime));
  sinkAppTcp.Stop(Seconds(endTime));
  sinkAppTcp.Get(0)->TraceConnectWithoutContext("Rx",MakeCallback(&RxTcp));
  
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (tcpLeftNodes.Get (0), TcpSocketFactory::GetTypeId ());
  ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow",MakeCallback(&CwndChange));
  Ptr<MyApp> tcpApp = CreateObject<MyApp> ();
  tcpApp->Setup (ns3TcpSocket, sinkAddressTcp, 1040,100000, DataRate ("1Mbps"));
  tcpLeftNodes.Get (0)->AddApplication (tcpApp);
  tcpApp->SetStartTime (Seconds (startTime+1.0));
  tcpApp->SetStopTime (Seconds (endTime));
  tcpApp->TraceConnectWithoutContext("Tx",MakeCallback(&PacketTxTcp));
  uint16_t sinkPortUdp = 9090;
  Address sinkAddressUdp (InetSocketAddress(udpRightInterface.GetAddress(1),sinkPortUdp));
  PacketSinkHelper packetSinkHelperUdp("ns3::UdpSocketFactory",InetSocketAddress(Ipv4Address::GetAny(),sinkPortUdp));
  ApplicationContainer sinkAppUdp = packetSinkHelperUdp.Install(udpRightNodes.Get(1));
  sinkAppUdp.Start(Seconds(startTime));
  sinkAppUdp.Stop(Seconds(endTime));
  sinkAppUdp.Get(0)->TraceConnectWithoutContext("Rx",MakeCallback(&RxUdp));
  
  Ptr<MyApp> udpApp = CreateObject<MyApp> ();
  Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (udpLeftNodes.Get (0),UdpSocketFactory::GetTypeId ());
  udpApp->Setup (ns3UdpSocket, sinkAddressUdp, 1040, 100000, DataRate ("1Mbps"));
  udpLeftNodes.Get (0)->AddApplication (udpApp);
  udpApp->SetStartTime (Seconds (startTime+1.0));
  udpApp->SetStopTime (Seconds (endTime));
  udpRightDevices.Get(1)->TraceConnectWithoutContext("PhyRxDrop",MakeCallback(&RxDrop));
  tcpRightDevices.Get(1)->TraceConnectWithoutContext("PhyRxDrop",MakeCallback(&RxDropTcp));
  udpApp->TraceConnectWithoutContext("Tx", MakeCallback(&PacketTxUdp));
  
  Ptr<FlowMonitor> flowMonitor; 
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  Simulator::Stop (Seconds (endTime+1.0));
  Simulator::Run ();
  //flowMonitor->SerializeToXmlFile ("results.xml",false,false);
  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  { 
 	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
	  if ((t.sourceAddress=="10.1.1.1" && t.destinationAddress == "10.1.2.2")||(t.sourceAddress == "10.2.1.1" && t.destinationAddress == "10.2.2.2"))
	       {
	            std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
	            std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
	            std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
	        	std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps\n";
	        }
	 
  }

  Simulator::Destroy ();
  tcp_rxFile.close();
  udp_rxFile.close();
  udp_packetlossFile.close();
  return 0;
}
