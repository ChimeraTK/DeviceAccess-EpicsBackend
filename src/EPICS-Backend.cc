// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * EPICS-Backend.cc
 *
 *  Created on: Jan 22, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "EPICS-Backend.h"

#include "EPICSBackendRegisterAccessor.h"
#include "EPICSChannelManager.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/DeviceAccessVersion.h>

#include <boost/tokenizer.hpp>

#include <cadef.h>

#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
typedef boost::tokenizer<boost::char_separator<char>> tokenizer;

extern "C" {
boost::shared_ptr<ChimeraTK::DeviceBackend> ChimeraTK_DeviceAccess_createBackend(
    std::string address, std::map<std::string, std::string> parameters) {
  return ChimeraTK::EpicsBackend::createInstance(address, parameters);
}

std::vector<std::string> ChimeraTK_DeviceAccess_sdmParameterNames{"map"};

std::string ChimeraTK_DeviceAccess_version{CHIMERATK_DEVICEACCESS_VERSION};

std::string backend_name = "epics";
}

namespace ChimeraTK {
  EpicsBackend::BackendRegisterer EpicsBackend::backendRegisterer;

  EpicsBackend::EpicsBackend(const std::string& mapfile) : _catalogue_filled(false), _freshCreated(true) {
    FILL_VIRTUAL_FUNCTION_TEMPLATE_VTABLE(getRegisterAccessor_impl);
    prepareChannelAccess();

    fillCatalogueFromMapFile(mapfile);
    _catalogue_filled = true;
  }

  EpicsBackend::~EpicsBackend() {
    _asyncReadActivated = false;
    close();
    ChannelManager::getInstance().cleanup();
  }

  void EpicsBackend::prepareChannelAccess() {
    // introduced to run the unit tests
    // \ToDo: Why is that needed - each test finishes with closing the device which should destroy the context already
    if(ca_current_context()) {
      ca_context_destroy();
    }
    auto result = ca_context_create(ca_enable_preemptive_callback);
    if(result != ECA_NORMAL) {
      std::stringstream ss;
      ss << "CA error " << ca_message(result) << "occurred while trying to start channel access.";
      throw ChimeraTK::runtime_error(ss.str());
    }
  }

