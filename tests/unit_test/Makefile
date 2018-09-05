#####################################################################################################################
# This makefile can be used to generate output files for all XRT example test
# To generate executable file run: make exe
# To generate xclbin file run: make xclbin
# To remove all the tests run: make clean
# To generate xclbin and exe files run: make all
# All generated output files locate in build/opt/$testcase where $testcase is testcase name
#####################################################################################################################
TARGETS = 001_basic_sincos 006_copy 011_mmult3 016_parkernels 002_bitonic_sort 007_copy_loop 012_mmult4 017_vectorswizzle 003_bringup0 008_globalbandwidth 013_montecarlo 018_bringup3 004_bringup1 009_mmult1 014_multikernel 019_bringup4 005_bringup2 010_mmult2 015_outoforderqueue 036_hello

all: 
	for t in $(TARGETS) ; do echo "Generating exe and xclbin files  .." ; cd  $$PWD/$$t ; make all  ;  cd .. ; done 

clean:
	for t in $(TARGETS) ; do echo "Cleaning testcase .." ; cd  $$PWD/$$t ; make clean  ;  cd .. ; done

exe:
	for t in $(TARGETS) ; do echo "Compiling the host for test $$PWD/$$t .." ; cd  $$PWD/$$t ; make exe  ;  cd .. ;done

xclbin:
	for t in $(TARGETS) ; do echo "Creating xclbin for test  $$PWD/$$t .." ; cd  $$PWD/$$t ; make xclbin  ;  cd .. ; done
