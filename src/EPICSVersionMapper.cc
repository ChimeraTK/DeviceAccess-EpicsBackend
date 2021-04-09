/*
 * VersionMapper.cc
 *
 *  Created on: Mar 23, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "EPICSVersionMapper.h"

ChimeraTK::VersionNumber EPICS::VersionMapper::getVersion(const epicsTimeStamp &timeStamp){
  std::lock_guard<std::mutex> lock(_mapMutex);
  int64_t timeStampEPOCH = (_epicsTimeOffset + timeStamp.secPastEpoch)*1e9+timeStamp.nsec;
  if(!_versionMap.count(timeStampEPOCH)){
    if(_versionMap.size() == maxSizeEventIdMap)
      _versionMap.erase(_versionMap.begin());
    std::chrono::duration<int64_t,std::nano> tp(timeStampEPOCH);
    _versionMap[timeStampEPOCH] = ChimeraTK::VersionNumber(timePoint_t(tp));
  }
  return _versionMap[timeStampEPOCH];
}


