// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * EPICS-Backend.h
 *
 *  Created on: Jan 22, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "EPICSRegisterInfo.h"
#include "EPICSTypes.h"

#include <ChimeraTK/BackendRegisterCatalogue.h>
#include <ChimeraTK/DeviceBackendImpl.h>
#include <ChimeraTK/VersionNumber.h>

#include <cadef.h>
#include <string.h>

#include <atomic>
#include <memory>

namespace ChimeraTK {

  class EpicsBackend : public DeviceBackendImpl {
   public:
    ~EpicsBackend();
    static boost::shared_ptr<DeviceBackend> createInstance(
        std::string address, std::map<std::string, std::string> parameters);
    void setBackendState(const bool& channelAccessUp) { _channelAccessUp = channelAccessUp; }
    std::atomic<bool> _asyncReadActivated{false};
    std::atomic<bool> _channelAccessUp{false};

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

    /**
     * During creation already channels are set up.
     * When entering open() the first time this is a special case,
     * because normally close() was called before and no channels are set up.
     */
    bool _freshCreated;

    VersionNumber _startVersion{nullptr};

    void fillCatalogueFromMapFile(const std::string& mapfile);

    void addCatalogueEntry(RegisterPath path, std::shared_ptr<std::string> pvName);

    void configureChannel(EpicsBackendRegisterInfo& info);

    /**
     * Prepare channel access context.
     */
    void prepareChannelAccess();
  };

} // namespace ChimeraTK
