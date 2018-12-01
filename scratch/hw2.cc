
#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
//#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/drop-tail-queue.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpTcpVariantsComparison");

static bool firstCwnd = true;
static bool firstSshThr = true;
static bool firstRtt = true;
static bool firstRto = true;
static Ptr<OutputStreamWrapper> cWndStream;
static Ptr<OutputStreamWrapper> ssThreshStream;
static Ptr<OutputStreamWrapper> rttStream;
static Ptr<OutputStreamWrapper> rtoStream;
static Ptr<OutputStreamWrapper> nextTxStream;
static Ptr<OutputStreamWrapper> nextRxStream;
static Ptr<OutputStreamWrapper> inFlightStream;
static uint32_t cWndValue;
static uint32_t ssThreshValue;
static int countTcp =0;
static int countUdp = 0;
static int udp_packetloss_cnt =0;
static int tcp_packetloss_cnt =0;
double pastTimeTcp = 0;
double pastTimeUdp = 0;
std::ofstream tcp_rxFile;
std::ofstream udp_rxFile;
std::ofstream udp_packetlossFile;
std::ofstream tcp_packetlossFile;

int rtsFailed = 0;

class MyApp : public Application
{
  public:
    MyApp();
    virtual ~MyApp();
    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
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
};

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
   if(++m_packetsSent <m_nPackets)
   {
     ScheduleTx();
   }
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
  if(countUdp == 100){
	  throughput = countUdp*400*8/(Simulator::Now().GetSeconds()-pastTimeUdp);
	  udp_rxFile<<Simulator::Now().GetSeconds() <<"\t"<<throughput/1000<<"\n";
	  countUdp = 0;
	  pastTimeUdp = Simulator::Now().GetSeconds();
  }
}


static void
CwndTracer (uint32_t oldval, uint32_t newval)
{
  if (firstCwnd)
    {
      *cWndStream->GetStream () << "0.0 " << oldval << std::endl;
      firstCwnd = false;
    }
  *cWndStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
  cWndValue = newval;

  if (!firstSshThr)
    {
      *ssThreshStream->GetStream () << Simulator::Now ().GetSeconds () << " " << ssThreshValue << std::endl;
    }
}

static void
SsThreshTracer (uint32_t oldval, uint32_t newval)
{
  if (firstSshThr)
    {
      *ssThreshStream->GetStream () << "0.0 " << oldval << std::endl;
      firstSshThr = false;
    }
  *ssThreshStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
  ssThreshValue = newval;

  if (!firstCwnd)
    {
      *cWndStream->GetStream () << Simulator::Now ().GetSeconds () << " " << cWndValue << std::endl;
    }
}

static void
RttTracer (Time oldval, Time newval)
{
  if (firstRtt)
    {
      *rttStream->GetStream () << "0.0 " << oldval.GetSeconds () << std::endl;
      firstRtt = false;
    }
  *rttStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
}

static void
RtoTracer (Time oldval, Time newval)
{
  if (firstRto)
    {
      *rtoStream->GetStream () << "0.0 " << oldval.GetSeconds () << std::endl;
      firstRto = false;
    }
  *rtoStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
}

static void
NextTxTracer (SequenceNumber32 old, SequenceNumber32 nextTx)
{
  NS_UNUSED (old);
  *nextTxStream->GetStream () << Simulator::Now ().GetSeconds () << " " << nextTx << std::endl;
}

static void
InFlightTracer (uint32_t old, uint32_t inFlight)
{
  NS_UNUSED (old);
  *inFlightStream->GetStream () << Simulator::Now ().GetSeconds () << " " << inFlight << std::endl;
}

static void
NextRxTracer (SequenceNumber32 old, SequenceNumber32 nextRx)
{
  NS_UNUSED (old);
  *nextRxStream->GetStream () << Simulator::Now ().GetSeconds () << " " << nextRx << std::endl;
}

static void
TraceCwnd (std::string cwnd_tr_file_name)
{
  AsciiTraceHelper ascii;
  cWndStream = ascii.CreateFileStream (cwnd_tr_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer));
}

