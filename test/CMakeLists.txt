find_program(PERL perl REQUIRED)

FIND_PACKAGE(Boost 1.83 COMPONENTS unit_test_framework REQUIRED)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/IOC
  COMMAND mkdir ${CMAKE_CURRENT_BINARY_DIR}/IOC)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp
  COMMAND ${EPICS_BASE}/bin/${EPICS_ARCH}/makeBaseApp.pl -t ioc -u ctkTest ctkTest
  COMMAND ${EPICS_BASE}/bin/${EPICS_ARCH}/makeBaseApp.pl -i -t ioc ctkTest
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/IOC
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/IOC)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp/Db/ctkTest.db
  COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/test.db ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp/Db/ctkTest.db
  COMMAND sed -i -e 's/\#DB/DB/g' ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp/Db/Makefile
  COMMAND sed -i -e 's/xxx.db/ctkTest.db/g' ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp/Db/Makefile
  COMMAND sed -i -e 's/\#dbLoadRecords/dbLoadRecords/g' ${CMAKE_CURRENT_BINARY_DIR}/IOC/iocBoot/iocctkTest/st.cmd
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/IOC)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/IOC/bin
  COMMAND make USR_LDFLAGS=-Wl,--rpath=${EPICS_BASE}/lib/${EPICS_ARCH}
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp/Db/ctkTest.db
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/IOC)

# binary used during the developement of the backend - they require the EPICS example IOC to be running
add_executable(testRead read.C)
target_link_libraries(testRead ChimeraTK::EPICS)

# binary used during the developement of the backend - they require the EPICS example IOC to be running
add_executable(testReadAsync readAsync.C ${CMAKE_CURRENT_BINARY_DIR}/IOC/bin)
target_link_libraries(testReadAsync ChimeraTK::EPICS)

# binary used during the developement of the backend
add_executable(testPauseIOC testPauseIOC.C ${library_sources})
set_target_properties(testPauseIOC PROPERTIES INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(testPauseIOC PROPERTIES COMPILE_FLAGS "-DCHIMERATK_UNITTEST")
set_target_properties(testPauseIOC PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
target_link_libraries(testPauseIOC PUBLIC ChimeraTK::ChimeraTK-DeviceAccess PRIVATE ChimeraTK::EPICS)

configure_file(testIOC.C.in testIOC.C)
configure_file(DummyIOC/DummyIOC.h.in DummyIOC.h)

add_executable(testIOC ${CMAKE_CURRENT_BINARY_DIR}/testIOC.C)
target_link_libraries(testIOC pthread)

FILE(COPY test.map DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_executable(testUnifiedBackendTest testUnifiedBackendTest.C ${library_sources})
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(testUnifiedBackendTest PUBLIC ChimeraTK::ChimeraTK-DeviceAccess PRIVATE ChimeraTK::EPICS)
set_target_properties(testUnifiedBackendTest PROPERTIES LINK_FLAGS "-Wl,--no-as-needed")
set_target_properties(testUnifiedBackendTest PROPERTIES COMPILE_FLAGS "-DCHIMERATK_UNITTEST")
set_target_properties(testUnifiedBackendTest PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
add_test(testUnifiedBackendTest testUnifiedBackendTest)
