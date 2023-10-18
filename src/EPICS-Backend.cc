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

  EpicsBackend::EpicsBackend(const std::string& mapfile) : _catalogue_filled(false) {
    FILL_VIRTUAL_FUNCTION_TEMPLATE_VTABLE(getRegisterAccessor_impl);
    auto result = ca_context_create(ca_enable_preemptive_callback);
    if(result != ECA_NORMAL) {
      std::stringstream ss;
      ss << "CA error " << ca_message(result) << "occurred while trying to start channel access.";
      throw ChimeraTK::runtime_error(ss.str());
    }

    fillCatalogueFromMapFile(mapfile);
    _catalogue_filled = true;
    _isFunctional = true;
  }

  EpicsBackend::~EpicsBackend() {
    _asyncReadActivated = false;
    ChannelManager::getInstance().cleanup();
    if(_isFunctional) ca_context_destroy();
  }

  void EpicsBackend::activateAsyncRead() noexcept {
    if(!_opened || !_isFunctional) return;
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
    info._pv.reset((pv*)calloc(1, sizeof(pv)));
    info._caName = std::string(*pvName.get());
    info._pv->name = (char*)info._caName.c_str();
    bool channelAlreadyCreated = ChannelManager::getInstance().channelPresent(info._caName);
    if(!channelAlreadyCreated) {
      auto result = ca_create_channel(
          info._pv->name, ChannelManager::channelStateHandler, this, DEFAULT_CA_PRIORITY, &info._pv->chid);
      ChannelManager::getInstance().addChannel(info._pv->chid, info._caName);
      if(result != ECA_NORMAL) {
        std::cerr << "CA error " << ca_message(result) << " occurred while trying to create channel " << info._pv->name
                  << std::endl;
        return;
      }
    }
    _catalogue_mutable.addRegister(info);
  }

  void EpicsBackend::configureChannel(EpicsBackendRegisterInfo& info) {
    info._pv->nElems = ca_element_count(info._pv->chid);
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
      std::cerr << "Failed to determine data type for node: " << info._pv->name
                << " -> entry is not added to the catalogue." << std::endl;
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
    size_t n = _caTimeout * 10.0 / 0.1; // sleep 100ms per loop, wait _caTimeout until giving up
    for(size_t i = 0; i < n; i++) {
      if(_isFunctional) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if(!_isFunctional) {
      throw ChimeraTK::runtime_error("Failed to establish channel access connection.");
    }
    for(auto& reg : _catalogue_mutable) {
      configureChannel(reg);
    }
  }

  void EpicsBackend::setException() {
    //\ToDo: Why I have to check is functional here? If not setException is called constantly and which means
    //_isFunctional will stay false even if reset in the state handler.
    if(_isFunctional) {
      _isFunctional = false;
      _asyncReadActivated = false;
      ChannelManager::getInstance().setException(std::string("Exception reported by another accessor."));
    }
  }
} // namespace ChimeraTK
