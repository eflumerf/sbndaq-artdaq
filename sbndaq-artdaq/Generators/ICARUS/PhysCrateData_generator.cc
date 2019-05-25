#define TRACE_NAME "PhysCrateData"
#include "sbndaq-artdaq/Generators/ICARUS/PhysCrateData.hh"
#include "artdaq/Generators/GeneratorMacros.hh"

#include <cerrno>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "sbndaq-artdaq-core/Trace/trace_defines.h"

#include "icarus-artdaq-base/PhysCrate.h"
#include "icarus-artdaq-base/A2795.h"
#include "CAENComm.h"

icarus::PhysCrateData::PhysCrateData(fhicl::ParameterSet const & ps)
  :
  PhysCrate_GeneratorBase(ps),
  veto_host(ps.get<std::string>("VetoHost")),
  veto_host_port(ps.get<int>("VetoPort")),
  veto_udp(veto_host.c_str(),veto_host_port)
{
  InitializeHardware();
  InitializeVeto();

  // Set up worker getdata thread.
  share::ThreadFunctor functor = std::bind(&PhysCrateData::GetData, this);
  auto worker_functor = share::WorkerThreadFunctorUPtr(new share::WorkerThreadFunctor(functor,"GetDataWorkerThread"));
  auto GetData_worker = share::WorkerThread::createWorkerThread(worker_functor);
  GetData_thread_.swap(GetData_worker);

  //some config things ...
  SetDCOffset();
  SetTestPulse();
}

void icarus::PhysCrateData::InitializeVeto(){
  //veto_state     = true; //defaults to on
  veto_state = false;
  
  TRACE(TR_LOG,"IP ADDRESS for veto is %s:%d\n",veto_host.c_str(),veto_host_port);
  
  _doVetoTest    = ps_.get<bool>("DoVetoTest",false);
  if(_doVetoTest){
    _vetoTestPeriod = ps_.get<unsigned int>("VetoTestPeriod",1e6);
    share::ThreadFunctor functor = std::bind(&PhysCrateData::VetoTest,this);
    auto worker_functor = share::WorkerThreadFunctorUPtr(new share::WorkerThreadFunctor(functor,"VetoTestWorkerThread"));
    auto veto_worker = share::WorkerThread::createWorkerThread(worker_functor);
    _vetoTestThread.swap(veto_worker);
  }
}

void icarus::PhysCrateData::SetDCOffset()
{
  uint16_t dc_offset_a = ps_.get<uint16_t>("DCOffsetA",0x0000);
  uint16_t dc_offset_b = ps_.get<uint16_t>("DCOffsetB",0x0000);
  uint16_t dc_offset_c = ps_.get<uint16_t>("DCOffsetC",0x0000);
  uint16_t dc_offset_d = ps_.get<uint16_t>("DCOffsetD",0x0000);

  for(int ib=0; ib<physCr->NBoards(); ++ib){
    auto bdhandle = physCr->BoardHandle(ib);
    
    CAENComm_Write32(bdhandle, A_DAC_A, 0x00070000 | dc_offset_a);
    CAENComm_Write32(bdhandle, A_DAC_B, 0x00070000 | dc_offset_b);
    CAENComm_Write32(bdhandle, A_DAC_C, 0x00070000 | dc_offset_c);
    CAENComm_Write32(bdhandle, A_DAC_D, 0x00070000 | dc_offset_d);
  }
}

void icarus::PhysCrateData::SetTestPulse()
{
  //TestPulseType tp_config = ps_.get<TestPulseType>("TestPulseType",TestPulseType::kDisable);
  int tp_config = ps_.get<int>("TestPulseType",0);
  uint16_t dc_offset = ps_.get<uint16_t>("DCOffsetTestPulse",0x0000);

  for(int ib=0; ib<physCr->NBoards(); ++ib){
    auto bdhandle = physCr->BoardHandle(ib);

    if(tp_config==0)
      CAENComm_Write32(bdhandle, A_RELE, RELE_TP_DIS);
    else if (tp_config==1)
      CAENComm_Write32(bdhandle, A_RELE, RELE_TP_EXT);
    else if (tp_config==2){
      CAENComm_Write32(bdhandle, A_RELE, RELE_TP_INT);
      sleep(1);
      CAENComm_Write32(bdhandle, A_RELE, RELE_TP_EVEN);
    }
    else if (tp_config==3){
      CAENComm_Write32(bdhandle, A_RELE, RELE_TP_INT);
      sleep(1);
      CAENComm_Write32(bdhandle, A_RELE, RELE_TP_ODD);
    }
    
    //set the test pulse dc offset
    CAENComm_Write32(bdhandle, A_DAC_CTRL, 0x00070000 | dc_offset);
  
  }

}
  
