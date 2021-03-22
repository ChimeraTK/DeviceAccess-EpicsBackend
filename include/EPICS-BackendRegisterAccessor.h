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

#include "EPICS_types.h"
#include "EPICS-Backend.h"
#include <cadef.h>




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
      }
    }
  };

  template <typename SourceType>
  class EpicsRangeCheckingDataConverter<std::string,SourceType>{
  public:
    std::string convert(SourceType& x){
      return std::to_string(x);
    }
  };

  template <typename DestType>
  class EpicsRangeCheckingDataConverter<DestType, std::string>{
  public:
    DestType convert(std::string&){
      throw std::logic_error("Conversion from string is not allowed.");

    }
  };

  class EpicsBackendRegisterAccessorBase{
  public:
    EpicsBackendRegisterAccessorBase(boost::shared_ptr<EpicsBackend> backend, EpicsBackendRegisterInfo* info, size_t numberOfWords, size_t wordOffsetInRegister):
      _info(info), _backend(backend), _numberOfWords(numberOfWords), _offsetWords(wordOffsetInRegister){
    }
    EpicsBackendRegisterInfo* _info;
    cppext::future_queue<void*> _notifications;
    boost::shared_ptr<EpicsBackend> _backend;
    size_t _numberOfWords; ///< Requested array length. Could be smaller than what is available on the server.
    size_t _offsetWords; ///< Requested offset for arrays.
    ChimeraTK::VersionNumber _currentVersion;
  };

  template<typename EpicsType, typename CTKType>
  class EpicsBackendRegisterAccessor : public EpicsBackendRegisterAccessorBase, public NDRegisterAccessor<CTKType> {
  public:
    ~EpicsBackendRegisterAccessor(){};

    void doReadTransferSynchronously() override;

    void doPreRead(TransferType) override{
      if(!_backend->isOpen())
        throw ChimeraTK::logic_error("Read operation not allowed while device is closed.");
    }

    void doPreWrite(TransferType, VersionNumber) override{
      if(!_backend->isOpen())
        throw ChimeraTK::logic_error("Write operation not allowed while device is closed.");
    }

    bool doWriteTransfer(VersionNumber /*versionNumber*/={}) override {return true;};

    void doPostRead(TransferType, bool hasNewData) override;

    bool isReadOnly() const override {
     return _info->_isReadonly;
    }

    bool isReadable() const override {
     return true;
    }

    bool isWriteable() const override {
     return !_info->_isReadonly;
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

    EpicsRangeCheckingDataConverter<CTKType, EpicsType> toCTK;

    EpicsBackendRegisterAccessor(const RegisterPath &path, boost::shared_ptr<DeviceBackend> backend, EpicsBackendRegisterInfo* registerInfo,
        AccessModeFlags flags, size_t numberOfWords, size_t wordOffsetInRegister);
  };

  template<typename EpicsType, typename CTKType>
  EpicsBackendRegisterAccessor<EpicsType, CTKType>::EpicsBackendRegisterAccessor(const RegisterPath &path, boost::shared_ptr<DeviceBackend> backend, EpicsBackendRegisterInfo* registerInfo,
          AccessModeFlags flags, size_t numberOfWords, size_t wordOffsetInRegister):
          EpicsBackendRegisterAccessorBase(boost::dynamic_pointer_cast<EpicsBackend>(backend), registerInfo, numberOfWords, wordOffsetInRegister),
          NDRegisterAccessor<CTKType>(path, flags){
    NDRegisterAccessor<CTKType>::buffer_2D.resize(1);
  }

  template<typename EpicsType, typename CTKType>
  void EpicsBackendRegisterAccessor<EpicsType, CTKType>::doReadTransferSynchronously(){
    if(ca_state(_info->_pv->chid) == cs_conn){
      _info->_pv->value = calloc(1, dbr_size_n(_info->_pv->dbrType, _info->_pv->nElems));
      auto result = ca_array_get(_info->_pv->dbrType, _info->_pv->nElems, _info->_pv->chid, _info->_pv->value);
      if(result != ECA_NORMAL){
        throw ChimeraTK::runtime_error(std::string("Failed to to read pv: ") + _info->_pv->name);
      }
      result = ca_pend_io(_backend->_caTimeout);
      if(result != ECA_TIMEOUT){
        throw ChimeraTK::runtime_error(std::string("Read operation timed out for pv: ") + _info->_pv->name);
      }


    } else {
      std::cerr << "Disconnected when filling catalogue entry for " << _info->_name << "(" << _info->_pv->name << ")" << std::endl;
      return;
    }

  }
  template<typename EpicsType, typename CTKType>
  void EpicsBackendRegisterAccessor<EpicsType, CTKType>::doPostRead(TransferType, bool hasNewData){
    if(!hasNewData) return;
    EpicsType* tmp = (EpicsType*)dbr_value_ptr(_info->_pv->value, _info->_pv->dbrType);
    for(size_t i = 0; i < _numberOfWords; i++){
      EpicsType value = tmp[_offsetWords+i];
      // Fill the NDRegisterAccessor buffer
      this->accessData(i) = toCTK.convert(value);
    }
    _currentVersion = ChimeraTK::VersionNumber();
    TransferElement::_versionNumber = _currentVersion;
  }

}



#endif /* INCLUDE_EPICS_BACKENDREGISTERACCESSOR_H_ */
