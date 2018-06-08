#!/bin/bash

#aarch64-linux-gnu-g++ -g -std=c++11 zynq_user_test_add1.cpp -I /proj/rdi-xsj/staff/umangp/work1/HEAD/src/products/sdx/ocl/src/runtime_src -I ../../kernel2 -ldl -o usertest

aarch64-linux-gnu-g++ -g -std=c++11 zynq_user_test_add1.cpp -I /proj/rdi-xsj/staff/umangp/work1/HEAD/src/products/sdx/ocl/src/runtime_src -I ../../kernel2 -ldl -o usertest   xclHALProxy2.cpp xclHALProxy2.h
