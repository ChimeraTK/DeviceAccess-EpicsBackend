// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * EPICS-BackendRegisterAccessor.h
 *
 *  Created on: Jan 22, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "EPICS-Backend.h"
#include "EPICSChannelManager.h"
#include "EPICSTypes.h"
#include "EPICSVersionMapper.h"

#include <ChimeraTK/AccessMode.h>
#include <ChimeraTK/NDRegisterAccessor.h>
#include <ChimeraTK/RegisterPath.h>

#include <boost/enable_shared_from_this.hpp>

#include <cadef.h>
#include <cstring> // memcpy
#include <string>
namespace ChimeraTK {

  template<typename DestType, typename SourceType>
  class EpicsRangeCheckingDataConverter {
    /** define round type for the boost::numeric::converter */
    template<class S>
    struct Round {
      static S nearbyint(S s) { return round(s); }

      typedef boost::mpl::integral_c<std::float_round_style, std::round_to_nearest> round_style;
    };
    typedef boost::numeric::converter<DestType, SourceType, boost::numeric::conversion_traits<DestType, SourceType>,
        boost::numeric::def_overflow_handler, Round<SourceType>>
        converter;

   public:
    DestType convert(SourceType& x) {
      try {
        return converter::convert(x);
      }
      catch(boost::numeric::positive_overflow&) {
        return std::numeric_limits<DestType>::max();
      }
      catch(boost::numeric::negative_overflow&) {
        return std::numeric_limits<DestType>::min();
      }
      catch(...) {
        throw ChimeraTK::runtime_error("Data conversion failed for unknown reason.");
      }
    }
  };

  template<typename DestType>
  class EpicsRangeCheckingDataConverter<DestType, Void> {
   public:
    DestType convert([[maybe_unused]] Void& x) { return DestType(); } // default constructed value
  };

  // partial specialization of conversion to void
  template<typename SourceType>
  class EpicsRangeCheckingDataConverter<Void, SourceType> {
   public:
    Void convert([[maybe_unused]] SourceType& x) { return Void(); }
  };

  template<typename SourceType>
  class EpicsRangeCheckingDataConverter<std::string, SourceType> {
   public:
    std::string convert(SourceType& x) { return std::to_string(x); }
  };

  template<>
  class EpicsRangeCheckingDataConverter<std::string, Void> {
   public:
    std::string convert(Void& /*x*/) { return std::string("void"); }
  };

  template<typename DestType>
  class EpicsRangeCheckingDataConverter<DestType, std::string> {
   public:
    DestType convert(std::string&) { throw std::logic_error("Conversion from string is not allowed."); }
  };

  template<>
  class EpicsRangeCheckingDataConverter<Void, std::string> {
   public:
    Void convert(std::string&) { return Void(); }
  };

  template<>
  class EpicsRangeCheckingDataConverter<dbr_string_t, Void> {
   public:
    std::string convert(Void& /*x*/) { return std::string("void"); }
  };

  template<>
  class EpicsRangeCheckingDataConverter<Void, dbr_string_t> {
   public:
    Void convert(dbr_string_t&) { return Void(); }
  };

  template<>
  class EpicsRangeCheckingDataConverter<std::string, dbr_string_t> {
   public:
    std::string convert(dbr_string_t& x) { return std::string(x); }
  };

  template<typename DestType>
  class EpicsRangeCheckingDataConverter<DestType, dbr_string_t> {
   public:
    DestType convert(char*) { throw std::logic_error("Conversion from string is not allowed."); }
  };

  template<typename SourceType>
  class EpicsRangeCheckingDataConverter<dbr_string_t, SourceType> {
   public:
    std::string convert(SourceType& x) { return std::to_string(x); }
  };

  template<>
  class EpicsRangeCheckingDataConverter<dbr_string_t, std::string> {
   public:
    std::string convert(std::string& x) { return x; }
  };

  template<>
  class EpicsRangeCheckingDataConverter<dbr_string_t, ChimeraTK::Boolean> {
   public:
    std::string convert(ChimeraTK::Boolean& x) { return (x == true ? std::string("true") : std::string("false")); }
  };

