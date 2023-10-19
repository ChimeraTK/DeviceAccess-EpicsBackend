// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * EPICSSubscriptionManager.cc
 *
 *  Created on: Mar 23, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "EPICSChannelManager.h"

#include "EPICSBackendRegisterAccessor.h"

#include <cadef.h>

namespace ChimeraTK {

  ChannelInfo::ChannelInfo(std::string channelName) {
    _pv.reset((pv*)calloc(1, sizeof(pv)));
    _pv->name = (char*)channelName.c_str();
    auto result =
        ca_create_channel(_pv->name, ChannelManager::channelStateHandler, this, DEFAULT_CA_PRIORITY, &_pv->chid);
    if(result != ECA_NORMAL) {
      std::stringstream ss;
      ss << "CA error " << ca_message(result) << " occurred while trying to create channel " << _pv->name;
      throw ChimeraTK::runtime_error(ss.str());
    }
  };

  bool ChannelInfo::isChannelName(std::string channelName) {
    return std::string(_pv->name).compare(channelName) == 0;
  }

  bool ChannelInfo::operator==(const ChannelInfo& other) {
    return std::string(other._pv->name).compare(std::string(_pv->name));
  }

  ChannelManager::~ChannelManager() {
    std::lock_guard<std::mutex> lock(mapLock);
    channelMap.clear();
  }

  ChannelManager& ChannelManager::getInstance() {
    static ChannelManager manager;
    return manager;
  }

  void ChannelManager::channelStateHandler(connection_handler_args args) {
    auto backend = reinterpret_cast<EpicsBackend*>(ca_puser(args.chid));
    std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
    // channel should have been added to the manager - if not let throw here, because this should not happen
    auto it = ChannelManager::getInstance().findChid(args.chid);
    if(args.op == CA_OP_CONN_UP) {
      std::cout << "Channel access established." << std::endl;
      backend->setBackendState(true);
      // configure channel
      it->second._pv->nElems = ca_element_count(args.chid);
      it->second._pv->dbfType = ca_field_type(args.chid);
      it->second._pv->dbrType = dbf_type_to_DBR_TIME(it->second._pv->dbfType);
      it->second._configured = true;
    }
    else if(args.op == CA_OP_CONN_DOWN) {
      std::cout << "Channel access closed." << std::endl;
      backend->setBackendState(false);
      for(auto& ch : it->second._accessors) {
        try {
          throw ChimeraTK::runtime_error(std::string("Channel for PV ") + it->second._pv->name + " was disconnected.");
        }
        catch(...) {
          ch->_notifications.push_overwrite_exception(std::current_exception());
        }
      }
    }
  }

  void ChannelManager::handleEvent(evargs args) {
    auto base = reinterpret_cast<ChannelManager*>(args.usr);
    //      std::cout << "Handling update of ca " << base->_info._caName << std::endl;
    if(base->findChid(args.chid)->second._asyncReadActivated) {
      for(auto& accessor : base->findChid(args.chid)->second._accessors) {
        accessor->_notifications.push_overwrite(args);
      }
    }
  }

  std::map<std::string, ChannelInfo>::iterator ChannelManager::findChid(const chanId& chid) {
    for(auto it = channelMap.begin(); it != channelMap.end(); it++) {
      if(chid == it->second._pv->chid) {
        return it;
      }
    }
    throw ChimeraTK::runtime_error("Failed to find channel ID in the channel manager map.");
  }

  std::shared_ptr<pv> ChannelManager::getPV(const std::string& name) {
    std::lock_guard<std::mutex> lock(mapLock);
    if(!channelPresent(name)) {
      throw ChimeraTK::runtime_error("Tryed to get pv without having a map entry!");
    }
    return channelMap.find(name)->second._pv;
  }

  void ChannelManager::addChannel(const std::string name) {
    std::lock_guard<std::mutex> lock(mapLock);
    if(!channelPresent(name)) {
      channelMap.insert(std::make_pair(name, ChannelInfo(name)));
    }
  }

  bool ChannelManager::channelPresent(const std::string name) {
    std::lock_guard<std::mutex> lock(mapLock);
    if(channelMap.count(name)) {
      return true;
    }
    else {
      return false;
    }
  }

  bool ChannelManager::isChannelConfigured(const std::string& name) {
    if(channelPresent(name)) {
      return channelMap.find(name)->second._configured;
    }
    else {
      return false;
    }
  }

  void ChannelManager::addAccessor(const std::string& name, EpicsBackendRegisterAccessorBase* accessor) {
    // map locked by calling function
    if(!channelPresent(name)) {
      throw ChimeraTK::runtime_error("Tryed to add an accessor without having a map entry!");
    }
    channelMap.find(name)->second._accessors.push_back(accessor);
  }

  void ChannelManager::removeAccessor(const std::string& name, EpicsBackendRegisterAccessorBase* accessor) {
    std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
    // check if channel is in map -> map might be already cleared.
    if(channelMap.count(name)) {
      auto entry = &channelMap.at(name);
      bool erased = false;
      for(auto itaccessor = entry->_accessors.begin(); itaccessor != entry->_accessors.end(); ++itaccessor) {
        if(accessor == *itaccessor) {
          entry->_accessors.erase(itaccessor);
          erased = true;
          break;
        }
      }
      if(!erased) {
        std::cout << "Failed to erase accessor for pv:" << name << std::endl;
      }
    }
  }

  void ChannelManager::activateChannel(const std::string& name) {
    auto pv = getPV(name);
    // take lock here, because getPV also uses the lock
    std::lock_guard<std::mutex> lock(mapLock);
    channelMap.find(name)->second._subscriptionId = new evid();
    ca_create_subscription(pv->dbrType, pv->nElems, pv->chid, DBE_VALUE, &ChannelManager::handleEvent,
        static_cast<ChannelManager*>(this), channelMap.find(name)->second._subscriptionId);
    ca_flush_io();
    channelMap.find(name)->second._asyncReadActivated = true;
    setInitialValue(name);
  }

  void ChannelManager::setInitialValue(const std::string& name) {
    //\ToDo: Here we read the same value multiple times (for each accessor) -> Read only once
    for(auto& accessor : channelMap.find(name)->second._accessors) {
      accessor->updateData();
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
} // namespace ChimeraTK
