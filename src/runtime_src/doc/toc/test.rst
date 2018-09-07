Developer Build and Test Instructions
-------------------------------------

Switching XRT development work from P4 to Git can be done without much
downtime provided you use a few scripts we have created:

- ``build.sh`` build script that builds XRT for both Debug and Release profiles.
- ``run.sh`` loader script that sets up environment assuming XRT was
  built with ``build.sh``.
- ``board.sh`` harvests sprite UNIT_HW test cases and runs board tests.

Building XRT
~~~~~~~~~~~~

Make sure you build XRT on a Centos7.4+ or Ubuntu16.04.4 host.

It is probably safest if you keep your Git clone of XRT on a network
mounted drive that can be accessed from different hosts.  One
advantage is that you can have your editor run on a host that is not
used for board testing, since you don't really want host/driver
crashes to leave your unsaved edits in limbo.

::

   git clone https://github.com/Xilinx/XRT.git
   cd XRT/build
   ./build.sh

``build.sh`` script builds for both Debug and Release profiles.  It is
necessary to use the build script if you intend to use the loader
script ``run.sh`` and the board testing script ``board.sh``.

For the normal development flow, it is not necessary to build RPM or
DEB packages.  The loader and test scripts both work by
setting the environment to point at the binaries created by the build
script.

Running XRT
~~~~~~~~~~~

To run your locally built XRT with a sample ``host.exe`` and
``kernel.xclbin``, simply prepend your command line invocation with
``XRT/build/run.sh``

::

   <path>/XRT/build/run.sh ./host.exe kernel.xclbin

By default the ``run.sh`` script uses the binaries from the Release
profile.  In order run with the binaries from Debug profile use ``-dbg``
flag; this way you can even start your favorite debugger by prefixing its
invocation with ``run.sh -dbg``

::

   <path>/XRT/build/run.sh -dbg emacs


Testing XRT
~~~~~~~~~~~

After making changes to XRT in your Git clone, rebuild with
``build.sh`` as explained above, then run a full set of board tests
using the ``board.sh`` script.  For example:

::

   mkdir tests
   cd tests
   <path>/XRT/build/board.sh -board vcu1525 -sync

The ``-sync`` option tells the script to rsync tests from the latest
nightly sprite area.  Without the ``-sync`` option, the board script will
run all tests that were previously synced into the current directory.

While tests run a file named ``results.all`` will list the test with
PASS/FAIL keyword.  This file is appended (not removed between runs).
A complete run should take 5-10 mins for approximately 70 tests.


Unit Testing XRT
~~~~~~~~~~~~~~~~

We use GTest to do unit testing. The GTest package is installed by
running ``XRT/src/runtime_src/tools/scripts/xrtdeps.sh``.

The GTest package on CentOS/RHEL 7.5 provides the GTest libraries here:
 * /usr/lib64/libgtest.so 
 * /usr/lib64/libgtest_main.so

However, the GTest package on Ubuntu 16.04 provides source only!

To use GTest on Ubuntu 16.04 use:

::

   cd /usr/src/gtest
   sudo cmake CMakeLists.txt
   sudo make
   cd /usr/lib
   sudo ln -s /usr/src/gtest/libgtest.a
   sudo ln -s /usr/src/gtest/libgtest_main.a
   # Validate:
   ls *gtest*

This will add GTest static library symbolic links here:
 * /usr/lib/libgtest.a
 * /usr/lib/libgtest_main.a

CMake will handle linking, finding etc. for you.

To add GTest support to a CMakeLists.txt use the following, and this is using 
an example executable called 'xclbintest':

::

   find_package(GTest)
   if (GTEST_FOUND)
     enable_testing()
     message (STATUS "GTest include dirs: '${GTEST_INCLUDE_DIRS}'")
     include_directories(${GTEST_INCLUDE_DIRS})
     add_executable(xclbintest unittests/main.cpp unittests/test.cpp)
     message (STATUS "GTest libraries: '${GTEST_BOTH_LIBRARIES}'")
     target_link_libraries(xclbintest ${GTEST_BOTH_LIBRARIES} pthread)
   else()
     message (STATUS "GTest was not found, skipping generation of test executables")
   endif()

