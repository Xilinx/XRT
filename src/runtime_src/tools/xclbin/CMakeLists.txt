include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../../driver/include
  )

file(GLOB XCLBINCAT_FILES
  "xclbincat.cxx"
  "xclbincat1.cxx"
  "xclbindata.cxx"
  "xclbinutil.cxx"
  )

SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
SET(BUILD_SHARED_LIBS OFF)
SET(CMAKE_EXE_LINKER_FLAGS "-static")

set(XCLBINCAT_SRC ${XCLBINCAT_FILES})
add_executable(xclbincat ${XCLBINCAT_SRC})

file(GLOB XCLBINSPLIT_FILES
  "xclbinsplit.cxx"
  "xclbinsplit1.cxx"
  "xclbindata.cxx"
  "xclbinutil.cxx"
  )

SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
SET(BUILD_SHARED_LIBS OFF)
SET(CMAKE_EXE_LINKER_FLAGS "-static")

set(XCLBINSPLIT_SRC ${XCLBINSPLIT_FILES})
add_executable(xclbinsplit ${XCLBINSPLIT_SRC})

install (TARGETS xclbincat xclbinsplit RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
