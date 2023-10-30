// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * testUnifiedBackendTest.C
 *
 *  Created on: Oct 17, 2023
 *      Author: Klaus Zenker (HZDR)
 */

#include "DummyIOC.h"

#include <cadef.h>
#include <iostream>

#define BOOST_TEST_MODULE testUnifiedBackendTest
#include <ChimeraTK/UnifiedBackendTest.h>

#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test_framework;
using namespace ChimeraTK;

class IOCLauncher {
 public:
  IOCLauncher() {
    helper = &_helper;
    _helper.start();
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  IOCHelper _helper;
  static IOCHelper* helper;
};

IOCHelper* IOCLauncher::helper;

struct AllRegisterDefaults {
  virtual ~AllRegisterDefaults() {}
  virtual bool isWriteable() { return true; }
  bool isReadable() { return true; }
  AccessModeFlags supportedFlags() { return {AccessMode::wait_for_new_data}; }
  size_t nChannels() { return 1; }
  size_t writeQueueLength() { return std::numeric_limits<size_t>::max(); }
  size_t nRuntimeErrorCases() { return 2; }
  typedef std::nullptr_t rawUserType;
  typedef int32_t minimumUserType;

  static constexpr auto capabilities = TestCapabilities<>()
                                           .disableForceDataLossWrite()
                                           .disableAsyncReadInconsistency()
                                           .disableSwitchReadOnly()
                                           .disableSwitchWriteOnly();

  void setForceRuntimeError(bool enable, size_t test) {
    switch(test) {
      case 0:
        if(enable) {
          IOCLauncher::helper->stop();
          // check if the server is really off
          //          if(!IOCLauncher::helper->checkConnection(IOCState::Off)) {
          //            throw std::runtime_error("Failed to force runtime error.");
          //          }
        }
        else {
          // check if server is running is done by the method itself.
          IOCLauncher::helper->start();
        }
        break;
      case 1:
        IOCLauncher::helper->pause(enable);
        break;
      default:
        throw std::runtime_error("Unknown error case.");
    }

    IOCLauncher::helper->checkState(enable == true ? CA_OP_CONN_DOWN : CA_OP_CONN_UP);
  }
};

template<typename T>
struct identity {
  typedef T type;
};

template<typename T>
struct ScalarDefaults : AllRegisterDefaults {
  using AllRegisterDefaults::AllRegisterDefaults;
  size_t nElementsPerChannel() { return 1; }
  virtual std::string path() = 0;
  virtual std::string pvName() = 0;

  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue() {
    return generateValue(identity<UserType>());
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    auto result = IOCLauncher::helper->getValue(pvName());
    auto d = ChimeraTK::userTypeToUserType<UserType, std::string>(IOCLauncher::helper->getValue(pvName()));
    std::stringstream ss;
    ss << "Received remote value: " << result.c_str() << " which is converted to : "
       << ChimeraTK::userTypeToUserType<UserType, std::string>(IOCLauncher::helper->getValue(pvName()))
       << " and d is set to: " << d << std::endl;
    std::cout << ss.str();
    return {{d}};
  }

  void setRemoteValue() {
    std::vector<std::string> value;
    value.push_back(generateValue<std::string>().at(0).at(0));
    std::stringstream ss;
    ss << value.at(0);
    std::cout << "Setting value: " << ss.str().c_str() << std::endl;
    IOCLauncher::helper->setValue(pvName(), value);
  }

 private:
  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue(identity<UserType>) {
    UserType increment(3);
    auto currentData = getRemoteValue<UserType>();
    UserType data = currentData.at(0).at(0) + increment;
    return {{data}};
  }

  std::vector<std::vector<std::string>> generateValue(identity<std::string>) {
    int increment(3);
    auto currentData = getRemoteValue<int>();
    int data = currentData.at(0).at(0) + increment;
    return {{std::to_string(data)}};
  }
};

template<>
struct ScalarDefaults<bool> : AllRegisterDefaults {
  using AllRegisterDefaults::AllRegisterDefaults;
  size_t nElementsPerChannel() { return 1; }
  virtual std::string path() = 0;
  virtual std::string pvName() = 0;

  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue() {
    return generateValue(identity<UserType>());
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    auto result = IOCLauncher::helper->getValue(pvName());
    auto d = ChimeraTK::userTypeToUserType<UserType, std::string>(IOCLauncher::helper->getValue(pvName()));
    std::stringstream ss;
    ss << "Received remote value: " << result.c_str() << " which is converted to : "
       << ChimeraTK::userTypeToUserType<UserType, std::string>(IOCLauncher::helper->getValue(pvName()))
       << " and d is set to: " << d << std::endl;
    std::cout << ss.str();
    return {{d}};
  }

  void setRemoteValue() {
    std::vector<std::string> value;
    value.push_back(generateValue<std::string>().at(0).at(0));
    std::stringstream ss;
    ss << value.at(0);
    std::cout << "Setting value: " << ss.str().c_str() << std::endl;
    IOCLauncher::helper->setValue(pvName(), value);
  }

 private:
  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue(identity<UserType>) {
    UserType increment(1);
    auto currentData = getRemoteValue<UserType>();
    if(currentData.at(0).at(0) > 0) {
      UserType data = currentData.at(0).at(0) - increment;
      return {{data}};
    }
    else {
      UserType data = currentData.at(0).at(0) + increment;
      return {{data}};
    }
  }

  std::vector<std::vector<std::string>> generateValue(identity<std::string>) {
    auto currentData = getRemoteValue<int>();
    std::string data;
    if(currentData.at(0).at(0) > 0) {
      data = "0";
    }
    else {
      data = "1";
    }
    return {{data}};
  }
};

struct RegAo : ScalarDefaults<int32_t> {
  std::string path() override { return "ctkTest/ao"; }
  std::string pvName() override { return std::string("ctkTest:ao"); }
  typedef int32_t minimumUserType;
};

struct RegBo : ScalarDefaults<bool> {
  std::string path() override { return "ctkTest/boInt"; }
  std::string pvName() override { return std::string("ctkTest:boInt"); }
  typedef Boolean minimumUserType;
};

// use test fixture suite to have access to the fixture class members
BOOST_FIXTURE_TEST_SUITE(s, IOCLauncher)
BOOST_AUTO_TEST_CASE(unifiedBackendTest) {
  //  auto ubt = ChimeraTK::UnifiedBackendTest<>().addRegister<RegAo>().addRegister<RegSomeBool>();
  auto ubt = ChimeraTK::UnifiedBackendTest<>().addRegister<RegBo>();
  ubt.runTests("(epics:?map=test.map)");
}

BOOST_AUTO_TEST_SUITE_END()