static void
TraceSsThresh (std::string ssthresh_tr_file_name)
{
  AsciiTraceHelper ascii;
  ssThreshStream = ascii.CreateFileStream (ssthresh_tr_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold", MakeCallback (&SsThreshTracer));
}

static void
TraceRtt (std::string rtt_tr_file_name)
{
  AsciiTraceHelper ascii;
  rttStream = ascii.CreateFileStream (rtt_tr_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RTT", MakeCallback (&RttTracer));
}

static void
TraceRto (std::string rto_tr_file_name)
{
  AsciiTraceHelper ascii;
  rtoStream = ascii.CreateFileStream (rto_tr_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RTO", MakeCallback (&RtoTracer));
}

static void
TraceNextTx (std::string &next_tx_seq_file_name)
{
  AsciiTraceHelper ascii;
  nextTxStream = ascii.CreateFileStream (next_tx_seq_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/NextTxSequence", MakeCallback (&NextTxTracer));
}

static void
TraceInFlight (std::string &in_flight_file_name)
{
  AsciiTraceHelper ascii;
  inFlightStream = ascii.CreateFileStream (in_flight_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/BytesInFlight", MakeCallback (&InFlightTracer));
}


static void
TraceNextRx (std::string &next_rx_seq_file_name)
{
  AsciiTraceHelper ascii;
  nextRxStream = ascii.CreateFileStream (next_rx_seq_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/2/$ns3::TcpL4Protocol/SocketList/1/RxBuffer/NextRxSequence", MakeCallback (&NextRxTracer));
}

static void
RxDrop(Ptr <const Packet> p)
{
  udp_packetloss_cnt = udp_packetloss_cnt+1;
  udp_packetlossFile <<Simulator::Now().GetSeconds() <<"\t"<<udp_packetloss_cnt<<"\n";
}

static void
RxDropTcp(Ptr <const Packet> p)
{
	tcp_packetloss_cnt = tcp_packetloss_cnt+1;
	tcp_packetlossFile << Simulator::Now().GetSeconds() <<"\t"<<udp_packetloss_cnt<<"\n";
}


int main (int argc, char *argv[])
{
  //Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue (100));
  //LogComponentEnable ("PacketLossCounter", LOG_LEVEL_INFO);
  //LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  tcp_rxFile.open("tcp_rx.txt");
  udp_rxFile.open("udp_rx.txt");
  udp_packetlossFile.open("udp_packetloss.txt");
  tcp_packetlossFile.open("tcp_packetloss.txt");
  double error_p = 0.00001;
  std::string bandwidth = "1Mbps";
  std::string delay = "1ms";
  std::string delay_core = "1ms";
  bool tracing = true;
  std::string prefix_file_name = "UdpTcpVariantsComparison";
  uint64_t data_mbytes = 0;
  uint32_t mtu_bytes = 400;
  std::string transport_prot = "TcpNewReno";
  transport_prot = std::string ("ns3::") + transport_prot;
  double duration = 30.0;
  CommandLine cmd;
  cmd.AddValue ("transport_prot", "Transport protocol to use: TcpNewReno, TcpVegas", transport_prot);
  cmd.AddValue ("error_p", "Packet error rate", error_p);
  cmd.AddValue ("bandwidth", "Bottleneck bandwidth", bandwidth);
  cmd.AddValue ("delay", "Bottleneck delay", delay);
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("prefix_name", "Prefix of output trace file", prefix_file_name);
  cmd.AddValue ("data", "Number of Megabytes of data to transmit", data_mbytes);
  cmd.AddValue ("mtu", "Size of IP packets to send in bytes", mtu_bytes);
  cmd.AddValue ("duration", "Time to allow flows to run in seconds", duration);
 
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
  
  NodeContainer cores;
  cores.Add(nodes.Get(2));
  cores.Add(nodes.Get(3));
  double start_time = 0.0;
  double stop_time = start_time + duration;

  // User may find it convenient to enable logging
  //LogComponentEnable("TcpVariantsComparison", LOG_LEVEL_ALL);
  //LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
  //LogComponentEnable("PfifoFastQueueDisc", LOG_LEVEL_ALL);


  // Configure the error model
  // Here we use RateErrorModel with packet error rate



  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth));
  p2p.SetChannelAttribute("Delay",StringValue(delay));

  PointToPointHelper p2p_core;
  p2p_core.SetDeviceAttribute("DataRate",StringValue(bandwidth));
  p2p_core.SetChannelAttribute("Delay",StringValue(delay_core));
  //p2p_core.SetQueue("ns3::DropTailQueue","MaxBytes",UintegerValue(125000));
  //p2p_core.SetQueue("ns3::DropTailQueue","Mode",EnumValue (DropTailQueue::QUEUE_MODE_BYTES),"MaxBytes",UintegerValue (125000));

 //Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
 // em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
  
  NetDeviceContainer coreDevices;
  coreDevices = p2p_core.Install(cores);
  NetDeviceContainer tcpLeftDevices;
  tcpLeftDevices = p2p.Install(tcpLeftNodes);
  NetDeviceContainer tcpRightDevices;
  tcpRightDevices = p2p.Install(tcpRightNodes);
  NetDeviceContainer udpLeftDevices;
  udpLeftDevices = p2p.Install(udpLeftNodes);
  NetDeviceContainer udpRightDevices;
  udpRightDevices = p2p.Install(udpRightNodes);

  
 // tcpGateway.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
 // udpGateway.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
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
  coreInterface = address.Assign(coreDevices);


  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  //Tcp sink app for Node 5
  uint16_t sinkPortTcp = 8080;
  Address sinkAddressTcp(InetSocketAddress(tcpRightInterface.GetAddress(1),sinkPortTcp));
  PacketSinkHelper packetSinkHelperTcp("ns3::TcpSocketFactory",InetSocketAddress(Ipv4Address::GetAny(), sinkPortTcp));
  ApplicationContainer sinkAppTcp = packetSinkHelperTcp.Install(tcpRightNodes.Get(1));
  sinkAppTcp.Start(Seconds(0.0));
  sinkAppTcp.Stop(Seconds(duration));
  sinkAppTcp.Get(0)->TraceConnectWithoutContext("Rx",MakeCallback(&RxTcp));
  //TCP TX app for Node 1
  //OnOffHelper tcpOnOff("ns3::TcpSocketFactory",Address(InetSocketAddress(tcpRightInterface.GetAddress(1),sinkPortTcp)));
  //tcpOnOff.SetAttribute("OnTime",StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  //tcpOnOff.SetAttribute("OffTime",StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  //tcpOnOff.SetAttribute("PacketSize",UintegerValue(512));
  //tcpOnOff.SetAttribute("DataRate",StringValue("1Mbps"));
  //ApplicationContainer tcpTxApp = tcpOnOff.Install(tcpLeftNodes.Get(0));
  //tcpTxApp.Start(Seconds(1.0));
  //tcpTxApp.Stop(Seconds(duration-1.0));
  Ptr<MyApp> tcpApp = CreateObject<MyApp> ();
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (tcpLeftNodes.Get (0), TcpSocketFactory::GetTypeId ());
  tcpApp->Setup (ns3TcpSocket, sinkAddressTcp, 512,10000, DataRate ("2Mbps"));
  tcpLeftNodes.Get (0)->AddApplication (tcpApp);
  tcpApp->SetStartTime (Seconds (1.));
  tcpApp->SetStopTime (Seconds (20.));
  
  uint16_t sinkPortUdp = 9090;
  Address sinkAddressUdp (InetSocketAddress(udpRightInterface.GetAddress(1),sinkPortUdp));
  PacketSinkHelper packetSinkHelperUdp("ns3::UdpSocketFactory",InetSocketAddress(Ipv4Address::GetAny(),sinkPortUdp));
  ApplicationContainer sinkAppUdp = packetSinkHelperUdp.Install(udpRightNodes.Get(1));
  sinkAppUdp.Start(Seconds(0.0));
  sinkAppUdp.Stop(Seconds(duration));
  sinkAppUdp.Get(0)->TraceConnectWithoutContext("Rx",MakeCallback(&RxUdp));
  //TCP TX app for Node 1
  //OnOffHelper udpOnOff("ns3::UdpSocketFactory",Address(InetSocketAddress(udpRightInterface.GetAddress(1),sinkPortUdp)));
  //udpOnOff.SetAttribute("OnTime",StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  //udpOnOff.SetAttribute("OffTime",StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  //udpOnOff.SetAttribute("PacketSize",UintegerValue(512));
  //udpOnOff.SetAttribute("DataRate",StringValue("1Mbps"));
  //ApplicationContainer udpTxApp = udpOnOff.Install(udpLeftNodes.Get(0));
  //udpTxApp.Start(Seconds(5.0));
  //udpTxApp.Stop(Seconds(8.0));
  Ptr<MyApp> udpApp = CreateObject<MyApp> ();
  Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (udpLeftNodes.Get (0),UdpSocketFactory::GetTypeId ());
  udpApp->Setup (ns3UdpSocket, sinkAddressUdp, 512, 10000, DataRate ("2Mbps"));
  udpLeftNodes.Get (0)->AddApplication (udpApp);
  udpApp->SetStartTime (Seconds (1.));
  udpApp->SetStopTime (Seconds (20.));
  udpLeftNodes.Get(1)->TraceConnectWithoutContext("PhyRxDrop",MakeCallback(&RxDrop));
  tcpRightNodes.Get(1)->TraceConnectWithoutContext("PhyRxDrop",MakeCallback(&RxDropTcp));
  // Set up tracing if enabled
  if (tracing)
    {
      std::ofstream ascii;
      Ptr<OutputStreamWrapper> ascii_wrap;
      ascii.open ((prefix_file_name + "-ascii").c_str ());
      ascii_wrap = new OutputStreamWrapper ((prefix_file_name + "-ascii").c_str (),
                                            std::ios::out);
      stack.EnableAsciiIpv4All (ascii_wrap);

      Simulator::Schedule (Seconds (0.00001), &TraceCwnd, prefix_file_name + "-cwnd.txt");
      Simulator::Schedule (Seconds (0.00001), &TraceSsThresh, prefix_file_name + "-ssth.txt");
      Simulator::Schedule (Seconds (0.00001), &TraceRtt, prefix_file_name + "-rtt.txt");
      Simulator::Schedule (Seconds (0.00001), &TraceRto, prefix_file_name + "-rto.txt");
      Simulator::Schedule (Seconds (0.00001), &TraceNextTx, prefix_file_name + "-next-tx.txt");
      Simulator::Schedule (Seconds (0.00001), &TraceInFlight, prefix_file_name + "-inflight.txt");
      Simulator::Schedule (Seconds (0.1), &TraceNextRx, prefix_file_name + "-next-rx.txt");
    }
  Ptr<FlowMonitor> flowMonitor; 
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  Simulator::Stop (Seconds (stop_time));
  Simulator::Run ();
  flowMonitor->SerializeToXmlFile ("results.xml",false,false);


  Simulator::Destroy ();
  tcp_rxFile.close();
  udp_rxFile.close();
  udp_packetlossFile.close();
  return 0;
}
