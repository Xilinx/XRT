#####################################################################################################################
# This makefile can be used to generate output for all XMA profiling tests
# To generate executable file run: make exe
# To generate xclbin file run: make xclbin
# To remove all the tests run: make clean
# To generate xclbin and exe files run: make all
# All generated output files locate in build/opt/$testcase where $testcase is testcase name
#####################################################################################################################

TARGETS = 02_simple_xma 03_loopback_xma 100_ert_ncu_xma
all: 
	for t in $(TARGETS) ; do echo "Generating exe and xclbin files  .." ; cd  $$PWD/$$t ; make all  ;  cd .. ; done 

clean:
	for t in $(TARGETS) ; do echo "Cleaning testcase .." ; cd  $$PWD/$$t ; make clean  ;  cd .. ; done

exe:
	for t in $(TARGETS) ; do echo "Compiling the host for test $$PWD/$$t .." ; cd  $$PWD/$$t ; make exe  ;  cd .. ;done

xclbin:
	for t in $(TARGETS) ; do echo "Creating xclbin for test  $$PWD/$$t .." ; cd  $$PWD/$$t ; make xclbin  ;  cd .. ; done
