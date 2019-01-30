#!/bin/bash

# Run board tests from sprite
# Usage:
#  % mkdir test
#  % cd test
#  % board.sh -board vcu1525 [-sync]
XRTBUILD=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
################################################################
# Environment
################################################################
xrt=$XRTBUILD/Release/opt/xilinx/xrt

board=vcu1525
keep=1
sync=0
ini=""
run=1
tests=
csv=
select="PASS"
rel=2018.3

usage()
{
    echo "Usage (example):"
    echo "% board.sh -board vcu1525 -sync"
    echo
    echo "[-help]                        List this help"
    echo "[-board <kcu1500|vcu1525|...>] Board to use"
    echo "[-rel <2018.2|2018.3>          Select branch to havest xclbins from (default: 2018.3)"
    echo "[-select <regex>]              Pattern to grep for in csv to pick test (default: PASS)"
    echo "[-sync]                        Sync from sprite"
    echo "[-norun]                       Don't run, just rsync all tests"
    echo "[-rm]                          Remove the synced test after run"
    echo "[-tests <path>]                List of tests to run"
    echo "[-csv <path>]                  Path to csv file to parse for tests"
    echo "[-ini <path>]                  Path to sdaccel.ini file"
    echo "[-xrt <path>]                  Path to XRT install (default: $xrt)"
    echo "[-sdx <path>]                  Path to SDx install (default: $sdx)"
    echo ""
    echo "With no optional options, this script runs all previously synced tests in"
    echo "current directory. "
    echo "% board.sh -board vcu1525"
    echo ""
    echo "Use -sync to sync all $rel UNIT_HW tests from latest sprite run into working directory."
    echo "% board.sh -board vcu1525 -sync "
    echo ""
    echo "Use -rel <release> to sync sprite tests from specified release"
    echo "% board.sh -board u200 -rel 2018.2 -sync "
    echo ""
    echo "Use -tests <file> (without -sync) to run a subset of curently synced tests.  "
    echo "The specified file should have one tests per line"
    echo "% board.sh -board vcu1525 -tests ~/tmp/files.txt"
    echo ""
    echo "Use -csv (with -sync) to explicity specify a csv file to parse for tests to sync."
    echo "The board script supports any csv file for any suite.  By default the board script"
    echo "syncs the UNIT_HW test suite, so use -csv option to sync a different suite."
    echo "The path to the csv file must be a absolute path to sprite generated file."
    echo "% board.sh -board vcu1525 -sync -csv /proj/fisdata2/results/sdx_${rel}/SDX_UNIT_HWBRD/sdx_u_hw_20180611_232013_lnx64.csv"
    echo "% board.sh -board u200 -sync -csv /proj/fisdata2/results/sdx_2018.3/SDX_CRS_HWBRD/sdx_crs_hw_20181024_223210_lnx64.csv"
    echo ""
    echo "When selecting tests from csv file, only PASS tests are selected by default."
    echo "Use the -select option to pick any tests that matches the regular expression."
    echo "% board.sh -board u200 -sync -select 'PASS|INTR' -csv <csv>"
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
        -rel)
            shift
            rel=$1
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
        -tests)
            shift
            tests=(`cat $1`)
            shift
            ;;
        -csv)
            shift
            csv=$1
            shift
            ;;
        -sdx)
            shift
            sdx=$1
            shift
            ;;
        -select)
            shift
            select=$1
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

sdx=/proj/xbuilds/${rel}_daily_latest/installs/lin64/SDx/${rel}

################################################################
# Environment
################################################################
if [ "X$ini" != "X" ] ; then
 echo "SDACCEL_INI_PATH=$ini"
 export SDACCEL_INI_PATH=$ini
fi


if [[ "X$xrt" != "X" && -d "$xrt" ]] ; then
 export XILINX_XRT=${XILINX_XRT:=$xrt}
 export LD_LIBRARY_PATH=$XILINX_XRT/lib
 export PATH=$XILINX_XRT/bin:${PATH}
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
if [[ "X$csv" == "X" ]]; then
 #default ot SDX_UNIT_HWBRD suite
 csvdir=/proj/fisdata2/results/sdx_${rel}/SDX_UNIT_HWBRD
else
 csvdir=$(dirname $csv)
fi

if [[ $sync == 0 && "X$tests" == "X" ]]; then
 # use existing already rsynced tests
 tests=(`find . -maxdepth 1 -mindepth 1 -type d`)
elif [ $sync == 1 ] ; then
 base=$csvdir

 # latests csv
 csvs=(`find $base -mindepth 1 -maxdepth 1 -type f -name \*.csv`)

 if [ "X$csv" == "X" ]; then
   csv=${csvs[-1]}
 fi

 suffix=$(basename $csv)
 suffix=${suffix%%.*}
 rundir=TEST_WORK_${suffix}

 # tests to rsync
 tests=(`egrep -e $select ${csv} | awk -F, '{print $3}' | egrep -v $select | grep $board | sort | awk -F/ '{print $NF}'`)

 if [ ${#tests[@]} == 0 ]; then
   echo "No tests found in $csv"
   echo "Use -csv to specify another csv file"
 fi
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
  echo $base/$f/${rundir}
  rsync -avz -f '- /*/*/' $base/$f/${rundir} $f/
  rsync -avz -f '+ /*/xclbin/' -f '+ /*/src/' -f '+ /*/data/' -f '- /*/*/' $base/$f/${rundir} $f/
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
  echo "RUNDIR          = $PWD"
  echo "XILINX_XRT      = $XILINX_XRT"
  echo "XILINX_SDX      = $XILINX_SDX"
  echo "XILINX_OPENCL   = $XILINX_OPENCL"
  echo "LD_LIBRARY_PATH = $LD_LIBRARY_PATH"
  echo "================================================================"

  cmd=`grep '\.exe' board_lsf.sh |grep  -v echo | grep -v '/bin/cp' | /bin/sed -e 's/2>&1 | tee output.log//g'| awk '{printf("./host.exe "); for(i=5;i<=NF;++i) printf("%s ",$i)}'`

  # this is required for dsv.onbrd suite
  if [ "X$cmd" == "X" ]; then
      cmd=`grep -e 'args.*:' sdainfo.yml | awk -F: '{print $2}'`
  fi

  echo "Running $cmd"
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