void icarus::PhysCrateData::VetoOn(){
  TRACEN("PhysCrateData",TLVL_DEBUG,"VetoOn called.");

  int result = veto_udp.VetoOn();
  TRACEN("PhysCrateData",TLVL_DEBUG,"VetoOn called. Result %d",result);
  if(result<0)
    TRACEN("PhysCrateData",TLVL_DEBUG,"VetoOn Error: %s",std::strerror(errno));

  veto_state = true;
}

void icarus::PhysCrateData::VetoOff(){
  TRACEN("PhysCrateData",TLVL_DEBUG,"VetoOff called.");

  int result = veto_udp.VetoOff();
  TRACEN("PhysCrateData",TLVL_DEBUG,"VetoOff called. Result %d",result);
  if(result<0)
    TRACEN("PhysCrateData",TLVL_DEBUG,"VetoOff Error: %s",std::strerror(errno));

  veto_state = false;
}

void icarus::PhysCrateData::InitializeHardware(){
  physCr = std::make_unique<PhysCrate>();
  physCr->initialize(pcieLinks_);
}

BoardConf icarus::PhysCrateData::GetBoardConf(){

  auto const& ps_board = ps_.get<fhicl::ParameterSet>("BoardConfig");

  BoardConf config;
  config.sampInterval = 1e-9;
  config.delayTime = 0.0;
  config.nbrSegments = 1;
  config.coupling = 3;
  config.bandwidth = 0;
  config.fullScale = ps_board.get<double>("fullScale")*0.001;
  config.thresh = ps_board.get<int>("thresh");
  config.offset = ps_board.get<int>("offset") * config.fullScale/256;
  config.offsetadc = ps_board.get<int>("offset");

  return config;
}

TrigConf icarus::PhysCrateData::GetTrigConf(){

  auto const& ps_trig = ps_.get<fhicl::ParameterSet>("TriggerConfig");

  TrigConf config;
  config.trigClass = 0; // 0: Edge trigger
  config.sourcePattern = 0x00000002; // 0x00000001: channel 1, 0x00000002: channel 2
  config.trigCoupling = 0;            
  config.channel = 2;
  config.trigSlope = 0; // 0: positive, 1: negative
  config.trigLevel1 = -20.0; // In % of vertical full scale or mV if using an external trigger source.
  config.trigLevel2 = 0.0;
  config.nsamples = ps_trig.get<int>("mode")*1000 ;
  config.presamples = ps_trig.get<int>("trigmode")*1000 ;

  return config;
}

void icarus::PhysCrateData::ConfigureStart(){

  _tloop_start = std::chrono::high_resolution_clock::now();
  _tloop_end = std::chrono::high_resolution_clock::now();

  //physCr->configureTrig(GetTrigConf());
  //physCr->configure(GetBoardConf());
  //VetoOff();
  physCr->start();

  if(_doVetoTest)
    _vetoTestThread->start();

  GetData_thread_->start();
}

void icarus::PhysCrateData::ConfigureStop(){
  if(_doVetoTest)
    _vetoTestThread->stop();

  GetData_thread_->stop();
}

bool icarus::PhysCrateData::Monitor(){ 
  //usleep(1e5);
  /*  
  if(veto_state)
    usleep(1.5e6);
  */
  /*
  if(!veto_state){
    bool need_to_veto = false;
    
    for(int ib=0; ib<physCr->NBoards(); ++ib){
      auto status = physCr->BoardStatus(ib);
      
      std::string varname = ".Board_"+std::to_string(ib)+"_Status.last";
      //GAL: metricMan->sendMetric(varname,status,"Status",1,artdaq::MetricMode::LastPoint);
      
      if( (status & STATUS_BUSY)!=0){
	TRACE(TR_ERROR,"PhysCrateData::Monitor : STATUS_BUSY on board %d!",ib);
	need_to_veto = true;
	break;
      }
    }
    
    if(need_to_veto && !veto_state)
      VetoOn();
    //else if(veto_state && !need_to_veto)
    //VetoOff();
  }
  
  if(veto_state)
    //GAL: metricMan->sendMetric(".VetoState.last",1,"state",1,artdaq::MetricMode::LastPoint);
  else
    //GAL: metricMan->sendMetric(".VetoState.last",0,"state",1,artdaq::MetricMode::LastPoint);    
  */
  for(int ib=0; ib<physCr->NBoards(); ++ib){
    auto status = physCr->BoardStatus(ib);
    std::cout << "Board " << ib << " status is " << status << std::endl;
  }
  usleep(10000000);
  return true; 
}

bool icarus::PhysCrateData::VetoTest(){
  
  if(veto_state)
    VetoOff();
  else
    VetoOn();
  usleep(_vetoTestPeriod);
  
  return true;
}

