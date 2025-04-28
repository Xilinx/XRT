# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

# Support building XRT or its components with local build of Boost libraries. 
# In particular the script runtime_src/tools/script/boost.sh downloads
# and builds static Boost libraries compiled with fPIC so that they
# can be used to resolve symbols in XRT dynamic libraries.
if (DEFINED ENV{XRT_BOOST_INSTALL})
  set(XRT_BOOST_INSTALL $ENV{XRT_BOOST_INSTALL})
  set(Boost_USE_STATIC_LIBS ON)
  if(CMAKE_VERSION VERSION_GREATER "3.29")
    find_package(Boost CONFIG
      HINTS $ENV{XRT_BOOST_INSTALL}
      REQUIRED COMPONENTS system filesystem program_options)
  else(CMAKE_VERSION VERSION_GREATER "3.29")
    find_package(Boost
      HINTS $ENV{XRT_BOOST_INSTALL}
      REQUIRED COMPONENTS system filesystem program_options)
  endif(CMAKE_VERSION VERSION_GREATER "3.29")

  # A bug in FindBoost maybe?  Doesn't set Boost_LIBRARY_DIRS when
  # Boost install has only static libraries. For static tool linking
  # this variable is needed in order for linker to locate the static
  # libraries.  Another bug in FindBoost fails to find static
  # libraries when shared ones are present too.
  if (Boost_FOUND AND "${Boost_LIBRARY_DIRS}" STREQUAL "")
    set (Boost_LIBRARY_DIRS $ENV{XRT_BOOST_INSTALL}/lib)
  endif()

else()
  if(CMAKE_VERSION VERSION_GREATER "3.29")
    find_package(Boost CONFIG
    REQUIRED COMPONENTS system filesystem program_options)
  else(CMAKE_VERSION VERSION_GREATER "3.29")
    find_package(Boost
      REQUIRED COMPONENTS system filesystem program_options)
  endif(CMAKE_VERSION VERSION_GREATER "3.29")
endif()
set(Boost_USE_MULTITHREADED ON)             # Multi-threaded libraries

# Some later versions of boost spews warnings form property_tree
# Default embedded boost is 1.74.0 which does spew warnings so
# making this defined global
add_compile_options("-DBOOST_BIND_GLOBAL_PLACEHOLDERS")

# Boost_VERSION_STRING is not working properly, use our own macro
set(XRT_BOOST_VERSION ${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION})
