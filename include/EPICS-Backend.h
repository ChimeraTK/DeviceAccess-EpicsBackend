// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * EPICS-Backend.h
 *
 *  Created on: Jan 22, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "EPICS_types.h"
#include "EPICSRegisterInfo.h"

#include <ChimeraTK/BackendRegisterCatalogue.h>
#include <ChimeraTK/DeviceBackendImpl.h>

#include <cadef.h>
#include <memory>
#include <string.h>

namespace ChimeraTK {

  class EpicsBackend : public DeviceBackendImpl {
   public:
    ~EpicsBackend();
    static boost::shared_ptr<DeviceBackend> createInstance(
        std::string address, std::map<std::string, std::string> parameters);
    bool _asyncReadActivated{false};

   protected:
    EpicsBackend(const std::string& mapfile = "");

    /**
     * Return the catalog and if not filled yet fill it.
     */
    RegisterCatalogue getRegisterCatalogue() const override { return RegisterCatalogue(_catalogue_mutable.clone()); };

    void setExceptionImpl() noexcept override;

    void open() override;

    void close() override;

    std::string readDeviceInfo() override {
      std::stringstream ss;
      ss << "EPICS Server";
      return ss.str();
    }

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

    double _caTimeout{1.0};

    std::string _mapfile;

    void fillCatalogueFromMapFile(const std::string& mapfile);

    /**
     * Open the Channel Access channel. Add channel to the CahnnelManager.
     */
    void openChannel(const EpicsBackendRegisterInfo& info);

    void addCatalogueEntry(RegisterPath path, std::shared_ptr<std::string> pvName);

    /**
     * \throw ChimeraTK::runtime_error if channel is disconnected
     */
    void configureChannel(EpicsBackendRegisterInfo& info);
  };

} // namespace ChimeraTK
