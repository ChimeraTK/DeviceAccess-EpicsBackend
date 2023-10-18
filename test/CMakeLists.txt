add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/IOC
                   COMMAND mkdir ${CMAKE_CURRENT_BINARY_DIR}/IOC)
                   
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp/Db/
                   COMMAND ${EPICS_PATH}/bin/${EPICS_ARCH}/makeBaseApp.pl -t ioc -u ctkTest ctkTest
                   COMMAND ${EPICS_PATH}/bin/${EPICS_ARCH}/makeBaseApp.pl -i -t ioc ctkTest
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/IOC
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/IOC)
                   
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp/Db/ctkTest.db
                   COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/test.db ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp/Db/ctkTest.db
                   COMMAND sed -i -e 's/\#DB/DB/g' ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp/Db/Makefile
                   COMMAND sed -i -e 's/xxx.db/ctkTest.db/g' ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp/Db/Makefile
                   DEPENDS  ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp/Db/
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/IOC)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/IOC/bin
                   COMMAND make
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/IOC/ctkTestApp/Db/ctkTest.db
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/IOC)                   
                   
# binary used during the developement of the backend - they require the EPICS example IOC to be running
add_executable(testRead read.C)
target_link_libraries(testRead ${EPICS_LIBS} ${EPICS_LDFLAGS})

# binary used during the developement of the backend - they require the EPICS example IOC to be running
add_executable(testReadAsync readAsync.C ${CMAKE_CURRENT_BINARY_DIR}/IOC/bin)
target_link_libraries(testReadAsync ${EPICS_LIBS} ${EPICS_LDFLAGS})

configure_file(testIOC.C.in testIOC.C)
configure_file(DummyIOC/DummyIOC.h.in DummyIOC.h)

add_executable(testIOC ${CMAKE_CURRENT_BINARY_DIR}/testIOC.C)
target_link_libraries(testIOC pthread)

FILE(COPY test.map DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_executable(testUnifiedBackendTest testUnifiedBackendTest.C)
set_target_properties(testUnifiedBackendTest PROPERTIES INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(testUnifiedBackendTest PUBLIC ChimeraTK::ChimeraTK-DeviceAccess
                                             PRIVATE ${PROJECT_NAME}
                                             PRIVATE ${EPICS_LIBS} 
                                             PRIVATE ${EPICS_LDFLAGS})
  set_target_properties(testUnifiedBackendTest PROPERTIES LINK_FLAGS "-Wl,--no-as-needed")                                             