  void EpicsBackend::open() {
    if(!isFunctional()) {
      // after closing a new ca_context is needed (_opened is set also in constructor to signal prepareChannelAccess was
      // just called before)
      if(!_freshCreated) {
        prepareChannelAccess();
        {
          std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
          ChannelManager::getInstance().addChannelsFromMap(this);
        }
      }
      else {
        _freshCreated = false;
      }
      size_t n = default_ca_timeout / 0.1; // sleep 100ms per loop, wait default_ca_timeout until giving up
      for(size_t i = 0; i < n; i++) {
        {
          std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
          if(ChannelManager::getInstance().checkAllConnections(true)) {
            _channelAccessUp = true;
            break;
          }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      if(!_channelAccessUp) {
        throw ChimeraTK::runtime_error("Failed to establish channel access connection.");
      }
      if(_asyncReadActivated) {
        std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
        ChannelManager::getInstance().activateChannels();
      }
      _startVersion = {};
      setOpenedAndClearException();
    }
  }

  void EpicsBackend::close() {
    _opened = false;
    _asyncReadActivated = false;
    std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
    ChannelManager::getInstance().deactivateChannels();
    ChannelManager::getInstance().resetConnectionState();
    ca_context_destroy();
  }

  void EpicsBackend::activateAsyncRead() noexcept {
    if(!isFunctional()) return;
    // activate async read expects an initial value so deactivate channels first to force initial value
    {
      std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
      ChannelManager::getInstance().deactivateChannels();
      ChannelManager::getInstance().activateChannels();
    }
    size_t n = default_ca_timeout / 0.1; // sleep 100ms per loop, wait default_ca_timeout until giving up
    bool allGood = false;
    for(size_t i = 0; i < n; i++) {
      if(ChannelManager::getInstance().checkInitialValueReceived()) {
        allGood = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if(!allGood) {
      std::cerr << "Failed to receive initial value for all subscriptions in activateAsyncRead()." << std::endl;
    }
    _asyncReadActivated = true;
  }

  template<typename UserType>
  boost::shared_ptr<NDRegisterAccessor<UserType>> EpicsBackend::getRegisterAccessor_impl(
      const RegisterPath& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags) {
    RegisterPath path = "EPICS://" + registerPathName;

    EpicsBackendRegisterInfo info = _catalogue_mutable.getBackendRegister(registerPathName);

    if(numberOfWords + wordOffsetInRegister > info._nElements || (numberOfWords == 0 && wordOffsetInRegister > 0)) {
      std::stringstream ss;
      ss << "Requested number of words/elements ( " << numberOfWords << ") with offset " << wordOffsetInRegister
         << " exceeds the number of available words/elements: " << info._nElements;
      throw ChimeraTK::logic_error(ss.str());
    }

    if(numberOfWords == 0) numberOfWords = info._nElements;

    unsigned base_type = info._dbfType % (LAST_TYPE + 1);
    if(info._dbfType == DBR_STSACK_STRING || info._dbfType == DBR_CLASS_NAME) base_type = DBR_STRING;
    //    switch(info._dpfType){
    switch(base_type) {
      case DBR_STRING:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_string_t, dbr_time_string, UserType>>(
            path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister, _asyncReadActivated);
        break;
      case DBR_FLOAT:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_float_t, dbr_time_float, UserType>>(
            path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister, _asyncReadActivated);
        break;
      case DBR_DOUBLE:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_double_t, dbr_time_double, UserType>>(
            path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister, _asyncReadActivated);
        break;
      case DBR_CHAR:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_char_t, dbr_time_char, UserType>>(
            path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister, _asyncReadActivated);
        break;
      case DBR_SHORT:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_short_t, dbr_time_short, UserType>>(
            path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister, _asyncReadActivated);
        break;
      case DBR_LONG:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_long_t, dbr_time_long, UserType>>(
            path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister, _asyncReadActivated);
        break;
      case DBR_ENUM:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_enum_t, dbr_time_enum, UserType>>(
            path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister, _asyncReadActivated);
        break;
        //        if(dbr_type_is_CTRL(info._dpfType))
        //          return boost::make_shared<EpicsBackendRegisterAccessor<dbr_gr_enum, UserType>>(path,
        //          shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
        //        else if (dbr_type_is_GR(info._dpfType))
        //          return boost::make_shared<EpicsBackendRegisterAccessor<dbr_ctrl_enum, UserType>>(path,
        //          shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
        //        else
        //          return boost::make_shared<EpicsBackendRegisterAccessor<int, UserType>>(path, shared_from_this(),
        //          info, flags, numberOfWords, wordOffsetInRegister);
        //        break;
      default:
        throw ChimeraTK::runtime_error(std::string("Type ") + std::to_string(info._dbfType) + " not implemented.");
        break;
    }
  }

  EpicsBackend::BackendRegisterer::BackendRegisterer() {
    BackendFactory::getInstance().registerBackendType("epics", &EpicsBackend::createInstance, {"map"});
    std::cout << "BackendRegisterer: registered backend type epics" << std::endl;
  }

  boost::shared_ptr<DeviceBackend> EpicsBackend::createInstance(
      std::string /*address*/, std::map<std::string, std::string> parameters) {
    if(parameters["map"].empty()) {
      throw ChimeraTK::logic_error("No map file provided.");
    }
    return boost::shared_ptr<DeviceBackend>(new EpicsBackend(parameters["map"]));
  }

  void EpicsBackend::addCatalogueEntry(RegisterPath path, std::shared_ptr<std::string> pvName) {
    EpicsBackendRegisterInfo info(path);
    info._caName = std::string(*pvName.get());
    try {
      std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
      ChannelManager::getInstance().addChannel(info._caName, this);
    }
    catch(ChimeraTK::runtime_error& e) {
      std::cerr << e.what() << ". PV is not added to the catalog." << std::endl;
      return;
    }
    _catalogue_mutable.addRegister(info);
  }

  void EpicsBackend::configureChannel(EpicsBackendRegisterInfo& info) {
    std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
    auto pv = ChannelManager::getInstance().getPV(info._caName);
    if(!ChannelManager::getInstance().isChannelConfigured(info._caName)) {
      throw ChimeraTK::runtime_error("Trying to read an unconfigured channel.");
    }
    info._nElements = pv->nElems;
    info._dbfType = pv->dbfType;

    if(ca_read_access(pv->chid) != 1) info._isReadable = false;
    if(ca_write_access(pv->chid) != 1) info._isWritable = false;
    if(pv->dbfType == DBF_STRING) {
      info._dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::string, true, true, 320, 300);
    }
    else if(pv->dbfType == DBF_DOUBLE || pv->dbfType == DBF_FLOAT) {
      info._dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, false, true, 320, 300);
    }
    else if(pv->dbfType == DBF_INT || pv->dbfType == DBF_LONG || pv->dbfType == DBF_SHORT) {
      info._dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, true, 320, 300);
    }
    else if(pv->dbfType == DBF_ENUM) {
      info._dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::boolean, true, true, 320, 300);
    }
    else {
      std::cerr << "Failed to data descriptor for node: " << info._caName << "." << std::endl;
    }
    info._accessModes.add(AccessMode::wait_for_new_data);
  }

