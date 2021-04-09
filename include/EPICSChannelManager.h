/*
 * EPICSSubscriptionManager.h
 *
 *  Created on: Mar 23, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#pragma once
#include <deque>
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
    std::deque<EpicsBackendRegisterAccessorBase*> _accessors;
    std::string _caName;

    ChannelInfo(std::string channelName):_caName(channelName){};

    bool isChannelName(std::string channelName);

    bool operator==(const ChannelInfo& other);

  };

  void addChannel(const chid &chidIn, const std::string name);

  bool channelPresent(const std::string name);

  void addAccessor(const chid &chidIn, EpicsBackendRegisterAccessorBase* accessor);

  void removeAccessor(const chid &chidIn, EpicsBackendRegisterAccessorBase* accessor);

  void setException(const std::string error);
private:
  std::mutex mapLock;

  std::map<chid,ChannelInfo> channelMap;
};
}
