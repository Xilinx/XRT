
# xbtest
xbtest framework for kernel testing


# requires glib for json parsing
/tools/xgs/bin/sudo yum install json-glib-devel.x86_64


# requires XRT environment configured 
source /proj/xbuilds/2018.3_daily_latest/installs/lin64/SDx/2018.3/settings64.csh
source /opt/xilinx/xrt/setup.csh



# Build Instructions
make
make clean


# Run Binary
cd output/
xbtest <json filename>


# File Descriptions
xbtest_class_diagram.png	- class diagram
xbtest_example.json		- example json format
makefile			- will automatically pick up new files added and build the binary
src/xbtest.cpp 			- entry point that drives the application and handles the result from each testcase
src/logging.cpp 		- singleton class that handles logging to stdout
src/testcase.cpp 		- handles spawning individual test cases and returning the result
src/inputparser.cpp		- parses the input json file and returns device list and test parameters
src/deviceinterface.cpp		- interface to XRT/opencl, start & stop kernel and read device info
src/powertest.cpp		- implementation of the power test, inherits the interface class and build on top
src/memorytest.cpp		- implementation of the memory test, inherits the interface class and build on top







