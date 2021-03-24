/*
 * EPICS-Backend.h
 *
 *  Created on: Jan 22, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#ifndef INCLUDE_EPICS_BACKEND_H_
#define INCLUDE_EPICS_BACKEND_H_

#include <ChimeraTK/DeviceBackendImpl.h>

#include "EPICS_types.h"
#include <cadef.h>

#include <string.h>
#include <memory>



namespace ChimeraTK {
struct EpicsBackendRegisterInfo : public RegisterInfo {

  EpicsBackendRegisterInfo(const RegisterPath &path):_name(path){};

  virtual ~EpicsBackendRegisterInfo() {}

  RegisterPath getRegisterName() const override { return _name; }

  std::string getRegisterPath() const { return _name; }

  unsigned int getNumberOfElements() const override { return _pv->nElems; }

  unsigned int getNumberOfChannels() const override { return 1; }

  unsigned int getNumberOfDimensions() const override { return _pv->nElems > 1 ? 1 : 0; }

  const RegisterInfo::DataDescriptor& getDataDescriptor() const override { return _dataDescriptor; }

  bool isReadable() const override {return _isReadable;}

  bool isWriteable() const override {return _isWritable;}

  AccessModeFlags getSupportedAccessModes() const override {return _accessModes;}

  RegisterPath _name;
  bool _isReadable{true}, _isWritable{true};
  RegisterInfo::DataDescriptor _dataDescriptor;
  AccessModeFlags _accessModes{};

  //\ToDo: Use pointer to have name persistent
  std::shared_ptr<pv> _pv;
  // this is needed because the name inside _pv is just a pointer
  std::string _caName;



};

class EpicsBackend : public DeviceBackendImpl{
public:
  ~EpicsBackend(){ca_context_destroy();}
  static boost::shared_ptr<DeviceBackend> createInstance(std::string address, std::map<std::string,std::string> parameters);
  static void channelStateHandler(connection_handler_args args);

protected:
  EpicsBackend(const std::string &mapfile = "");

  /**
   * Return the catalogue and if not filled yet fill it.
   */
  const RegisterCatalogue& getRegisterCatalogue() const override {return _catalogue_mutable;};

  void setException() override {};

  void open() override {
    _opened = true;
   _isFunctional = true;
  };

  void close() override {
    _opened = false;
  };

  std::string readDeviceInfo() override {
    std::stringstream ss;
    ss << "EPICS Server";
    return ss.str();
  }

  bool isFunctional() const override {return _isFunctional;};

  void activateAsyncRead() noexcept override {};

  template<typename UserType>
  boost::shared_ptr< NDRegisterAccessor<UserType> > getRegisterAccessor_impl(const RegisterPath &registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);

  DEFINE_VIRTUAL_FUNCTION_TEMPLATE_VTABLE_FILLER( EpicsBackend, getRegisterAccessor_impl, 4);


  /** We need to make the catalogue mutable, since we fill it within getRegisterCatalogue() */
  mutable RegisterCatalogue _catalogue_mutable;

  /** Class to register the backend type with the factory. */
  class BackendRegisterer {
    public:
      BackendRegisterer();
  };
  static BackendRegisterer backendRegisterer;

  bool isAsyncReadActive(){return _asyncReadActivated;}

  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  friend class EpicsBackendRegisterAccessor;

private:
  /**
   * Catalogue is filled when device is opened. When working with LogicalNameMapping the
   * catalogue is requested even if the device is not opened!
   * Keep track if catalogue is filled using this bool.
   */
  bool _catalogue_filled;

  bool _isFunctional{false};

  bool _asyncReadActivated{false};

  double _caTimeout{1.0};

  void fillCatalogueFromMapFile(const std::string &mapfile);

  void addCatalogueEntry(RegisterPath path, std::shared_ptr<std::string> pvName);

  void configureChannel(EpicsBackendRegisterInfo *info);

};

}

#endif /* INCLUDE_EPICS_BACKEND_H_ */
