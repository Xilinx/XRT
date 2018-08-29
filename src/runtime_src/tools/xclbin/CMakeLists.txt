include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../../driver/include
  )

find_package(Boost REQUIRED COMPONENTS system filesystem)

# -----------------------------------------------------------------------------

file(GLOB XCLBINCAT_FILES
  "xclbincat.cxx"
  "xclbincat1.cxx"
  "xclbindata.cxx"
  "xclbinutil.cxx"
  )

set(XCLBINCAT_SRC ${XCLBINCAT_FILES})
add_executable(xclbincat ${XCLBINCAT_SRC})

target_link_libraries(xclbincat -static ${Boost_LIBRARIES})

# -----------------------------------------------------------------------------

file(GLOB XCLBINSPLIT_FILES
  "xclbinsplit.cxx"
  "xclbinsplit1.cxx"
  "xclbindata.cxx"
  "xclbinutil.cxx"
  )

set(XCLBINSPLIT_SRC ${XCLBINSPLIT_FILES})
add_executable(xclbinsplit ${XCLBINSPLIT_SRC})

target_link_libraries(xclbinsplit -static ${Boost_LIBRARIES})

# -----------------------------------------------------------------------------

install (TARGETS xclbincat xclbinsplit RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
