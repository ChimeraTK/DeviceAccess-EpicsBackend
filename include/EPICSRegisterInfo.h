// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

/*
 * EPICSRegisterInfo.h
 *
 *  Created on: Oct 19, 2023
 *      Author: Klaus Zenker (HZDR)
 */

#include <ChimeraTK/BackendRegisterCatalogue.h>

namespace ChimeraTK {

  struct EpicsBackendRegisterInfo : public BackendRegisterInfoBase {
    EpicsBackendRegisterInfo(const RegisterPath& path) : _name(path) {};
    EpicsBackendRegisterInfo() = default;

    RegisterPath getRegisterName() const override { return _name; }

    std::string getRegisterPath() const { return _name; }

    unsigned int getNumberOfElements() const override { return _nElements; }

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
    unsigned int _nElements{};
    long _dbfType{};

    // this is needed because the name inside _pv is just a pointer
    std::string _caName;
  };
} // namespace ChimeraTK
