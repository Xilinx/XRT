#!/bin/bash
debugDir="${DEBUG_DIR:-Debug}" 
relDir="${REL_DIR:-Release}" 

here=$PWD
mkdir -p $debugDir $relDir
cd  $debugDir
cmake -DCMAKE_BUILD_TYPE=Debug ../../src
make -j4 DESTDIR=$PWD install
cd $here

cd $relDir
cmake -DCMAKE_BUILD_TYPE=Release ../../src
make -j4 DESTDIR=$PWD install
cd $here


