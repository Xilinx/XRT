include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/xcl2
  ${XRT_SOURCE_DIR}/include/1_2
  )

pkg_check_modules(XBTEST_GLIB REQUIRED json-glib-1.0 glib-2.0)
include_directories(${XBTEST_GLIB_INCLUDE_DIRS})

add_compile_options("-DPERFORCE_VERSION=0" "-DBUILD_DATE=xyz")
  
file(GLOB XBTEST_SRCS
  "src/*.cpp"
  )

set(XBTEST_JSON
  xbtest_pfm_def_u200.json
  xbtest_pfm_def_u250.json
  xbtest_pfm_def_u280.json
  xbtest_pfm_def_u50.json
  )

install (FILES ${XBTEST_JSON}
  DESTINATION ${XRT_INSTALL_DIR}/xbtest
  COMPONENT xbtest)

add_executable(xbtest ${XBTEST_SRCS})

target_link_libraries(xbtest
  xilinxopencl
  xrt_core
  pthread
  ${XBTEST_GLIB_LIBRARIES}
  )

install(TARGETS xbtest
  RUNTIME
  DESTINATION ${XRT_INSTALL_DIR}/xbtest
  COMPONENT xbtest)



# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/deviceinterface.o -c src/deviceinterface.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/devicemgt.o -c src/devicemgt.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/dmatest.o -c src/dmatest.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/gtmactest.o -c src/gtmactest.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/gttest.o -c src/gttest.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/inputparser.o -c src/inputparser.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/logging.o -c src/logging.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/memorytest.o -c src/memorytest.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/powertest.o -c src/powertest.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/testcase.o -c src/testcase.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/testinterface.o -c src/testinterface.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/verifytest.o -c src/verifytest.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/xbtest.o -c src/xbtest.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/xbtestpfmdefparser.o -c src/xbtestpfmdefparser.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/xbutildumpparser.o -c src/xbutildumpparser.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/xjsonparser.o -c src/xjsonparser.cpp

# g++ -O0 -g -Wall -c -I./include -I./lib/xcl2 -I/opt/xilinx/xrt/include/ -std=c++11 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include   -DBUILD_DATE="Wed Dec 11 21:47:48 UTC 2019" -DPERFORCE_VERSION=0  -o ./output/xcl2.o -c lib/xcl2/xcl2.cpp

# g++  -o ./output/xbtest ./output/deviceinterface.o ./output/devicemgt.o ./output/dmatest.o ./output/gtmactest.o ./output/gttest.o ./output/inputparser.o ./output/logging.o ./output/memorytest.o ./output/powertest.o ./output/testcase.o ./output/testinterface.o ./output/verifytest.o ./output/xbtest.o ./output/xbtestpfmdefparser.o ./output/xbutildumpparser.o ./output/xjsonparser.o ./output/xcl2.o -lxilinxopencl -lxrt_core -lpthread -lrt -lstdc++ -L/opt/xilinx/xrt/lib/ -ljson-glib-1.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0  

# MAKEFILE:  INFO: Target generated ./output/xbtest
# ...Done
# bash-4.2$ 
