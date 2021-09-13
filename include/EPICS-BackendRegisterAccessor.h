/*
 * EPICS-BackendRegisterAccessor.h
 *
 *  Created on: Jan 22, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#ifndef INCLUDE_EPICS_BACKENDREGISTERACCESSOR_H_
#define INCLUDE_EPICS_BACKENDREGISTERACCESSOR_H_

#include <ChimeraTK/NDRegisterAccessor.h>
#include <ChimeraTK/RegisterPath.h>
#include <ChimeraTK/AccessMode.h>

#include <boost/enable_shared_from_this.hpp>

#include "string.h"

#include "EPICS_types.h"
#include "EPICS-Backend.h"
#include <cadef.h>

#include "EPICSChannelManager.h"
#include "EPICSVersionMapper.h"

namespace ChimeraTK{

  template <typename DestType, typename SourceType>
  class EpicsRangeCheckingDataConverter{
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
    DestType convert(SourceType& x){
      try{
        return converter::convert(x);
      } catch (boost::numeric::positive_overflow&) {
        return std::numeric_limits<DestType>::max();
      } catch (boost::numeric::negative_overflow&) {
        return std::numeric_limits<DestType>::min();
      } catch(...){
        throw ChimeraTK::runtime_error("Data conversion failed for unknown reason.");
      }
    }
  };

  template<typename DestType>
  class EpicsRangeCheckingDataConverter<DestType, Void> {
   public:
    DestType convert([[maybe_unused]] Void& x) { return DestType(); } // default constructed value
  };

  //partial specialization of conversion to void
  template<typename SourceType>
  class EpicsRangeCheckingDataConverter<Void, SourceType> {
   public:
    Void convert([[maybe_unused]] SourceType& x) { return Void(); }
  };

  template <typename SourceType>
  class EpicsRangeCheckingDataConverter<std::string,SourceType>{
  public:
    std::string convert(SourceType& x){
      return std::to_string(x);
    }
  };

  template <>
  class EpicsRangeCheckingDataConverter<std::string,Void>{
  public:
    std::string convert(Void& x){
      return std::string("void");
    }
  };

  template <typename DestType>
  class EpicsRangeCheckingDataConverter<DestType, std::string>{
  public:
    DestType convert(std::string&){
      throw std::logic_error("Conversion from string is not allowed.");

    }
  };

  template <>
  class EpicsRangeCheckingDataConverter<Void, std::string>{
  public:
    Void convert(std::string&){
      return Void();
    }
  };

  class EpicsBackendRegisterAccessorBase{
  public:
    EpicsBackendRegisterAccessorBase(boost::shared_ptr<EpicsBackend> backend, EpicsBackendRegisterInfo* info, size_t numberOfWords, size_t wordOffsetInRegister):
      _info(info), _backend(backend), _numberOfWords(numberOfWords), _offsetWords(wordOffsetInRegister){
    }
    EpicsBackendRegisterInfo* _info;
    cppext::future_queue<evargs> _notifications;
    boost::shared_ptr<EpicsBackend> _backend;
    size_t _numberOfWords; ///< Requested array length. Could be smaller than what is available on the server.
    size_t _offsetWords; ///< Requested offset for arrays.
    bool _isPartial{false};
    ChimeraTK::VersionNumber _currentVersion;
    evid* _subscriptionId{nullptr}; ///< Id used for subscriptions

    static void handleEvent(evargs args){
      auto base = reinterpret_cast<EpicsBackendRegisterAccessorBase*>(args.usr);
//      std::cout << "Handling update of ca " << base->_info->_caName << std::endl;
      if(base->_backend->_asyncReadActivated)
        base->_notifications.push_overwrite(args);
    }
  };

  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  class EpicsBackendRegisterAccessor : public EpicsBackendRegisterAccessorBase, public NDRegisterAccessor<CTKType> {
  public:
    ~EpicsBackendRegisterAccessor();

    void doReadTransferSynchronously() override;

    void doPreRead(TransferType) override{
      if(!_backend->isOpen())
        throw ChimeraTK::logic_error("Read operation not allowed while device is closed.");
    }

    void doPreWrite(TransferType, VersionNumber) override{
      if(!_backend->isOpen())
        throw ChimeraTK::logic_error("Write operation not allowed while device is closed.");
    }

    bool doWriteTransfer(VersionNumber /*versionNumber*/={}) override;

    void doPostRead(TransferType, bool hasNewData) override;

    bool isReadOnly() const override {
     return (_info->_isReadable && !_info->_isWritable);
    }

    bool isReadable() const override {
     return true;
    }

    bool isWriteable() const override {
     return _info->_isWritable;
    }

    void interrupt() override { this->interrupt_impl(this->_notifications);}

    using TransferElement::_readQueue;

    std::vector< boost::shared_ptr<TransferElement> > getHardwareAccessingElements() override {
     return { boost::enable_shared_from_this<TransferElement>::shared_from_this() };
    }

    std::list<boost::shared_ptr<TransferElement> > getInternalElements() override {
     return {};
    }

    void replaceTransferElement(boost::shared_ptr<TransferElement> /*newElement*/) override {} // LCOV_EXCL_LINE

    friend class EpicsBackend;

    EpicsRangeCheckingDataConverter<CTKType, EpicsBaseType> toCTK;
    EpicsRangeCheckingDataConverter<EpicsBaseType,CTKType> toEpics;

    EpicsBackendRegisterAccessor(const RegisterPath &path, boost::shared_ptr<DeviceBackend> backend, EpicsBackendRegisterInfo* registerInfo,
        AccessModeFlags flags, size_t numberOfWords, size_t wordOffsetInRegister);
  };

  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  EpicsBackendRegisterAccessor<EpicsBaseType, EpicsType, CTKType>::EpicsBackendRegisterAccessor(const RegisterPath &path, boost::shared_ptr<DeviceBackend> backend, EpicsBackendRegisterInfo* registerInfo,
          AccessModeFlags flags, size_t numberOfWords, size_t wordOffsetInRegister):
          EpicsBackendRegisterAccessorBase(boost::dynamic_pointer_cast<EpicsBackend>(backend), registerInfo, numberOfWords, wordOffsetInRegister),
          NDRegisterAccessor<CTKType>(path, flags){
    if(flags.has(AccessMode::raw))
      throw ChimeraTK::logic_error("Raw access mode is not supported.");
    NDRegisterAccessor<CTKType>::buffer_2D.resize(1);
    this->accessChannel(0).resize(numberOfWords);
    // allocate value
    _info->_pv->value = calloc(1, dbr_size_n(_info->_pv->dbrType, _info->_pv->nElems));
    /* \ToDo: Use callback to get informed once the connection state changes.
              Pass backend pointer and not pv as usr data and in the handler
              use backend functions to handle connection change.
    */
    if(flags.has(AccessMode::wait_for_new_data)){
      _notifications = cppext::future_queue<evargs>(3);
      _readQueue = _notifications.then<void>([this](evargs &args){memcpy(_info->_pv->value, args.dbr, dbr_size_n(args.type, args.count));});
      _subscriptionId = new evid();
      ca_create_subscription(_info->_pv->dbrType,
          _info->_pv->nElems,
          _info->_pv->chid,
          DBE_VALUE,
          &EpicsBackendRegisterAccessorBase::handleEvent,
          static_cast<EpicsBackendRegisterAccessorBase*>(this),
          _subscriptionId);
      {
      ChannelManager::getInstance().addAccessor(_info->_pv->chid, this);
      }
      ca_flush_io();
    }
    if(_info->_pv->nElems != numberOfWords)
      _isPartial = true;
    NDRegisterAccessor<CTKType>::_exceptionBackend = backend;
  }

  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  void EpicsBackendRegisterAccessor<EpicsBaseType, EpicsType, CTKType>::doReadTransferSynchronously(){
    if(!_backend->isFunctional()){
      throw ChimeraTK::runtime_error(std::string("Exception reported by another accessor."));
    }
    if(ca_state(_info->_pv->chid) == cs_conn){
      auto result = ca_array_get(_info->_pv->dbrType, _info->_pv->nElems, _info->_pv->chid, _info->_pv->value);
      if(result != ECA_NORMAL){
        throw ChimeraTK::runtime_error(std::string("Failed to to read pv: ") + _info->_pv->name);
      }
      result = ca_pend_io(_backend->_caTimeout);
      if(result == ECA_TIMEOUT){
        throw ChimeraTK::runtime_error(std::string("Read operation timed out for pv: ") + _info->_pv->name);
      }


    } else {
      std::cerr << "Disconnected when filling catalogue entry for " << _info->_name << "(" << _info->_pv->name << ")" << std::endl;
      return;
    }

  }
  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  void EpicsBackendRegisterAccessor<EpicsBaseType, EpicsType, CTKType>::doPostRead(TransferType, bool hasNewData){
    if(!hasNewData) return;
    EpicsBaseType* tmp = (EpicsBaseType*)dbr_value_ptr(_info->_pv->value, _info->_pv->dbrType);
    for(size_t i = 0; i < _numberOfWords; i++){
      EpicsBaseType value = tmp[_offsetWords+i];
      // Fill the NDRegisterAccessor buffer
      this->accessData(i) = toCTK.convert(value);
    }
    EpicsType* tp = (EpicsType*)_info->_pv->value;
    _currentVersion = EPICS::VersionMapper::getInstance().getVersion(tp[0].stamp);
    TransferElement::_versionNumber = _currentVersion;
  }

  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  bool EpicsBackendRegisterAccessor<EpicsBaseType, EpicsType, CTKType>::doWriteTransfer(VersionNumber /*versionNumber*/){
    if(!_backend->isFunctional()) {
      throw ChimeraTK::runtime_error(std::string("Exception reported by another accessor."));
    }
    if(_isPartial)
      EpicsBackendRegisterAccessor<EpicsBaseType, EpicsType, CTKType>::doReadTransferSynchronously();

    EpicsBaseType* tmp = (EpicsBaseType*)dbr_value_ptr(_info->_pv->value, _info->_pv->dbrType);
    for(size_t i = 0; i < _numberOfWords; i++){
      tmp[_offsetWords + i] = toEpics.convert(this->accessData(i));
    }
    auto result = ca_array_put(_info->_pv->dbfType, _info->_pv->nElems,_info->_pv->chid,tmp);
    if(result != ECA_NORMAL){
      std::cerr << "Failed to to write pv: " << _info->_caName << std::endl;
      return false;
    }
    result = ca_pend_io(_backend->_caTimeout);
    if(result == ECA_TIMEOUT){
      std::cerr << "Timeout while writing pv: " << _info->_caName << std::endl;
      return false;
    }
    return true;
  }

  template<typename EpicsBaseType, typename EpicsType, typename CTKType>
  EpicsBackendRegisterAccessor<EpicsBaseType, EpicsType, CTKType>::~EpicsBackendRegisterAccessor(){
    if(_subscriptionId){
      ChannelManager::getInstance().removeAccessor(_info->_pv->chid, this);
    }
  }

}



#endif /* INCLUDE_EPICS_BACKENDREGISTERACCESSOR_H_ */
