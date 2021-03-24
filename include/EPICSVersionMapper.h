/*
 * VersionMapper.h
 *
 *  Created on: Mar 23, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#pragma once

#include <map>
#include <mutex>
#include <chrono>
#include <ChimeraTK/VersionNumber.h>
#include "epicsTime.h"

using timePoint_t = std::chrono::time_point<std::chrono::system_clock,std::chrono::duration<int64_t,std::nano> > ;

namespace EPICS{
/**
 * This class is needed, because if two accessors are create and receive the same data from the server their
 * VersionNumber is required to be identical. Since the VersionNumber is not identical just because it is created
 * with the same timestamp this VersionMapper is needed. It takes care of holding a global map that connects source
 * time stamps attached  to OPC UA data to ChimeraTK::VersionNumber.
 *
 * It assumes that OPC UA source time stamps are unique.
 * \ToDo: Does every accessor needs it own VersionNumber history??
 */
class VersionMapper {
 public:
  static VersionMapper& getInstance() {
    static VersionMapper instance;
    return instance;
  }

  ChimeraTK::VersionNumber getVersion(const epicsTimeStamp& timeStamp);

 private:
  VersionMapper() = default;
  ~VersionMapper() = default;
  VersionMapper(const VersionMapper&) = delete;
  VersionMapper& operator=(const VersionMapper&) = delete;

  std::mutex _mapMutex;
  std::map<int64_t, ChimeraTK::VersionNumber> _versionMap{};

  const int64_t _epicsTimeOffset{631152000}; ///< Offest to be applied since epics counts seconds since 1990

  constexpr static size_t maxSizeEventIdMap = 2000;
};
}
