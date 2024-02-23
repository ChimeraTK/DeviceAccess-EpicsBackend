project(ChimeraTK-DeviceAccess-EPICS-Backend)

cmake_minimum_required(VERSION 3.16)

set(${PROJECT_NAME}_MAJOR_VERSION 00)
set(${PROJECT_NAME}_MINOR_VERSION 02)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)

option(BUILD_TESTS "Build tests." OFF)

include(cmake/set_default_flags.cmake)
include(cmake/set_default_build_to_release.cmake)
include(cmake/enable_code_style_check.cmake)

find_package(ChimeraTK-DeviceAccess 03.14 REQUIRED)

include(FindPkgConfig)

if(DEFINED EPICS_DIR)
  set(ENV{PKG_CONFIG_PATH} $ENV{PKG_CONFIG_PATH}:${EPICS_DIR}/base/lib/pkgconfig)
endif()

message("Using PKG_CONFIG_PATH=$ENV{PKG_CONFIG_PATH}")
pkg_check_modules(EPICS REQUIRED epics-base-linux-x86_64)
#pkg_get_variable(EPICS_LIBS epics-base-linux-x86_64 EPICS_BASE_IOC_LIBS)
pkg_get_variable(EPICS_LIBS epics-base-linux-x86_64 EPICS_BASE_HOST_LIBS)
pkg_get_variable(EPICS_ARCH epics-base-linux-x86_64 ARCH)
pkg_get_variable(EPICS_PATH epics-base-linux-x86_64 FINAL_LOCATION)

list(APPEND CMAKE_INSTALL_RPATH ${EPICS_LIBRARY_DIRS})
include_directories(include ${EPICS_INCLUDE_DIRS})

aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)

add_library(${PROJECT_NAME} SHARED ${library_sources} )
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION} SOVERSION ${${PROJECT_NAME}_SOVERSION})
target_link_libraries(${PROJECT_NAME} PUBLIC ChimeraTK::ChimeraTK-DeviceAccess
                                      PRIVATE ${EPICS_LIBS}
                                      PRIVATE ${EPICS_LDFLAGS})
if(BUILD_TESTS)
  add_subdirectory(test)
endif(BUILD_TESTS)

# Enable documentation
include(cmake/enable_doxygen_documentation.cmake)

# Install the library and the executables
# this defines architecture-dependent ${CMAKE_INSTALL_LIBDIR}
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# we support our cmake EXPORTS as imported targets
set(PROVIDES_EXPORTED_TARGETS 1)
# we need the public dependencies so create_cmake_config_files can find them as implicit dependencies
list(APPEND ${PROJECT_NAME}_PUBLIC_DEPENDENCIES "ChimeraTK-DeviceAccess")
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)