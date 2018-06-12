#!/bin/bash

XRTBUILD=$(dirname ${BASH_SOURCE[0]})

################################################################
# Environment
################################################################
xrt=$XRTBUILD/Release/opt
sdx=/proj/xbuilds/2018.2_daily_latest/installs/lin64/SDx/2018.2
#sdx=/home/soeren/perforce/sbx-p4/REL/2018.2/prep/rdi/sdx

board=vcu1525
keep=1
sync=0
ini=""
run=1

usage()
{
    echo "Usage:"
    echo
    echo "[-help]                    List this help"
    echo "[-board <ku3|vu9p|...>]    Board to use"
    echo "[-sync]                    Sync from sprite"
    echo "[-norun]                   Don't run, just rsync all tests"
    echo "[-rm]                      Remove the synced test after run"
    echo "[-ini <path>]              Path to sdaccel.ini file"
    echo "[-xrt <path>]              Path to XRT install"
    echo "[-sdx <path>]              Path to SDx install"

    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        -board)
            shift
            board=$1
            shift
            ;;
        -rm)
            keep=0
            shift
            ;;
        -sync)
            sync=1
            shift
            ;;
        -norun)
            run=0
            shift
            ;;
        -sdx)
            shift
            sdx=$1
            shift
            ;;
        -xrt)
            shift
            xrt=$1
            shift
            ;;
        -ini)
            shift
            ini=$1
            shift
            ;;
        *)
            echo "$1 invalid argument."
            usage
            ;;
    esac
done

################################################################
# Environment
################################################################
if [ "X$ini" != "X" ] ; then
 echo "SDACCEL_INI_PATH=$ini"
 export SDACCEL_INI_PATH=$ini
fi


if [[ "X$xrt" != "X" && -d "$xrt" ]] ; then
 export XILINX_XRT=${XILINX_XRT:=$xrt}
 export LD_LIBRARY_PATH=$XILINX_XRT/xrt/lib
 export PATH=$XILINX_XRT/xrt/bin:${PATH}
fi

if [[ "X$sdx" != "X" && -d "$sdx" ]] ; then
 export XILINX_SDX=${XILINX_SDX:=$sdx}
 export XILINX_OPENCL=$XILINX_SDX
 export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$XILINX_SDX/lib/lnx64${ext}/Default:$XILINX_SDX/lib/lnx64${ext}:$XILINX_SDX/runtime/lib/x86_64
fi

echo "XILINX_XRT      = $XILINX_XRT"
echo "XILINX_SDX      = $XILINX_SDX"
echo "XILINX_OPENCL   = $XILINX_OPENCL"
echo "LD_LIBRARY_PATH = $LD_LIBRARY_PATH"


################################################################
# Test extraction
################################################################
if [ $sync -eq 0 ]; then
 # use existing already rsynced tests
 tests=(`find . -maxdepth 1 -mindepth 1 -type d`)
else
 base=/proj/fisdata2/results/sdx_2018.2/SDX_UNIT_HWBRD

 # latests csv
 csv=(`find $base -mindepth 1 -maxdepth 1 -type f -name \*.csv`)
 csv=${csv[-1]}
 suffix=$(basename $csv)
 suffix=${suffix%%.*}
 rundir=TEST_WORK_${suffix}

 # tests to rsync
 tests=(`egrep -e 'PASS' ${csv} | awk -F, '{print $3}' | grep -v PASS | grep $board | sort | awk -F/ '{print $NF}'`)
fi

for f in ${tests[*]}; do
 echo $f
done

################################################################
# Test driver
################################################################
here=$PWD
for f in ${tests[*]}; do
 cd $here
 if [ $sync == 1 ]; then
  # sync from sprite
  rsync -avz -f '- /*/*/' $base/$f/${rundir} $f/
 fi

 if [ $run -eq 0 ]; then
   continue
 fi

 cd $f
 for d in `find . -maxdepth 1 -mindepth 1 -type d`; do
  if [ `find $d -name \*.xclbin |wc -l` == 0 ]; then
   echo "NO XCLBIN : $PWD" | tee -a $here/results.all
   continue
  fi
  cd $d
  echo "================================================================"
  echo "Running test in = $PWD"
  echo "XILINX_XRT      = $XILINX_XRT"
  echo "XILINX_SDX      = $XILINX_SDX"
  echo "XILINX_OPENCL   = $XILINX_OPENCL"
  echo "LD_LIBRARY_PATH = $LD_LIBRARY_PATH"
  echo "================================================================"
  cmd=`grep '\.exe' board_lsf.sh |grep  -v echo | grep -v '/bin/cp' | /bin/sed -e 's/2>&1 | tee output.log//g'| awk '{printf("./host.exe "); for(i=5;i<=NF;++i) printf("%s ",$i)}'`
  echo "Running $f $cmd ..."
  $cmd | tee run.log
  rc=${PIPESTATUS[0]}
  cd $here
  if [ $rc != 0 ]; then
   echo "FAIL: $f $cmd" | tee -a results.all
  else
   echo "SUCCESS: $f $cmd" | tee -a results.all
   if [ $keep == 0 ]; then
    echo "deleteing test"
    /bin/rm -rf $f
   fi
  fi
 done
done
