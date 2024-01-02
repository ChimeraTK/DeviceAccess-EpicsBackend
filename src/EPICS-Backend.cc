// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * EPICS-Backend.cc
 *
 *  Created on: Jan 22, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "EPICS-Backend.h"

#include "EPICS-BackendRegisterAccessor.h"
#include "EPICSChannelManager.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/DeviceAccessVersion.h>

#include <boost/tokenizer.hpp>

#include <fstream>
#include <iostream>
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

  EpicsBackend::EpicsBackend(const std::string& mapfile) : _catalogue_filled(false), _mapfile(mapfile) {
    FILL_VIRTUAL_FUNCTION_TEMPLATE_VTABLE(getRegisterAccessor_impl);
    auto result = ca_context_create(ca_enable_preemptive_callback);
    if(result != ECA_NORMAL) {
      std::stringstream ss;
      ss << "CA error " << ca_message(result) << "occurred while trying to start channel access.";
      throw ChimeraTK::runtime_error(ss.str());
    }

    fillCatalogueFromMapFile(_mapfile);
    for(auto& reg : _catalogue_mutable) {
      configureChannel(reg);
    }
    _catalogue_filled = true;
  }

  EpicsBackend::~EpicsBackend() {
    _asyncReadActivated = false;
    if(_opened) close();
    for(auto& reg : _catalogue_mutable) {
      ca_clear_channel(reg._pv->chid);
    }
    ChannelManager::getInstance().cleanup();
    if(isFunctional()) ca_context_destroy(); // FIXME: after close() isFunctional() cannot be true!
  }

  void EpicsBackend::open() {
    if(!_catalogue_filled) {
      fillCatalogueFromMapFile(_mapfile);
      _catalogue_filled = true;
    }
    if(!isFunctional()) {
      for(auto& reg : _catalogue_mutable) {
        openChannel(reg);
      }
      ChannelManager::getInstance().waitForConnections(_caTimeout);
    }
    setOpenedAndClearException();
  }

  void EpicsBackend::close() {
    /* Do not try to close the channel of CA
     * Each Accessor sets up a channel access subscription based on the channel ID
     * If it is closed here each accessor would need to set up again the subscription,
     * which is done currently in the constructor of the Accessor.
     */
    _opened = false;
  }

  void EpicsBackend::activateAsyncRead() noexcept {
    if(!isFunctional()) return;
    /// FIXME: This is not thread safe. See comment in setExceptionImpl() for details.
    _asyncReadActivated = true;
  }

  template<typename UserType>
  boost::shared_ptr<NDRegisterAccessor<UserType>> EpicsBackend::getRegisterAccessor_impl(
      const RegisterPath& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags) {
    RegisterPath path = "EPICS://" + registerPathName;

    EpicsBackendRegisterInfo info = _catalogue_mutable.getBackendRegister(registerPathName);

    if(numberOfWords + wordOffsetInRegister > info._pv->nElems || (numberOfWords == 0 && wordOffsetInRegister > 0)) {
      std::stringstream ss;
      ss << "Requested number of words/elements ( " << numberOfWords << ") with offset " << wordOffsetInRegister
         << " exceeds the number of available words/elements: " << info._pv->nElems;
      throw ChimeraTK::logic_error(ss.str());
    }

    if(numberOfWords == 0) numberOfWords = info._pv->nElems;

    unsigned base_type = info._pv->dbfType % (LAST_TYPE + 1);
    if(info._pv->dbfType == DBR_STSACK_STRING || info._pv->dbfType == DBR_CLASS_NAME) base_type = DBR_STRING;
    info._pv->name = (char*)info._caName.c_str();
    //    switch(info._dpfType){
    switch(base_type) {
        //      case DBR_STRING:
        //        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_string_t, UserType>>(path,
        //        shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister); break;
      case DBR_FLOAT:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_float_t, dbr_time_float, UserType>>(
            path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case DBR_DOUBLE:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_double_t, dbr_time_double, UserType>>(
            path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case DBR_CHAR:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_char_t, dbr_time_char, UserType>>(
            path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case DBR_SHORT:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_short_t, dbr_time_short, UserType>>(
            path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case DBR_LONG:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_long_t, dbr_time_long, UserType>>(
            path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
        break;
        //      case DBR_ENUM:
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
        throw ChimeraTK::runtime_error(std::string("Type ") + std::to_string(info._pv->dbfType) + " not implemented.");
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
    info._pv.reset(static_cast<pv*>(calloc(1, sizeof(pv))), free);
    info._caName = std::string(*pvName.get());
    info._pv->name = (char*)info._caName.c_str();
    _catalogue_mutable.addRegister(info);
    openChannel(info);
  }

  void EpicsBackend::openChannel(const EpicsBackendRegisterInfo& info) {
    bool channelAlreadyCreated = ChannelManager::getInstance().channelPresent(info._caName);
    if(!channelAlreadyCreated || info._pv->chid == nullptr) {
      auto result = ca_create_channel(
          info._pv->name, ChannelManager::channelStateHandler, this, DEFAULT_CA_PRIORITY, &info._pv->chid);
      ChannelManager::getInstance().addChannel(info._pv->chid, info._caName);
      if(result != ECA_NORMAL) {
        std::cerr << "CA error " << ca_message(result) << " occurred while trying to create channel " << info._pv->name
                  << std::endl;
        return;
      }
    }
  }

  void EpicsBackend::configureChannel(EpicsBackendRegisterInfo& info) {
    if(ca_state(info._pv->chid) != cs_conn) {
      std::stringstream ss;
      ss << "EPICS variable " << info._caName << " mapped to " << info.getRegisterName()
         << " is diconnected -> entry is not added to the catalogue." << std::endl;
      throw ChimeraTK::runtime_error(ss.str());
    }

    info._pv->nElems = ca_element_count(info._pv->chid);
    auto result = ca_pend_io(_caTimeout);
    if(result == ECA_TIMEOUT) {
      throw ChimeraTK::runtime_error("EPICS failed setting up time out.");
    }
    info._pv->dbfType = ca_field_type(info._pv->chid);
    info._pv->dbrType = dbf_type_to_DBR_TIME(info._pv->dbfType);

    if(ca_read_access(info._pv->chid) != 1) info._isReadable = false;
    if(ca_write_access(info._pv->chid) != 1) info._isWritable = false;

    if(info._pv->dbfType == DBF_DOUBLE || info._pv->dbfType == DBF_FLOAT) {
      info._dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, false, true, 320, 300);
    }
    else if(info._pv->dbfType == DBF_INT || info._pv->dbfType == DBF_LONG || info._pv->dbfType == DBF_SHORT) {
      info._dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, true, 320, 300);
    }
    else {
      std::stringstream ss;
      ss << "Failed to determine data type for EPICS variable " << info._caName << " mapped to "
         << info.getRegisterName() << " -> entry is not added to the catalogue." << std::endl;
      throw ChimeraTK::runtime_error(ss.str());
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
      ChimeraTK::runtime_error(std::string("Failed reading mapfile: ") + mapfileName);
    }

    auto result = ca_flush_io();
    if(result == ECA_TIMEOUT) {
      ChimeraTK::runtime_error("Channel setup failed.");
      return;
    }

    ChannelManager::getInstance().waitForConnections(_caTimeout);
  }

  void EpicsBackend::setExceptionImpl() noexcept {
    /// FIXME: This is not thread safe for several reasons!
    /// 1. _asyncReadActivated is a plain bool variable, so some thread synchronisation mechamism is missing (e.g.
    ///    convert into std::atomic).
    /// 2. We need the guarantee that nothing writes to the accessor queues beyond this point. In the
    ///    EpicsBackendRegisterAccessorBase, the _asyncReadActivated flag is first checked and then the queue is filled.
    ///    There is no guarantee that in between the check and the queue filling this function is called. This would
    ///    violate the specification and confuse e.g. ApplicationCore exception handling.
    _asyncReadActivated = false;
    /// FIXME: There is missing code here! The queues of all accessors need to be filled with exceptions (if asyncRead
    /// was active before)!
  }
} // namespace ChimeraTK
