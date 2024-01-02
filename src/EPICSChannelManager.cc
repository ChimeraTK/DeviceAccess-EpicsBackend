// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * EPICSSubscriptionManager.cc
 *
 *  Created on: Mar 23, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "EPICSChannelManager.h"

#include "EPICS-BackendRegisterAccessor.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace ChimeraTK {

  ChannelManager::~ChannelManager() {
    std::lock_guard<std::mutex> lock(mapLock);
    channelMap.clear();
  }

  bool ChannelManager::ChannelInfo::isChannelName(std::string channelName) {
    return _caName.compare(channelName) == 0;
  }

  ChannelManager& ChannelManager::getInstance() {
    static ChannelManager manager;
    return manager;
  }

  bool ChannelManager::ChannelInfo::operator==(const ChannelInfo& other) {
    return other._caName.compare(_caName);
  }

  void ChannelManager::channelStateHandler(connection_handler_args args) {
    auto backend = reinterpret_cast<EpicsBackend*>(ca_puser(args.chid));
    if(args.op == CA_OP_CONN_UP) {
      std::cout << "Channel access established." << std::endl;
      ChannelManager::getInstance().channelMap.at(args.chid)._connected = true;
      // Note: backend opened/isFunctional state must not be set to open/function outside the open() function!
    }
    else if(args.op == CA_OP_CONN_DOWN) {
      std::cout << "Channel access closed." << std::endl;
      backend->setException(std::string("Channel for PV ") +
          ChannelManager::getInstance().channelMap.at(args.chid)._caName + " was disconnected.");
      std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
      // check if channel is in map -> map might be already cleared.
      if(ChannelManager::getInstance().channelMap.count(args.chid)) {
        auto channelInfo = &ChannelManager::getInstance().channelMap.at(args.chid);
        channelInfo->_connected = false;
        for(auto& ch : channelInfo->_accessors) {
          try {
            throw ChimeraTK::runtime_error(std::string("Channel for PV ") +
                ChannelManager::getInstance().channelMap.at(args.chid)._caName + " was disconnected.");
          }
          catch(...) {
            ch->_notifications.push_overwrite_exception(std::current_exception());
          }
        }
      }
    }
  }

  void ChannelManager::addChannel(const chid& chidIn, const std::string name) {
    std::lock_guard<std::mutex> lock(mapLock);
    channelMap.insert(std::make_pair(chidIn, ChannelInfo(name)));
  }

  bool ChannelManager::channelPresent(const std::string name) {
    std::lock_guard<std::mutex> lock(mapLock);
    for(auto item : channelMap) {
      if(item.second.isChannelName(name)) return true;
    }
    return false;
  }

  void ChannelManager::addAccessor(const chid& chidIn, EpicsBackendRegisterAccessorBase* accessor) {
    std::lock_guard<std::mutex> lock(mapLock);
    channelMap.at(chidIn)._accessors.push_back(accessor);
  }

  void ChannelManager::removeAccessor(const chid& chidIn, EpicsBackendRegisterAccessorBase* accessor) {
    std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
    // check if channel is in map -> map might be already cleared.
    if(channelMap.count(chidIn)) {
      auto entry = &channelMap.at(chidIn);
      bool erased = false;
      for(auto itaccessor = entry->_accessors.begin(); itaccessor != entry->_accessors.end(); ++itaccessor) {
        if(accessor == *itaccessor) {
          entry->_accessors.erase(itaccessor);
          erased = true;
          break;
        }
      }
      if(!erased) {
        std::cout << "Failed to erase accessor for pv:" << entry->_caName << std::endl;
      }
    }
  }

  void ChannelManager::setException(const std::string error) {
    std::lock_guard<std::mutex> lock(mapLock);
    for(auto& mapItem : channelMap) {
      for(auto& accessor : mapItem.second._accessors) {
        try {
          throw ChimeraTK::runtime_error(error);
        }
        catch(...) {
          accessor->_notifications.push_exception(std::current_exception());
        }
      }
    }
  }

  void ChannelManager::cleanup() {
    channelMap.clear();
  }

  bool ChannelManager::checkChannels() {
    // no lock needed as it is only called in waitForConnections which holds the mapLock
    for(auto& mapItem : channelMap) {
      if(!mapItem.second._connected) return false;
    }
    return true;
  }

  void ChannelManager::waitForConnections(double timeout) {
    std::lock_guard<std::mutex> lock(mapLock);
    size_t max = (size_t)(timeout / 0.02);
    for(size_t i = 0; i < max; i++) {
      if(checkChannels()) return;
      std::this_thread::sleep_for(20ms);
    }

    std::stringstream ss;
    ss << "Failed to build channel access conection to the following channels:";
    for(auto& mapItem : channelMap) {
      if(!mapItem.second._connected) ss << "\n\t - " << mapItem.second._caName;
    }
    throw ChimeraTK::runtime_error(ss.str());
  }
} // namespace ChimeraTK
