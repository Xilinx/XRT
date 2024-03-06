# Top-level CMakeLists.txt

cmake_minimum_required(VERSION 3.10)
project(aie-pdi-transform)

# Set the C standard to C99
set(CMAKE_C_STANDARD 99)

# Add a preprocessor definition
add_definitions(-DXCDO_DEBUG_STUB)

# Add subdirectories for src/
add_subdirectory(src)

set(TRANSFORM_NAME "transform_static")

add_executable( ${TRANSFORM_NAME}
    cdo-main-static.c
)

# Link the executable with the static library
target_link_libraries(${TRANSFORM_NAME} PRIVATE transformcdo)

install (TARGETS ${TRANSFORM_NAME} RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR})
