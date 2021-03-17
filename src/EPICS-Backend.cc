/*
 * EPICS-Backend.cc
 *
 *  Created on: Jan 22, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include <iostream>
#include <fstream>
#include <vector>

#include "EPICS-Backend.h"
#include "EPICS-BackendRegisterAccessor.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/DeviceAccessVersion.h>

#include <boost/tokenizer.hpp>
typedef boost::tokenizer<boost::char_separator<char>> tokenizer;

extern "C"{
    boost::shared_ptr<ChimeraTK::DeviceBackend> ChimeraTK_DeviceAccess_createBackend(
        std::string address, std::map<std::string, std::string> parameters) {
      return ChimeraTK::EpicsBackend::createInstance(address, parameters);
    }

    std::vector<std::string> ChimeraTK_DeviceAccess_sdmParameterNames{"map"};

    std::string ChimeraTK_DeviceAccess_version{CHIMERATK_DEVICEACCESS_VERSION};

    std::string backend_name = "epics";
}

namespace ChimeraTK{
  EpicsBackend::BackendRegisterer EpicsBackend::backendRegisterer;

  EpicsBackend::EpicsBackend(const std::string &mapfile): _catalogue_filled(false){
    FILL_VIRTUAL_FUNCTION_TEMPLATE_VTABLE(getRegisterAccessor_impl);
    fillCatalogueFromMapFile(mapfile);
    _catalogue_filled = true;
    _isFunctional = true;
    auto result = ca_context_create(ca_disable_preemptive_callback);
    if (result != ECA_NORMAL) {
      std::stringstream ss;
      ss << "CA error " << ca_message(result) << "occurred while trying to start channel access.";
      throw ChimeraTK::runtime_error(ss.str());
    }
  }

  template<typename UserType>
  boost::shared_ptr< NDRegisterAccessor<UserType> > EpicsBackend::getRegisterAccessor_impl(
      const RegisterPath &registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags) {
    RegisterPath path = "EPICS://" + registerPathName;
    EpicsBackendRegisterInfo* info = nullptr;
    for(auto it = _catalogue_mutable.begin(), ite = _catalogue_mutable.end(); it != ite; it++){
      if(it->getRegisterName() == registerPathName){
        info = dynamic_cast<EpicsBackendRegisterInfo*>(&(*it));
        break;
      }
    }
    if(info == nullptr){
      throw ChimeraTK::logic_error(std::string("Requested register (") + registerPathName + ") was not found in the catalog.");
    }

    if(numberOfWords + wordOffsetInRegister > info->_pv.nElems ||
       (numberOfWords == 0 && wordOffsetInRegister > 0)){
      std::stringstream ss;
      ss << "Requested number of words/elements ( " << numberOfWords << ") with offset " + wordOffsetInRegister << " exceeds the number of available words/elements: " << info->_pv.nElems;
      throw ChimeraTK::logic_error(ss.str());
    }

    if(numberOfWords == 0)
      numberOfWords = info->_pv.nElems;

    unsigned base_type = info->_pv.dbfType % (LAST_TYPE+1);
    if(info->_pv.dbfType == DBR_STSACK_STRING || info->_pv.dbfType == DBR_CLASS_NAME)
      base_type = DBR_STRING;
    info->_pv.name = (char*)info->_caName.c_str();
//    switch(info->_dpfType){
    switch (base_type){
//      case DBR_STRING:
//        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_string_t, UserType>>(path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
//        break;
      case DBR_FLOAT:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_float_t, UserType>>(path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case DBR_DOUBLE:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_double_t, UserType>>(path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case DBR_CHAR:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_char_t, UserType>>(path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case DBR_INT:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_int_t, UserType>>(path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case DBR_LONG:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_long_t, UserType>>(path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
        break;
//      case DBR_ENUM:
//        if(dbr_type_is_CTRL(info->_dpfType))
//          return boost::make_shared<EpicsBackendRegisterAccessor<dbr_gr_enum, UserType>>(path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
//        else if (dbr_type_is_GR(info->_dpfType))
//          return boost::make_shared<EpicsBackendRegisterAccessor<dbr_ctrl_enum, UserType>>(path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
//        else
//          return boost::make_shared<EpicsBackendRegisterAccessor<int, UserType>>(path, shared_from_this(), info, flags, numberOfWords, wordOffsetInRegister);
//        break;
      default:
        throw ChimeraTK::runtime_error(std::string("Type ") + std::to_string(info->_pv.dbfType) + " not implemented.");
        break;
    }

  }

  EpicsBackend::BackendRegisterer::BackendRegisterer() {
    BackendFactory::getInstance().registerBackendType("epics", &EpicsBackend::createInstance, {"map"});
    std::cout << "BackendRegisterer: registered backend type epics" << std::endl;
  }

  boost::shared_ptr<DeviceBackend> EpicsBackend::createInstance(std::string /*address*/, std::map<std::string,std::string> parameters) {
    if(parameters["map"].empty()) {
      throw ChimeraTK::logic_error("No map file provided.");
    }
    return boost::shared_ptr<DeviceBackend> (new EpicsBackend(parameters["map"]));
  }

  pv EpicsBackend::createPV(const std::string &pvName){
    pv pv;
    pv.name = (char*)pvName.c_str();

    auto result = ca_create_channel(pv.name, 0, &pv, DEFAULT_CA_PRIORITY, &pv.chid);
    if(result != ECA_NORMAL){
      std::cerr << "CA error " << ca_message(result) << " occurred while trying to create channel " << pv.name << std::endl;
    }
    pv.status = result;
    return pv;
  }

  void EpicsBackend::addCatalogueEntry(RegisterPath path, std::shared_ptr<std::string> pvName){
    boost::shared_ptr<EpicsBackendRegisterInfo> info = boost::make_shared<EpicsBackendRegisterInfo>(path);
    info->_pv = createPV(*(pvName.get()));
    if(info->_pv.status != ECA_NORMAL)
      return;
    auto result = ca_pend_io(_caTimeout);
    if(result == ECA_TIMEOUT){
      std::cerr << "Channel time out for PV: " << pvName << std::endl;
      return;
    }
    info->_caName = std::string(*pvName.get());
    info->_isReadonly = false;

    info->_pv.nElems = ca_element_count(info->_pv.chid);
    info->_pv.dbfType = ca_field_type(info->_pv.chid);
    info->_pv.dbrType = dbf_type_to_DBR_TIME(info->_pv.dbfType);

    // try reading
    result = ca_array_get(info->_pv.dbrType, info->_pv.nElems, info->_pv.chid, info->_pv.value);
    if(result == ECA_NORDACCESS){
      std::cerr << "No read access to PV: " << info->_pv.name << " -> will not be added to the catalogue." << std::endl;
    } else if (result == ECA_NOWTACCESS){
      info->_isReadonly = true;
    }

    info->_dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                        true, false, 320, 300 );
    _catalogue_mutable.addRegister(info);
  }

  void EpicsBackend::fillCatalogueFromMapFile(const std::string &mapfileName){
    boost::char_separator<char> sep{"\t ", "", boost::drop_empty_tokens};
    std::string line;
    std::ifstream mapfile (mapfileName);
    if (mapfile.is_open()) {
      while (std::getline(mapfile,line)) {
        if(line.empty())
          continue;
        tokenizer tok{line, sep};
        size_t nTokens = std::distance(tok.begin(), tok.end());
        if (!(nTokens == 2)){
          std::cerr << "Wrong number of tokens (" << nTokens << ") in mapfile " << mapfileName << " line (-> line is ignored): \n "  << line << std::endl;
          continue;
        }
        auto it = tok.begin();

        try{
          std::shared_ptr<std::string> pathStr = std::make_shared<std::string>(*it);
          RegisterPath path(*(pathStr.get()));
          it++;
          std::shared_ptr<std::string> nodeName = std::make_shared<std::string>(*it);
          addCatalogueEntry(path, nodeName);
        } catch (std::out_of_range &e){
          std::cerr << "Failed reading the following line from mapping file " << mapfileName
              << "\n " << line << std::endl;
        }
      }
      mapfile.close();
    } else {
      ChimeraTK::runtime_error(std::string("Failed reading mapfile: ") + mapfileName);
    }
  }
}
