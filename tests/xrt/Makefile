#####################################################################################################################
# This makefile can be used to generate output files for all XRT example test
# To generate executable file run: make exe
# To generate xclbin file run: make xclbin
# To remove all the tests run: make clean
# To generate xclbin and exe files run: make all
# All generated output files locate in build/opt/$testcase where $testcase is testcase name
#####################################################################################################################
TARGETS := \
 00_hello \
 02_simple \
 03_loopback \
 04_swizzle \
 07_sequence \
 11_fp_mmult256 \
 13_add_one \
 15_buffer_size \
 22_verify \
 100_ert_ncu \
 102_multiproc_verify \
 103_multiproc

all:
	for t in $(TARGETS) ; do echo "Generating exe and xclbin files  .." ; cd  $$PWD/$$t ; make all  ;  cd .. ; done

clean:
	for t in $(TARGETS) ; do echo "Cleaning testcase .." ; cd  $$PWD/$$t ; make clean  ;  cd .. ; done

exe:
	for t in $(TARGETS) ; do echo "Compiling the host for test $$PWD/$$t .." ; cd  $$PWD/$$t ; make exe  ;  cd .. ;done

xclbin:
	for t in $(TARGETS) ; do echo "Creating xclbin for test  $$PWD/$$t .." ; cd  $$PWD/$$t ; make xclbin  ;  cd .. ; done
