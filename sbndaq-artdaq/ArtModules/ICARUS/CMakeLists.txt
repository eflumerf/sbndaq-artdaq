cet_make(
 NO_PLUGINS
 BASENAME_ONLY
 LIBRARY_NAME
    sbndaq-artdaq_ArtModules_ICARUS
 LIB_LIBRARIES
    sbndaq_artdaq_core::sbndaq-artdaq-core_Overlays
    sbndaq_artdaq_core::sbndaq-artdaq-core_Overlays_Common
    sbndaq_artdaq_core::sbndaq-artdaq-core_Overlays_ICARUS
 EXCLUDE
    Icarus_fragmentNameHelper.cc
    IcarusFragmentWatcher_module.cc
)
#      artdaq_DAQdata
#      artdaq_Generators
#      artdaq-utilities_Plugins
#      artdaq_core::artdaq-core_Utilities
list(APPEND BUILD_PLUGIN_CORE_LIB_LIST sbndaq_artdaq_core::sbndaq-artdaq-core_Overlays_ICARUS)
list(APPEND BUILD_PLUGIN_CORE_LIB_LIST artdaq_DAQdata)
list(APPEND BUILD_PLUGIN_FRAGNAMEHELPER_LIB_LIST  sbndaq_artdaq_core::sbndaq-artdaq-core_Overlays_ICARUS)

build_plugin(IcarusArtdaqBuildInfo "module" LIBRARIES REG ${BUILD_PLUGIN_BUILDINFO_LIB_LIST})
build_plugin(PhysCrateDump "module" LIBRARIES REG ${BUILD_PLUGIN_CORE_LIB_LIST})
build_plugin(ICARUSTriggerUDPDump "module" LIBRARIES REG ${BUILD_PLUGIN_CORE_LIB_LIST})
build_plugin(ICARUSGateFilter  "module" LIBRARIES REG ${BUILD_PLUGIN_CORE_LIB_LIST})
build_plugin(PhysCrateWriteICARUSDataFile "module" LIBRARIES REG ${BUILD_PLUGIN_CORE_LIB_LIST})
build_plugin(PhysCrateViewer "module" LIBRARIES REG ${BUILD_PLUGIN_CORE_LIB_LIST})
build_plugin(PhysCrateSimpleAna "module" LIBRARIES REG ${BUILD_PLUGIN_CORE_LIB_LIST})
build_plugin(ICARUSTriggerUDPSimpleAna "module" LIBRARIES REG ${BUILD_PLUGIN_CORE_LIB_LIST})
basic_plugin(Icarus "fragmentNameHelper" LIBRARIES REG ${BUILD_PLUGIN_FRAGNAMEHELPER_LIB_LIST})

simple_plugin(IcarusFragmentWatcher "module"
   artdaq_DAQdata
   artdaq-utilities_Plugins
   art_Framework_Services_Registry
   art_Utilities
   MF_MessageLogger
   fhiclcpp
   cetlib
)

