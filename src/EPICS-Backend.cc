/*
 * EPICS-Backend.cc
 *
 *  Created on: Jan 22, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include <iostream>
#include <fstream>

#include "EPICS-Backend.h"
#include "EPICS-BackendRegisterAccessor.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/DeviceAccessVersion.h>

#include <boost/tokenizer.hpp>
typedef boost::tokenizer<boost::char_separator<char>> tokenizer;

#include <cadef.h>

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
    auto result = ca_context_create(ca_disable_preemptive_callback);
    if (result != ECA_NORMAL) {
      std::stringstream ss;
      ss << "CA error " << ca_message(result) << "occurred while trying to start channel access.";
      throw ChimeraTK::runtime_error(ss.str());
    }
  }
  EpicsBackend::BackendRegisterer::BackendRegisterer() {
    BackendFactory::getInstance().registerBackendType("epics", &EpicsBackend::createInstance, {"map"});
    std::cout << "BackendRegisterer: registered backend type epics" << std::endl;
  }

  template<typename UserType>
  boost::shared_ptr< NDRegisterAccessor<UserType> > EpicsBackend::getRegisterAccessor_impl(const RegisterPath &registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags) {
    std::string path = "EPICS://";
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

    if(numberOfWords + wordOffsetInRegister > info->_arrayLength ||
       (numberOfWords == 0 && wordOffsetInRegister > 0)){
      std::stringstream ss;
      ss << "Requested number of words/elements ( " << numberOfWords << ") with offset " + wordOffsetInRegister << " exceeds the number of available words/elements: " << info->_arrayLength;
      throw ChimeraTK::logic_error(ss.str());
    }

    if(numberOfWords == 0)
      numberOfWords = info->_arrayLength;

    unsigned base_type = info->_dpfType % (LAST_TYPE+1);
    if(info->_dpfType == DBR_STSACK_STRING || info->_dpfType == DBR_CLASS_NAME)
      base_type = DBR_STRING;
    switch(info->_dpfType){
//      case DBR_STRING:
//        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_string_t, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
//        break;
      case DBR_FLOAT:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_float_t, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case DBR_DOUBLE:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_double_t, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
//      case DBR_CHAR:
//        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_char_t, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
//        break;
      case DBR_INT:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_int_t, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case DBR_LONG:
        return boost::make_shared<EpicsBackendRegisterAccessor<dbr_long_t, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
//      case DBR_ENUM:
//        if(dbr_type_is_CTRL(info->_dpfType))
//          return boost::make_shared<EpicsBackendRegisterAccessor<dbr_gr_enum, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
//        else if (dbr_type_is_GR(info->_dpfType))
//          return boost::make_shared<EpicsBackendRegisterAccessor<dbr_ctrl_enum, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
//        else
//          return boost::make_shared<EpicsBackendRegisterAccessor<int, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
//        break;
      default:
        throw ChimeraTK::runtime_error(std::string("Type ") + std::to_string(info->_dpfType) + " not implemented.");
        break;
    }

  }

  boost::shared_ptr<DeviceBackend> EpicsBackend::createInstance(std::string address, std::map<std::string,std::string> parameters) {
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
    pv pv = createPV(*(pvName.get()));
    if(pv.status != ECA_NORMAL)
      return;
    auto result = ca_pend_io(_caTimeout);
    if(result == ECA_TIMEOUT){
      std::cerr << "Channel time out for PV: " << pvName << std::endl;
      return;
    }

    EpicsBackendRegisterInfo info(path);
    info._isReadonly = false;
    // try reading
    result = ca_array_get(pv.dbrType, pv.nElems, pv.chid, pv.value);
    if(result == ECA_NORDACCESS){
      std::cerr << "No read access to PV: " << pv.name << " -> will not be added to the catalogue." << std::endl;
    } else if (result == ECA_NOWTACCESS){
      info._isReadonly = true;
    }

    info._arrayLength = ca_element_count(pv.chid);
    info._dpfType = ca_field_type(pv.chid);
    info._id = pv.chid;
    _catalogue_mutable.addRegister(info);

    ca_context_destroy();
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
