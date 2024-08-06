cmake_minimum_required(VERSION 3.16)

project(ChimeraTK-DeviceAccess-EPICS-Backend)

set(${PROJECT_NAME}_MAJOR_VERSION 01)
set(${PROJECT_NAME}_MINOR_VERSION 00)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)

include(cmake/set_default_flags.cmake)
include(cmake/set_default_build_to_release.cmake)
include(cmake/enable_code_style_check.cmake)

find_package(ChimeraTK-DeviceAccess 03.14 REQUIRED)

find_package(ChimeraTK-EPICS REQUIRED)

get_target_property(EPICS_BASE ChimeraTK::EPICS INTERFACE_EPICS_BASE)
get_target_property(EPICS_ARCH ChimeraTK::EPICS INTERFACE_EPICS_ARCH)

list(APPEND CMAKE_INSTALL_RPATH ${EPICS_LIBRARY_DIRS})
include_directories(include ${EPICS_INCLUDE_DIRS})

aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)

add_library(${PROJECT_NAME} SHARED ${library_sources})
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION} SOVERSION ${${PROJECT_NAME}_SOVERSION})
target_link_libraries(${PROJECT_NAME} PUBLIC ChimeraTK::ChimeraTK-DeviceAccess PRIVATE ChimeraTK::EPICS)

FIND_PACKAGE(Boost 1.83 COMPONENTS unit_test_framework)

if(Boost_FOUND)
  option(BUILD_TESTS "Build tests." ON)

  if(BUILD_TESTS)
    add_subdirectory(test)
  endif(BUILD_TESTS)
else()
  message(WARNING "Tests will not be built since no suitable version of BOOST unit_test_framework has been found!")
endif()

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
