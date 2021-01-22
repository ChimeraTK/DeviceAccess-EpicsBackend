/*
 * EPICS-Backend.h
 *
 *  Created on: Jan 22, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#ifndef INCLUDE_EPICS_BACKEND_H_
#define INCLUDE_EPICS_BACKEND_H_

#include <ChimeraTK/DeviceBackendImpl.h>
#include <boost/enable_shared_from_this.hpp>

#include "EPICS_types.h"

#include <string.h>

namespace ChimeraTK {
struct EpicsBackendRegisterInfo : public RegisterInfo {

  EpicsBackendRegisterInfo(const std::string name):_name(name){};

  virtual ~EpicsBackendRegisterInfo() {}

  RegisterPath getRegisterName() const override { return RegisterPath(_name); }

  std::string getRegisterPath() const { return _path; }

  unsigned int getNumberOfElements() const override { return _arrayLength; }

  unsigned int getNumberOfChannels() const override { return 1; }

  unsigned int getNumberOfDimensions() const override { return _arrayLength > 1 ? 1 : 0; }

  const RegisterInfo::DataDescriptor& getDataDescriptor() const override { return _dataDescriptor; }

  bool isReadable() const override {return true;}

  bool isWriteable() const override {return !_isReadonly;}

  AccessModeFlags getSupportedAccessModes() const override {return _accessModes;}

  RegisterPath _path;
  std::string _name;
  bool _isReadonly;
  size_t _arrayLength;
  RegisterInfo::DataDescriptor _dataDescriptor;
  AccessModeFlags _accessModes{};

  long _dpfType; // Data type
  chid _id; // Channel id



};

class EpicsBackend : public DeviceBackendImpl{
public:
  static boost::shared_ptr<DeviceBackend> createInstance(std::string address, std::map<std::string,std::string> parameters);

protected:
  EpicsBackend(const std::string &mapfile = "");

  void addCatalogueEntry(const std::string &pvName);

  pv createPV(const std::string &pvName);

  /**
   * Return the catalogue and if not filled yet fill it.
   */
  const RegisterCatalogue& getRegisterCatalogue() const override {return _catalogue_mutable;};

  void setException() override {};

  void open() override {};

  void close() override {};

  bool isFunctional() const override {return _isFunctional;};

  void activateAsyncRead() noexcept override {};

  template<typename UserType>
  boost::shared_ptr< NDRegisterAccessor<UserType> > getRegisterAccessor_impl(const RegisterPath &registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);

  DEFINE_VIRTUAL_FUNCTION_TEMPLATE_VTABLE_FILLER( EpicsBackend, getRegisterAccessor_impl, 1);


  /** We need to make the catalogue mutable, since we fill it within getRegisterCatalogue() */
  mutable RegisterCatalogue _catalogue_mutable;

  /** Class to register the backend type with the factory. */
  class BackendRegisterer {
    public:
      BackendRegisterer();
  };
  static BackendRegisterer backendRegisterer;

  bool isAsyncReadActive(){return _asyncReadActivated;}

  template<typename UAType, typename CTKType>
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
};

}

#endif /* INCLUDE_EPICS_BACKEND_H_ */
