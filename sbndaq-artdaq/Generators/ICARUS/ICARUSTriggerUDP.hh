#ifndef sbndaq_artdaq_Generators_ICARUSTriggerUDP_hh
#define sbndaq_artdaq_Generators_ICARUSTriggerUDP_hh

// The UDP Receiver class recieves UDP data from an OtSDAQ applicance and
// puts that data into UDPFragments for further ARTDAQ analysis.
//
// It currently assumes two things to be true:
// 1. The first word of the UDP packet is an 8-bit flag with information
// about the status of the sender
// 2. The second word is an 8-bit sequence ID, used for detecting
// dropped UDP datagrams
// Some C++ conventions used:
// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh"
#include "artdaq/Generators/CommandableFragmentGenerator.hh"
#include "sbndaq-artdaq-core/Overlays/ICARUS/ICARUSTriggerUDPFragment.hh"
#include "sbndaq-artdaq-core/Overlays/ICARUS/ICARUSPMTGateFragment.hh"
#include "sbndaq-artdaq-core/Overlays/FragmentType.hh"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <array>
#include <list>
#include <queue>
#include <atomic>
#include <vector>

namespace sbndaq
{

  //command definitions
  typedef char Command_t[16];
  const Command_t TTLK_INIT="TTLK_CMD_INIT";
  const Command_t TRIG_VETO="TRIG_CMD_VETO";
  const Command_t TRIG_ALLW="TRIG_CMD_ALLW";

  class ICARUSTriggerUDP : public artdaq::CommandableFragmentGenerator
  {
  public:
    
    explicit ICARUSTriggerUDP(fhicl::ParameterSet const& ps);
    
  private:
    
    bool getNext_(artdaq::FragmentPtrs& output) override;
    void start() override;
    void stop() override;
    void stopNoMutex() override {} // nothing special needs to be done in this method
    void pause() override;
    void resume() override;
    
    void send(const Command_t);
    int poll_with_timeout(int,std::string,struct sockaddr_in&, int);
    //int read(int,uint16_t*);
    int read(int, std::string, struct sockaddr_in&,int,char*);
    
    int send_TTLK_INIT(int,int);
    void send_TRIG_VETO();
    void send_TRIG_ALLW();
    
    // FHiCL-configurable variables. Note that the C++ variable names
    // are the FHiCL variable names with a "_" appended

    uint32_t fragment_id_;
    uint32_t fragment_id_pmt_;
    size_t max_fragment_size_bytes_;
    size_t max_fragment_size_bytes_pmt_;

    int configport_;
    std::string ip_config_;

    int dataport_;
    std::string ip_data_;

    //Socket parameters
    struct sockaddr_in si_config_;
    int configsocket_;

    struct sockaddr_in si_data_;
    int datasocket_;

    int pmtdataport_;
    std::string ip_data_pmt_;

    struct sockaddr_in si_pmtdata_;
    int pmtsocket_;

    //retry for init msg
    int n_init_retries_;
    int n_init_timeout_ms_;
    uint64_t fEventCounter;
    uint64_t fLastEvent;
    uint64_t fNTP_time;
    uint64_t fLastTimestamp;
    uint64_t fLastTimestampBNB;
    uint64_t fLastTimestampNuMI;
    uint64_t fLastTimestampOther;
    long fLastGatesNum;
    long fLastGatesNumBNB;
    long fLastGatesNumNuMI;
    long fLastGatesNumOther;
    long fDeltaGates;
    long fDeltaGatesBNB;
    long fDeltaGatesNuMI;
    long fDeltaGatesOther;
    uint64_t fStartOfRun;
    int fInitialStep;
    bool use_wr_time_;
    long wr_time_offset_ns_;
    //expected fragments
    int generated_fragments_per_event_;
    static constexpr std::size_t BufferSize = 1500;
    char buffer[BufferSize] = {'\0'};
    // char buffer[1000] = {'\0'};
    uint8_t peekBuffer[2] = {0,0};
  };
}
#endif /* sbndaq_artdaq_Generators_ICARUSTriggerUDP_hh */
