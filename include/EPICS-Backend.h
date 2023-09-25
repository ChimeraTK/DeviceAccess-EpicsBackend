/*
 * EPICS-Backend.h
 *
 *  Created on: Jan 22, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#ifndef INCLUDE_EPICS_BACKEND_H_
#define INCLUDE_EPICS_BACKEND_H_

#include "EPICS_types.h"

#include <ChimeraTK/BackendRegisterCatalogue.h>
#include <ChimeraTK/DeviceBackendImpl.h>

#include <cadef.h>
#include <memory>
#include <string.h>

namespace ChimeraTK {

  struct EpicsBackendRegisterInfo : public BackendRegisterInfoBase {
    EpicsBackendRegisterInfo(const RegisterPath& path) : _name(path){};
    EpicsBackendRegisterInfo() = default;

    RegisterPath getRegisterName() const override { return _name; }

    std::string getRegisterPath() const { return _name; }

    unsigned int getNumberOfElements() const override { return _pv->nElems; }

    unsigned int getNumberOfChannels() const override { return 1; }

    const DataDescriptor& getDataDescriptor() const override { return _dataDescriptor; }

    bool isReadable() const override { return _isReadable; }

    bool isWriteable() const override { return _isWritable; }

    AccessModeFlags getSupportedAccessModes() const override { return _accessModes; }

    std::unique_ptr<BackendRegisterInfoBase> clone() const override {
      return std::unique_ptr<BackendRegisterInfoBase>(new EpicsBackendRegisterInfo(*this));
    }

    RegisterPath _name;
    bool _isReadable{true}, _isWritable{true};
    DataDescriptor _dataDescriptor;
    AccessModeFlags _accessModes{};

    //\ToDo: Use pointer to have name persistent
    std::shared_ptr<pv> _pv;
    // this is needed because the name inside _pv is just a pointer
    std::string _caName;
  };

  class EpicsBackend : public DeviceBackendImpl {
   public:
    ~EpicsBackend();
    static boost::shared_ptr<DeviceBackend> createInstance(
        std::string address, std::map<std::string, std::string> parameters);
    void setBackendState(bool isFunctional) { _isFunctional = isFunctional; }
    bool _asyncReadActivated{false};

   protected:
    EpicsBackend(const std::string& mapfile = "");

    /**
     * Return the catalog and if not filled yet fill it.
     */
    RegisterCatalogue getRegisterCatalogue() const override { return RegisterCatalogue(_catalogue_mutable.clone()); };

    void setException() override;

    void open() override;

    void close() override;

    std::string readDeviceInfo() override {
      std::stringstream ss;
      ss << "EPICS Server";
      return ss.str();
    }

    bool isFunctional() const override { return _isFunctional; };

    void activateAsyncRead() noexcept override;

    template<typename UserType>
    boost::shared_ptr<NDRegisterAccessor<UserType>> getRegisterAccessor_impl(
        const RegisterPath& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);

    DEFINE_VIRTUAL_FUNCTION_TEMPLATE_VTABLE_FILLER(EpicsBackend, getRegisterAccessor_impl, 4);

    /** We need to make the catalog mutable, since we fill it within getRegisterCatalogue() */
    mutable BackendRegisterCatalogue<EpicsBackendRegisterInfo> _catalogue_mutable;

    /** Class to register the backend type with the factory. */
    class BackendRegisterer {
     public:
      BackendRegisterer();
    };
    static BackendRegisterer backendRegisterer;

    bool isAsyncReadActive() { return _asyncReadActivated; }

    template<typename EpicsBaseType, typename EpicsType, typename CTKType>
    friend class EpicsBackendRegisterAccessor;

   private:
    /**
     * Catalog is filled when device is opened. When working with LogicalNameMapping the
     * Catalog is requested even if the device is not opened!
     * Keep track if catalog is filled using this bool.
     */
    bool _catalogue_filled;

    bool _isFunctional{false};

    double _caTimeout{1.0};

    std::string _mapfile;

    void fillCatalogueFromMapFile(const std::string& mapfile);

    void addCatalogueEntry(RegisterPath path, std::shared_ptr<std::string> pvName);

    /**
     * \throw ChimeraTK::runtime_error if channel is disconnected
     */
    void configureChannel(EpicsBackendRegisterInfo& info);
  };

} // namespace ChimeraTK

#endif /* INCLUDE_EPICS_BACKEND_H_ */