  class EpicsBackendRegisterAccessorBase {
   public:
    EpicsBackendRegisterAccessorBase(boost::shared_ptr<EpicsBackend> backend, const EpicsBackendRegisterInfo& info,
        size_t numberOfWords, size_t wordOffsetInRegister)
    : _info(info), _backend(backend), _numberOfWords(numberOfWords), _offsetWords(wordOffsetInRegister) {}
    EpicsBackendRegisterInfo _info;
    cppext::future_queue<EpicsRawData> _notifications;
    boost::shared_ptr<EpicsBackend> _backend;
    size_t _numberOfWords; ///< Requested array length. Could be smaller than what is available on the server.
    size_t _offsetWords;   ///< Requested offset for arrays.
    bool _isPartial{false};
    ChimeraTK::VersionNumber _currentVersion;
    bool _hasNotificationsQueue{false};
    /**
     * Push value to the notification queue. Used if subscription already exists and an additional accessor is added to
     * the ChannelManager.
     */
    virtual void setInitialValue(void* value, long type, long count) = 0;
  };

  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  class EpicsBackendRegisterAccessor : public EpicsBackendRegisterAccessorBase, public NDRegisterAccessor<CTKType> {
   public:
    ~EpicsBackendRegisterAccessor();

    void doReadTransferSynchronously() override;

    void doPreRead(TransferType) override {
      if(!_backend->isOpen()) throw ChimeraTK::logic_error("Read operation not allowed while device is closed.");
    }

    void doPreWrite(TransferType, VersionNumber) override {
      if(!_backend->isOpen()) throw ChimeraTK::logic_error("Write operation not allowed while device is closed.");
    }

    bool doWriteTransfer(VersionNumber /*versionNumber*/ = {}) override;

    void doPostRead(TransferType, bool hasNewData) override;

    /**
     * Push value to the notification queue. Used if subscription already exists and an additional accessor is added to
     * the ChannelManager.
     */
    void setInitialValue(void* value, long type, long count) override;

    bool isReadOnly() const override { return (_info._isReadable && !_info._isWritable); }

    bool isReadable() const override { return true; }

    bool isWriteable() const override { return _info._isWritable; }

    void interrupt() override { this->interrupt_impl(this->_notifications); }

    using TransferElement::_readQueue;

    std::vector<boost::shared_ptr<TransferElement>> getHardwareAccessingElements() override {
      return {boost::enable_shared_from_this<TransferElement>::shared_from_this()};
    }

    std::list<boost::shared_ptr<TransferElement>> getInternalElements() override { return {}; }

    void replaceTransferElement(boost::shared_ptr<TransferElement> /*newElement*/) override {} // LCOV_EXCL_LINE

    friend class EpicsBackend;

    EpicsRangeCheckingDataConverter<CTKType, EpicsBaseType> toCTK;
    EpicsRangeCheckingDataConverter<EpicsBaseType, CTKType> toEpics;

    EpicsBackendRegisterAccessor(const RegisterPath& path, boost::shared_ptr<DeviceBackend> backend,
        const EpicsBackendRegisterInfo& registerInfo, AccessModeFlags flags, size_t numberOfWords,
        size_t wordOffsetInRegister, bool asyncReadActivated);
  };

  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  EpicsBackendRegisterAccessor<EpicsBaseType, EpicsType, CTKType>::EpicsBackendRegisterAccessor(
      const RegisterPath& path, boost::shared_ptr<DeviceBackend> backend, const EpicsBackendRegisterInfo& registerInfo,
      AccessModeFlags flags, size_t numberOfWords, size_t wordOffsetInRegister, bool asyncReadActivated)
  : EpicsBackendRegisterAccessorBase(
        boost::dynamic_pointer_cast<EpicsBackend>(backend), registerInfo, numberOfWords, wordOffsetInRegister),
    NDRegisterAccessor<CTKType>(path, flags) {
    if constexpr(std::is_array_v<EpicsBaseType>) {
      if(numberOfWords > 1) {
        throw ChimeraTK::logic_error("No support for lso array.");
      }
    }
    if(flags.has(AccessMode::raw)) throw ChimeraTK::logic_error("Raw access mode is not supported.");
    NDRegisterAccessor<CTKType>::buffer_2D.resize(1);
    this->accessChannel(0).resize(numberOfWords);
    std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
    auto pv = ChannelManager::getInstance().getPV(_info._caName);
    if(flags.has(AccessMode::wait_for_new_data)) {
      _hasNotificationsQueue = true;
      _notifications = cppext::future_queue<EpicsRawData>(3);
      _readQueue = _notifications.then<void>(
          [pv](EpicsRawData& data) { memcpy(pv->value, data.data, data.size); }, std::launch::deferred);
    }
    if(pv->nElems != numberOfWords) _isPartial = true;
    ChannelManager::getInstance().addAccessor(_info._caName, this);
    if(flags.has(AccessMode::wait_for_new_data) && asyncReadActivated) {
      ChannelManager::getInstance().activateChannel(_info._caName);
    }
    NDRegisterAccessor<CTKType>::_exceptionBackend = backend;
  }

  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  void EpicsBackendRegisterAccessor<EpicsBaseType, EpicsType, CTKType>::setInitialValue(
      void* value, long type, long count) {
    auto data = EpicsRawData(value, type, count);
    _notifications.push_overwrite(std::move(data));
  }

  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  void EpicsBackendRegisterAccessor<EpicsBaseType, EpicsType, CTKType>::doReadTransferSynchronously() {
    _backend->checkActiveException();
    std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
    auto pv = ChannelManager::getInstance().getPV(_info._caName);
    // one could also use ChannelManager::isChannelConnected -> however we ask explicitly ChannelAccess here
    if(ca_state(pv->chid) == cs_conn) {
      if(pv->nElems == 1) {
        auto result = ca_get(pv->dbrType, pv->chid, pv->value);
        if(result != ECA_NORMAL) {
          throw ChimeraTK::runtime_error(std::string("Failed to read pv: ") + pv->name);
        }
        result = ca_pend_io(default_ca_timeout);
        if(result == ECA_TIMEOUT) {
          throw ChimeraTK::runtime_error(std::string("Read operation timed out for pv: ") + pv->name);
        }
      }
      else {
        auto result = ca_array_get(pv->dbrType, pv->nElems, pv->chid, pv->value);
        if(result != ECA_NORMAL) {
          throw ChimeraTK::runtime_error(std::string("Failed to read pv: ") + pv->name);
        }
        result = ca_pend_io(default_ca_timeout);
        if(result == ECA_TIMEOUT) {
          throw ChimeraTK::runtime_error(std::string("Read operation timed out for pv: ") + pv->name);
        }
      }
    }
    else {
      throw ChimeraTK::runtime_error(
          std::string("ChannelAccess not connected in doReadTransferSynchronously when writing: ") + _info._name + "(" +
          pv->name + ")");
    }
  }

  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  void EpicsBackendRegisterAccessor<EpicsBaseType, EpicsType, CTKType>::doPostRead(TransferType, bool hasNewData) {
    if(!hasNewData) return;
    std::lock_guard<std::mutex> lock(ChannelManager::getInstance().mapLock);
    auto pv = ChannelManager::getInstance().getPV(_info._caName);
    EpicsBaseType* tmp = (EpicsBaseType*)dbr_value_ptr(pv->value, pv->dbrType);

    if constexpr(std::is_array_v<EpicsBaseType>) {
      // only single element as checked in the constructor
      dbr_string_t tmpStr;
      strcpy(tmpStr, (char*)tmp);
      this->accessData(0) = toCTK.convert(tmpStr);
    }
    else {
      for(size_t i = 0; i < _numberOfWords; i++) {
        EpicsBaseType value = tmp[_offsetWords + i];
        // Fill the NDRegisterAccessor buffer
        this->accessData(i) = toCTK.convert(value);
      }
    }

    EpicsType* tp = (EpicsType*)pv->value;
    _currentVersion = EPICS::VersionMapper::getInstance().getVersion(tp[0].stamp);
    if(_currentVersion < _backend->_startVersion) {
      _currentVersion = _backend->_startVersion;
    }
    TransferElement::_versionNumber = _currentVersion;
  }

  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  bool EpicsBackendRegisterAccessor<EpicsBaseType, EpicsType, CTKType>::doWriteTransfer(
      VersionNumber /*versionNumber*/) {
    _backend->checkActiveException();
    auto pv = ChannelManager::getInstance().getPV(_info._caName);
    // one could also use ChannelManager::isChannelConnected -> however we ask explicitly ChannelAccess here
    if(ca_state(pv->chid) == cs_conn) {
      if(_isPartial) EpicsBackendRegisterAccessor<EpicsBaseType, EpicsType, CTKType>::doReadTransferSynchronously();
      EpicsBaseType* tmp = (EpicsBaseType*)dbr_value_ptr(pv->value, pv->dbrType);
      long result;
      if constexpr(std::is_array_v<EpicsBaseType>) {
        // only single element as checked in the constructor
        result = ca_array_put(pv->dbfType, pv->nElems, pv->chid, toEpics.convert(this->accessData(0)).c_str());
      }
      else {
        for(size_t i = 0; i < _numberOfWords; i++) {
          tmp[_offsetWords + i] = toEpics.convert(this->accessData(i));
        }
        result = ca_array_put(pv->dbfType, pv->nElems, pv->chid, tmp);
      }

      if(result != ECA_NORMAL) {
        std::cerr << "Failed to to write pv: " << _info._caName << std::endl;
        return false;
      }
      result = ca_pend_io(default_ca_timeout);
      if(result == ECA_TIMEOUT) {
        std::cerr << "Timeout while writing pv: " << _info._caName << std::endl;
        return false;
      }
    }
    else {
      throw ChimeraTK::runtime_error(std::string("ChannelAccess not connected in doWriteTransfer when writing: ") +
          _info._name + "(" + pv->name + ")");
    }
    return true;
  }

  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  EpicsBackendRegisterAccessor<EpicsBaseType, EpicsType, CTKType>::~EpicsBackendRegisterAccessor() {
    ChannelManager::getInstance().removeAccessor(_info._caName, this);
  }

} // namespace ChimeraTK
