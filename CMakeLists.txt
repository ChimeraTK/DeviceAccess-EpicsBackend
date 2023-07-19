project(ChimeraTK-DeviceAccess-EPICS-Backend)

cmake_minimum_required(VERSION 2.8.13)

set(${PROJECT_NAME}_MAJOR_VERSION 00)
set(${PROJECT_NAME}_MINOR_VERSION 01)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)

include(cmake/add_dependency.cmake)
include(cmake/set_default_flags.cmake)

add_dependency(ChimeraTK-DeviceAccess 03.00 REQUIRED)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake4epics/cmake/Modules)

include(FindPkgConfig)

if(DEFINED EPICS_DIR)
  set(ENV{PKG_CONFIG_PATH} $ENV{PKG_CONFIG_PATH}:${EPICS_DIR}/pkgconfig)
endif()
message("Using PKG_CONFIG_PATH=$ENV{PKG_CONFIG_PATH}")
pkg_check_modules(EPICS REQUIRED epics-base-linux-x86_64)
pkg_get_variable(EPICS_LIBS epics-base-linux-x86_64 EPICS_BASE_IOC_LIBS)

#find_package(EPICS COMPONENTS ca REQUIRED)

include_directories( include ${EPICS_INCLUDE_DIRS})

aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)

add_library(${PROJECT_NAME} SHARED ${library_sources} )
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION ${${PROJECT_NAME}_SOVERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_FLAGS "-Wl,--enable-new-dtags")
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${ChimeraTK-DeviceAccess_CXX_FLAGS}")
target_link_libraries(${PROJECT_NAME} ${ChimeraTK-DeviceAccess_LIBRARIES} ${EPICS_LIBS} ${EPICS_LDFLAGS})


add_executable(testRead test/read.C)
target_link_libraries(testRead ${EPICS_LIBS} ${EPICS_LDFLAGS})

add_executable(testReadAsync test/readAsync.C)
target_link_libraries(testReadAsync ${EPICS_LIBS} ${EPICS_LDFLAGS})

#Install the library and the executables
install(TARGETS ${PROJECT_NAME}
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
)

#all include files go into include/PROJECT_NAME
#The exclusion of ${PROJECT_NAME} prevents the recursive installation of the files just being installed.
#The original headers are in include/*.h, the installed ones in include/PROJECT_NAME/*.h.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include/${PROJECT_NAME}
   FILES_MATCHING PATTERN "*.h"
   PATTERN "${PROJECT_NAME}" EXCLUDE
)

#Create the config files by filling the correct variables into the template (*.cmake.in).
#All variables @VARIABLE@ are replaced with the current values, but they have to be set first....
#Only the reference header is needed. The other variables are for dependencies of this library,
#but there are none.
set(${PROJECT_NAME}_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include ${Boost_INCLUDE_DIRS}")
set(${PROJECT_NAME}_LIBRARIES "${PROJECT_NAME} ${ChimeraTK-DeviceAccess_LIBRARIES}")
set(${PROJECT_NAME}_LIBRARY_DIRS "${CMAKE_INSTALL_PREFIX}/lib ${ChimeraTK-DeviceAccess_LIBRARY_DIRS}")
set(${PROJECT_NAME}_CPPFLAGS "${ChimeraTK-DeviceAccess_CXX_FLAGS}")
set(${PROJECT_NAME}_LINKER_FLAGS "-Wl,-rpath=${CMAKE_INSTALL_PREFIX}/lib,--enable-new-dtags")
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)  
