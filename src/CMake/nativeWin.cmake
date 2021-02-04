# This cmake file is for native build. Host and target processor are the same.
# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_INSTALL_DIR
# XRT_VERSION_MAJOR
# XRT_VERSION_MINOR
# XRT_VERSION_PATCH

# install under c:/xrt
set (CMAKE_INSTALL_PREFIX "${PROJECT_BINARY_DIR}/xilinx")

# --- Git ---
find_package(Git)

IF(GIT_FOUND)
  MESSAGE(STATUS "Looking for GIT - found at ${GIT_EXECUTABLE}")
ELSE(GIT_FOUND)
  MESSAGE(FATAL_ERROR "Looking for GIT - not found")
endif(GIT_FOUND)

# --- Boost ---
#set(Boost_DEBUG 1)

INCLUDE (FindBoost)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost
  REQUIRED COMPONENTS system filesystem)

# Boost_VERSION_STRING is not working properly, use our own macro
set(XRT_BOOST_VERSION ${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION})

include_directories(${Boost_INCLUDE_DIRS})
add_compile_options("-DBOOST_LOCALE_HIDE_AUTO_PTR")

INCLUDE (FindGTest)

# --- XRT Variables ---
set (XRT_INSTALL_DIR "xrt")
set (XRT_INSTALL_BIN_DIR       "${XRT_INSTALL_DIR}/bin")
set (XRT_INSTALL_UNWRAPPED_DIR "${XRT_INSTALL_BIN_DIR}/unwrapped")
set (XRT_INSTALL_INCLUDE_DIR   "${XRT_INSTALL_DIR}/include")
set (XRT_INSTALL_LIB_DIR       "${XRT_INSTALL_DIR}/lib")

# --- Release: eula ---
file(GLOB XRT_EULA
  "license/*.txt"
  )
#install (FILES ${XRT_EULA} DESTINATION ${XRT_INSTALL_DIR}/license)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE DESTINATION ${XRT_INSTALL_DIR}/license)
message("-- XRT EA eula files  ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE")

# -- CPack
include (CMake/cpackWin.cmake)

# --- Create Version header and JSON file ---
include (CMake/version.cmake)

message ("------------ xrt install dir: ${XRT_INSTALL_DIR}")
add_subdirectory(runtime_src)

# --- Find Package Support ---
include (CMake/findpackage.cmake)