  void EpicsBackend::fillCatalogueFromMapFile(const std::string& mapfileName) {
    boost::char_separator<char> sep{"\t ", "", boost::drop_empty_tokens};
    std::string line;
    std::ifstream mapfile(mapfileName);
    if(mapfile.is_open()) {
      while(std::getline(mapfile, line)) {
        if(line.empty()) continue;
        tokenizer tok{line, sep};
        size_t nTokens = std::distance(tok.begin(), tok.end());
        if(!(nTokens == 2)) {
          std::cerr << "Wrong number of tokens (" << nTokens << ") in mapfile " << mapfileName
                    << " line (-> line is ignored): \n " << line << std::endl;
          continue;
        }
        auto it = tok.begin();

        try {
          std::shared_ptr<std::string> pathStr = std::make_shared<std::string>(*it);
          RegisterPath path(*(pathStr.get()));
          it++;
          std::shared_ptr<std::string> nodeName = std::make_shared<std::string>(*it);
          addCatalogueEntry(path, nodeName);
        }
        catch(std::out_of_range& e) {
          std::cerr << "Failed reading the following line from mapping file " << mapfileName << "\n " << line
                    << std::endl;
        }
      }
      mapfile.close();
    }
    else {
      throw ChimeraTK::runtime_error(std::string("Failed reading mapfile: ") + mapfileName);
    }

    if(_catalogue_mutable.getNumberOfRegisters() == 0) {
      throw ChimeraTK::runtime_error("No registers found in catalogue!");
    }

    auto result = ca_flush_io();
    if(result == ECA_TIMEOUT) {
      throw ChimeraTK::runtime_error("Channel setup failed.");
    }
    size_t n = default_ca_timeout / 0.1; // sleep 100ms per loop, wait default_ca_timeout until giving up
    for(size_t i = 0; i < n; i++) {
      {
        std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
        if(ChannelManager::getInstance().checkAllConnections(true)) {
          _channelAccessUp = true;
          break;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if(!_channelAccessUp) {
      throw ChimeraTK::runtime_error("Failed to establish channel access connection.");
    }
    for(auto& reg : _catalogue_mutable) {
      configureChannel(reg);
    }
  }

  void EpicsBackend::setExceptionImpl() noexcept {
    _asyncReadActivated = false;
    ChannelManager::getInstance().setException(std::string("Exception reported by another accessor."));
  }
} // namespace ChimeraTK
