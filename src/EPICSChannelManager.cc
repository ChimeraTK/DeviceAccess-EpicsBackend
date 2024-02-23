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
    _pv.reset((pv*)calloc(1, sizeof(pv)), free);
    _caName = channelName;
    _pv->name = (char*)_caName.c_str();
  }

  bool ChannelInfo::isChannelName(std::string channelName) {
    return _caName.compare(channelName) == 0;
  }

  bool ChannelInfo::operator==(const ChannelInfo& other) {
    return other._caName.compare(_caName);
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
    if(args.op == CA_OP_CONN_UP) {
      backend->setBackendState(true);
      std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
      // channel should have been added to the manager - if not let throw here, because this should not happen
      auto it = ChannelManager::getInstance().findChid(args.chid);
      it->second._connected = true;
      // configure channel
      if(!it->second._configured) {
        it->second._pv->nElems = ca_element_count(args.chid);
        it->second._pv->dbfType = ca_field_type(args.chid);
        it->second._pv->dbrType = dbf_type_to_DBR_TIME(it->second._pv->dbfType);
        it->second._pv->value =
            calloc(it->second._pv->nElems, dbr_size_n(it->second._pv->dbrType, it->second._pv->nElems));
        it->second._configured = true;
      }
    }
    else if(args.op == CA_OP_CONN_DOWN) {
      backend->setBackendState(false);
      if(!backend->isOpen()) {
#ifdef CHIMERATK_UNITTEST
        ChannelManager::getInstance().currentState = args.op;
#endif
        return;
      }
      std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
      // channel should have been added to the manager - if not let throw here, because this should not happen
      auto it = ChannelManager::getInstance().findChid(args.chid);
      it->second._connected = false;
      if(it->second._asyncReadActivated) {
        ChannelManager::getInstance().deactivateChannels();
      }

      for(auto& accessor : it->second._accessors) {
        if(!accessor->_hasNotificationsQueue || !accessor->_backend->_asyncReadActivated) {
          continue;
        }
        try {
          throw ChimeraTK::runtime_error(std::string("Channel for PV ") + it->second._caName + " was disconnected.");
        }
        catch(...) {
          accessor->_notifications.push_overwrite_exception(std::current_exception());
        }
      }
    }
#ifdef CHIMERATK_UNITTEST
    // set state -> it is used in the test to wait for a connect/reconnect
    std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
    if(args.op == CA_OP_CONN_UP) {
      if(ChannelManager::getInstance().checkAllConnections(true)) {
        // only set connected once all are up
        ChannelManager::getInstance().currentState = args.op;
      }
    }
    else {
      if(ChannelManager::getInstance().checkAllConnections(false)) {
        // only set connected once all are down and exceptions have been pushed
        ChannelManager::getInstance().currentState = args.op;
      }
    }
#endif
  }

  void ChannelManager::handleEvent(evargs args) {
    auto base = reinterpret_cast<ChannelManager*>(args.usr);
    auto backend = reinterpret_cast<EpicsBackend*>(ca_puser(args.chid));
    std::lock_guard<std::mutex> lock(base->mapLock);
    auto it = base->findChid(args.chid);
    if(backend->isOpen() && backend->isFunctional()) {
      if(it->second._asyncReadActivated) {
        for(auto& accessor : it->second._accessors) {
          // channel can have accessors without mode wait_for_new_data -> no notification queue
          if(accessor->_hasNotificationsQueue) {
            EpicsRawData data(args);
            accessor->_notifications.push_overwrite(std::move(data));
            it->second._initialValueReceived = true;
          }
        }
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
    if(!channelPresent(name)) {
      throw ChimeraTK::runtime_error("Tried to get pv without having a map entry!");
    }
    return channelMap.find(name)->second._pv;
  }

  void ChannelManager::addChannel(const std::string name, EpicsBackend* backend) {
    if(!channelPresent(name)) {
      channelMap.insert(std::make_pair(name, ChannelInfo(name)));
    }
    auto pv = getPV(name);
    auto result =
        ca_create_channel(name.c_str(), ChannelManager::channelStateHandler, backend, default_ca_priority, &pv->chid);
    if(result != ECA_NORMAL) {
      std::stringstream ss;
      ss << "CA error " << ca_message(result) << " occurred while trying to create channel " << name;
      throw ChimeraTK::runtime_error(ss.str());
    }
  }

  void ChannelManager::addChannelsFromMap(EpicsBackend* backend) {
    for(auto& ch : channelMap) {
      auto result = ca_create_channel(ch.second._caName.c_str(), ChannelManager::channelStateHandler, backend,
          default_ca_priority, &ch.second._pv->chid);
      if(result != ECA_NORMAL) {
        std::stringstream ss;
        ss << "CA error " << ca_message(result) << " occurred while trying to create channel " << ch.second._caName;
        throw ChimeraTK::runtime_error(ss.str());
      }
    }
  }

  bool ChannelManager::checkAllConnections(const bool& connected) {
    if(connected) {
      // check if all are connected
      for(auto& ch : channelMap) {
        if(!ch.second._connected) return false;
      }
    }
    else {
      // check if all are disconnected
      for(auto& ch : channelMap) {
        if(ch.second._connected) return false;
      }
    }
    return true;
  }
  bool ChannelManager::isChannelConnected(const std::string name) {
    if(!channelPresent(name)) return false;
    return channelMap.find(name)->second._connected;
  }

  bool ChannelManager::channelPresent(const std::string name) {
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
    if(channelMap.find(name)->second._accessors.size() > 1 && channelMap.find(name)->second._asyncReadActivated) {
      if(accessor->_hasNotificationsQueue) {
        accessor->setInitialValue(channelMap.find(name)->second._pv->value, channelMap.find(name)->second._pv->dbrType,
            channelMap.find(name)->second._pv->nElems);
      }
    }
  }

  void ChannelManager::removeAccessor(const std::string& name, EpicsBackendRegisterAccessorBase* accessor) {
    std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
    // check if channel is in map -> map might be already cleared.
    if(channelMap.count(name)) {
      auto entry = &channelMap.at(name);
      if(entry->_accessors.size() > 0) {
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
      if(entry->_accessors.size() == 0) {
        if(!entry->_asyncReadActivated) return;
        ca_clear_subscription(*entry->_subscriptionId);
        entry->_asyncReadActivated = false;
        ca_flush_io();
      }
    }
  }

  void ChannelManager::activateChannel(const std::string& name) {
    auto pv = getPV(name);
    activateChannel(&channelMap.find(name)->second);
  }

  void ChannelManager::activateChannel(ChannelInfo* channel) {
    if(channel->_asyncReadActivated) return;
    // only open subscription if accessors are present -> else the initial value will be lost
    // The handler will be called directly after creating the subscription
    // E.g. in case of QtHardmon EpicsBackend::activateAsyncRead is called first and accessors are added later
    if(channel->_accessors.size() == 0) return;
    channel->_subscriptionId = new evid();
    auto ret = ca_create_subscription(channel->_pv->dbrType, channel->_pv->nElems, channel->_pv->chid, DBE_VALUE,
        &ChannelManager::handleEvent, static_cast<ChannelManager*>(this), channel->_subscriptionId);
    if(ret != ECA_NORMAL) {
      throw ChimeraTK::runtime_error(std::string("Failed to create subscription for channel: ") + channel->_pv->name);
    }
    ca_flush_io();
    channel->_asyncReadActivated = true;
    channel->_initialValueReceived = false;
    std::cout << "Channel " << channel->_caName << " activated for async read." << std::endl;
  }

  void ChannelManager::activateChannels() {
    for(auto& ch : channelMap) {
      activateChannel(&ch.second);
    }
  }

  bool ChannelManager::checkInitialValueReceived() {
    std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
    for(auto& ch : channelMap) {
      if(ch.second._asyncReadActivated && !ch.second._initialValueReceived) return false;
    }
    return true;
  }

  void ChannelManager::deactivateChannels() {
    for(auto& ch : channelMap) {
      if(!ch.second._asyncReadActivated) continue;
      ca_clear_subscription(*ch.second._subscriptionId);
      ch.second._asyncReadActivated = false;
    }
    ca_flush_io();
  }

  void ChannelManager::resetConnectionState() {
    for(auto& ch : channelMap) {
      ch.second._connected = false;
    }
  }

  void ChannelManager::setException(const std::string error) {
    std::lock_guard<std::mutex> lock(mapLock);
    ChannelManager::getInstance().deactivateChannels();
    for(auto& mapItem : channelMap) {
      // only push exceptions to channels that are still connected
      // if an exception is see on the first channel it is push to the notification queue and _connected is set false.
      if(mapItem.second._connected) {
        mapItem.second._connected = false;
        for(auto& accessor : mapItem.second._accessors) {
          try {
            throw ChimeraTK::runtime_error(error);
          }
          catch(...) {
            if(accessor->_hasNotificationsQueue) {
              accessor->_notifications.push_overwrite_exception(std::current_exception());
            }
          }
        }
      }
    }
  }
} // namespace ChimeraTK