int icarus::PhysCrateData::GetData(){

  TRACEN("PhysCrateData",TLVL_DEBUG,"GetData called.");

  physCr->ArmTrigger();

  size_t data_size_bytes = 0;

  //end loop timer
  _tloop_end = std::chrono::high_resolution_clock::now();
  UpdateDuration();
  TRACEN("PhysCrateData",TR_TIMER,"GetData : waitData loop time was %lf seconds",_tloop_duration.count());
  //GAL: metricMan->sendMetric(".GetData.ReturnTime.last",_tloop_duration.count()*1000.,"ms",1,artdaq::MetricMode::LastPoint);
  //GAL: metricMan->sendMetric(".GetData.ReturnTime.max",_tloop_duration.count()*1000.,"ms",1,artdaq::MetricMode::LastPoint);

  TRACEN("PhysCrateData",TLVL_DEBUG,"GetData : Calling waitData()");
  physCr->waitData();

  //start loop timer
  _tloop_start = std::chrono::high_resolution_clock::now();

  _tloop_duration = std::chrono::duration_cast< std::chrono::duration<double> >(_tloop_end-_tloop_start);
  TRACEN("PhysCrateData",TR_TIMER,"GetData : waitData call time was %lf seconds",_tloop_duration.count());
  //GAL: metricMan->sendMetric(".GetData.WaitTime.last",_tloop_duration.count()*1000.,"ms",1,artdaq::MetricMode::LastPoint);
  //GAL: metricMan->sendMetric(".GetData.WaitTime.max",_tloop_duration.count()*1000.,"ms",1,artdaq::MetricMode::LastPoint);

  // Yun-Tse: ugly and tentative workaround at this moment...  need to change!!
  // int iBoard = 0, nBoards = 2;
      
  while(physCr->dataAvail()){
    TRACEN("PhysCrateData",TLVL_DEBUG,"GetData : DataAvail!");
    auto data_ptr = physCr->getData();
    
    size_t const this_data_size_bytes = ntohl( data_ptr->Header.packSize );
    TRACEN("PhysCrateData",TLVL_DEBUG,"GetData : Data acquired! Size is %lu bytes, with %lu bytes already acquired.",
        this_data_size_bytes, data_size_bytes);

    if( this_data_size_bytes == 32 ) continue;

    // ++iBoard;
    
    TLOG(TLVL_DEBUG) << "PhysCrateData: data_size_bytes: " << std::dec << data_size_bytes 
              << ", this_data_size_bytes: " << this_data_size_bytes
              << ", token: " << std::hex << data_ptr->Header.token << ", info1: " << data_ptr->Header.info1 
              << ", info2: " << data_ptr->Header.info2 << ", info3: " << data_ptr->Header.info3 
              << ", timeinfo: " << data_ptr->Header.timeinfo << ", chID: " << data_ptr->Header.chID << std::endl;
   
    // auto ev_ptr = reinterpret_cast<uint32_t*>(data_ptr->data);    
    // TRACEN("PhysCrateData",TLVL_DEBUG,"GetData : Data event number is %#8X",*ev_ptr);
    
    auto const* board_block = reinterpret_cast< A2795DataBlock const * >( data_ptr->data );
    TLOG(TLVL_DEBUG) << "PhysCrateData: event_number: " << board_block->header.event_number 
              << ", time_stamp: " << board_block->header.time_stamp << std::endl;

    // if ( iBoard == nBoards ) {
    //   fCircularBuffer.Insert( data_size, reinterpret_cast<uint16_t const*>(data_ptr) );
    //   iBoard = 0;
    //   data_size = 0;
    // }
    fCircularBuffer.Insert( this_data_size_bytes/sizeof(uint16_t), reinterpret_cast<uint16_t const*>(data_ptr) );
    data_size_bytes += this_data_size_bytes;
    TRACEN("PhysCrateData",TLVL_DEBUG,"GetData : Data copied! Size was %lu bytes, with %lu bytes now acquired.",
         this_data_size_bytes, data_size_bytes);
  }
  
  TRACEN("PhysCrateData",TLVL_DEBUG,"GetData completed. Status %d, Data size %lu bytes",0,data_size_bytes);

  if(data_size_bytes==0 && veto_state)
    VetoOff();

  return 0;
}

void icarus::PhysCrateData::FillStatPack( statpack & pack )
{
  memcpy(&(pack.token), "STAT", 4);
  //pack->crateid = htonl(158);
  pack.crateid = htonl(physCr->getId());
  pack.memstat1 = 0;
  pack.memstat2 = 0;
  pack.size = htonl(28);

  TRACEN("PhysCrateData",TLVL_DEBUG,"statpack initilized...");

  //return pack;
}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(icarus::PhysCrateData) 
