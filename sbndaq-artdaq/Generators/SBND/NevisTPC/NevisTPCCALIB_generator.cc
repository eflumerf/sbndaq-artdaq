#define TRACE_NAME "NevisTPCGeneratorCALIB"

#include "artdaq/DAQdata/Globals.hh"
#include "artdaq/Generators/GeneratorMacros.hh"
#include "sbndaq-artdaq/Generators/SBND/NevisTPC/NevisTPCCALIB.hh"

#include <chrono>
#include <ctime>

void sbndaq::NevisTPCCALIB::ConfigureStart() {
  TLOG(TLVL_INFO)<< "ConfigureStart";

  fChunkSize = ps_.get<int>("ChunkSize", 1000000);
  fMonitorPeriod = ps_.get<int>("MonitorPeriod", 12);
  fCALIBFreq = ps_.get<double>("CALIBTriggerFrequency", 1.);
  fDumpBinary = ps_.get<bool>("DumpBinary", true);
  fDumpBinaryDir = ps_.get<std::string>("DumpBinaryDir", ".");
  fSubrunCheckPeriod_us = ps_.get<int>("SubrunCheckPeriod", -1);

  // Create Crate object
  fCrate = std::make_shared<nevistpc::Crate>( fControllerModule, fNUXMITReader, ps_ );
  
  // Run configuration recipe
  fCrate->runCalib( ps_ );
  // To do: nevistpc::Crate should have a general runConfiguration function
  // The specific Crate configuration function to run should be specified in a fcl file
  // Therefore, only one common generator would be need for all configurations that run the same GetFEMCrateData() function

  // Set up worker MonitorCrate thread.
  share::ThreadFunctor MonitorCrate_functor = std::bind( &NevisTPCCALIB::MonitorCrate, this );
  auto MonitorCrate_worker_functor = share::WorkerThreadFunctorUPtr( new share::WorkerThreadFunctor( MonitorCrate_functor, "MonitorCrateWorkerThread" ) );
  auto MonitorCrate_worker = share::WorkerThread::createWorkerThread( MonitorCrate_worker_functor );
  MonitorCrate_thread_.swap( MonitorCrate_worker );
  MonitorCrate_thread_->start();

  // Set up worker FireCALIB thread.
  share::ThreadFunctor FireCALIB_functor = std::bind( &NevisTPCCALIB::FireCALIB, this );
  auto FireCALIB_worker_functor = share::WorkerThreadFunctorUPtr( new share::WorkerThreadFunctor( FireCALIB_functor, "FireCALIBWorkerThread" ) );
  auto FireCALIB_worker = share::WorkerThread::createWorkerThread( FireCALIB_worker_functor );
  FireCALIB_thread_.swap(FireCALIB_worker);
  FireCALIB_thread_->start();

  // Get timestamp for binary file name
  time_t t = time(0);
  struct tm ltm = *localtime( &t );
  sprintf(binFileName, "%s/sbndrawbin_run%06i_%4i.%02i.%02i-%02i.%02i.%02i_NevisTPCCALIB.dat",
	  fDumpBinaryDir.c_str(), sbndaq::NevisTPC_generatorBase::run_number(), 
	  ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec);

  if( fDumpBinary ){
    TLOG(TLVL_INFO)<< "Opening raw binary file " << binFileName;
    binFile.open(binFileName, std::ofstream::out | std::ofstream::binary ); // temp
  }

  TLOG(TLVL_INFO) << "Successful " << __func__ ;
}

void sbndaq::NevisTPCCALIB::ConfigureStop() {
  FireCALIB_thread_->stop();
  MonitorCrate_thread_->stop();

  if( fDumpBinary ){
    TLOG(TLVL_INFO)<< "Closig raw binary file " << binFileName;
    binFile.close(); // temp
  }

  TLOG(TLVL_INFO)<< "Successful " << __func__ ;
}

bool sbndaq::NevisTPCCALIB::FireCALIB() {
  static int fCALIBPeriod_us = 1./fCALIBFreq * 1e6;
  static std::chrono::steady_clock::time_point next_trigger_time{std::chrono::steady_clock::now() + std::chrono::microseconds(fCALIBPeriod_us)};
  if( next_trigger_time > std::chrono::steady_clock::now() ) return false;

  fCrate->getTriggerModule()->sendOutCalibTrigger();
  mf::LogInfo("NevisTPCCALIB") << "CALIB Trigger ";
  TLOG(TLVL_INFO) << "Called " << __func__ ;
  next_trigger_time = std::chrono::steady_clock::now() + std::chrono::microseconds( fCALIBPeriod_us );
  usleep(10000);

  return true;
}

