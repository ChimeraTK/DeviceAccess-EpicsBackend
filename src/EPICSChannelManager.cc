/*
 * EPICSSubscriptionManager.cc
 *
 *  Created on: Mar 23, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "EPICSChannelManager.h"
#include "EPICS-BackendRegisterAccessor.h"

namespace ChimeraTK{

  bool ChannelManager::ChannelInfo::isChannelName(std::string channelName){
    return _caName.compare(channelName) == 0;
  }

  bool ChannelManager::ChannelInfo::operator==(const ChannelInfo& other){
    return other._caName.compare(_caName);
  }

  void ChannelManager::channelStateHandler(connection_handler_args args){
    auto backend = reinterpret_cast<EpicsBackend*>(ca_puser(args.chid));
    if(args.op == CA_OP_CONN_UP){
      std::cout << "Channel access established." << std::endl;
      backend->setBackendState(true);
    } else if (args.op == CA_OP_CONN_DOWN){
      std::cout << "Channel access closed." << std::endl;
      backend->setBackendState(false);
      std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
      auto channelInfo = ChannelManager::getInstance().channelMap.at(args.chid);
      for( auto &ch : channelInfo._accessors){
        try{
          throw ChimeraTK::runtime_error(std::string("Channel for PV ") + ChannelManager::getInstance().channelMap.at(args.chid)._caName + " was disconnected.");
        } catch(...){
          ch->_notifications.push_overwrite_exception(std::current_exception());
        }
      }
    }
  }
}
