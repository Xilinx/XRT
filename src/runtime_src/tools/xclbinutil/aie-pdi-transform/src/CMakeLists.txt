# CMakeLists.txt in src/

add_compile_options(-Wextra -fvisibility=default)

# This builds a static library libtransformcdo, which 
# includes all the *.c files in lib/
add_library( transformcdo STATIC
    pdi-parsing.c
    cdo-parsing.c
    pdi-transform.c
    ../lib/cdo_cmd.c
    ../lib/load_pdi.c
    ../lib/pdi_transform.c
)

# Set C flags
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra -fvisibility=default")

target_include_directories(transformcdo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../libinclude
    ${CMAKE_CURRENT_SOURCE_DIR}
)