bool sbndaq::NevisTPCCALIB::MonitorCrate() {
  static std::chrono::steady_clock::time_point next_monitor_cycle_time{ std::chrono::steady_clock::now() };

  if( next_monitor_cycle_time > std::chrono::steady_clock::now() ) return false;
  
  fCrate->getXMITModule()->readStatus();

  TLOG(TSTATUS) << "Called " << __func__ ;
  // To do: add other board status checks. Follow uboonedaq/projects/sebs/configManager_CrateMonitor.cpp
  next_monitor_cycle_time = std::chrono::steady_clock::now() + std::chrono::seconds( fMonitorPeriod );

  return true;
}

size_t sbndaq::NevisTPCCALIB::GetFEMCrateData() {
  
  TLOG(TGETDATA)<< "GetFEMCrateData";

  // Just for tests
  // Taken from NevisTPCFile_generator and adapted to use an XMITReader
  // To be reviewed
  uint16_t* buffer = new uint16_t[fChunkSize];

  std::streamsize bytesRead = fNUXMITReader->readsome(reinterpret_cast<char*>(buffer), fChunkSize);

  if( bytesRead > 0 ){

    unsigned wordsRead = bytesRead * sizeof(char) / sizeof(uint16_t);

    std::copy(buffer, buffer + wordsRead, &DMABuffer_[0]);

    if( fDumpBinary ) binFile.write( (char*)buffer, fChunkSize );

  }

  delete[] buffer;

  return bytesRead;
}

// Reimplement function to be able to cerate subruns using hardware inputs
bool sbndaq::NevisTPCCALIB::FillFragment(artdaq::FragmentPtrs &frags, bool clear_buffer[[gnu::unused]]){
  static std::chrono::steady_clock::time_point next_check_time{std::chrono::steady_clock::now() + std::chrono::microseconds(fSubrunCheckPeriod_us)};
  static nevistpc::TriggerModuleGPSStamp lastGPSStamp = fCrate->getTriggerModule()->getLastGPSClockRegister();

  // Call the base function first
  bool status = sbndaq::NevisTPC_generatorBase::FillFragment(frags, clear_buffer);

  // Make artdaq create a new subrun if a new $30 was received

  if( fSubrunCheckPeriod_us < 0 || next_check_time > std::chrono::steady_clock::now() ) return status;

  nevistpc::TriggerModuleGPSStamp nowGPSStamp = fCrate->getTriggerModule()->getLastGPSClockRegister();
  // Check if a new $30 arrived to the GPS input of the Nevis Trigger Board
  if( (nowGPSStamp.gps_frame != lastGPSStamp.gps_frame) || 
      (nowGPSStamp.gps_sample != lastGPSStamp.gps_sample) || 
      (nowGPSStamp.gps_sample_div != lastGPSStamp.gps_sample_div) ){

    mf::LogInfo("NevisTPCCALIB") << "\n\n***** New subrun *****\n\n";
    TLOG(TLVL_INFO) << "New $30 signal will create a new subrun";

    TLOG(TLVL_INFO) << "Last GPS timestamp frame " << lastGPSStamp.gps_frame 
		    << " sample " << lastGPSStamp.gps_sample 
		    << " div " << lastGPSStamp.gps_sample_div ;
    TLOG(TLVL_INFO) << "Now GPS timestamp frame " << nowGPSStamp.gps_frame 
		    << " sample " << nowGPSStamp.gps_sample 
		    << " div " << nowGPSStamp.gps_sample_div ;

    lastGPSStamp = nowGPSStamp;

    // artdaq snippet to create a new subrun
    artdaq::FragmentPtr endOfSubrunFrag(new artdaq::Fragment(static_cast<size_t>(ceil(sizeof(my_rank) / static_cast<double>(sizeof(artdaq::Fragment::value_type))))));
    endOfSubrunFrag->setSystemType(artdaq::Fragment::EndOfSubrunFragmentType);
    // Use the event number from the base class
    endOfSubrunFrag->setSequenceID(sbndaq::NevisTPC_generatorBase::_this_event);
    *endOfSubrunFrag->dataBegin() = my_rank;
    frags.emplace_back(std::move(endOfSubrunFrag));
    // end of artdaq snippet to create a new subrun
  }
  next_check_time = std::chrono::steady_clock::now() + std::chrono::microseconds( fSubrunCheckPeriod_us );

  return status;
}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(sbndaq::NevisTPCCALIB)

