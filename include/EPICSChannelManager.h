/*
 * EPICSSubscriptionManager.h
 *
 *  Created on: Mar 23, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#pragma once
#include <vector>
#include <map>
#include <string>
#include <mutex>

#include "EPICS_types.h"


namespace ChimeraTK{
class EpicsBackendRegisterAccessorBase;

class ChannelManager{
public:
  static ChannelManager& getInstance(){
    static ChannelManager manager;
    return manager;
  }

  static void channelStateHandler(connection_handler_args args);

  struct ChannelInfo{
    std::vector<EpicsBackendRegisterAccessorBase*> _accessors;
    std::string _caName;

    ChannelInfo(std::string channelName):_caName(channelName){};

    bool isChannelName(std::string channelName);

    bool operator==(const ChannelInfo& other);

  };

  std::mutex mapLock;

  std::map<chid,ChannelInfo> channelMap;
};
}
