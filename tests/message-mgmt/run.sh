usage()
{
    echo "Usage: run.sh [options]"
    echo
    echo "[-xrt <path>]    xrt install location, e.g. /opt/xilinx/xrt"
    exit 1
}

xrt_install_path="/opt/xilinx/xrt"

while [ $# -gt 0 ]; do
    case "$1" in
        -xrt)
            shift
            xrt_install_path=$1
            shift
            ;;
        *)
            echo "unknown option"
            usage
            ;;
    esac
done

export XRT_INSTALL_PATH=${xrt_install_path}
echo "XRT_INSTALL_PATH is: $XRT_INSTALL_PATH"

make

ROOT=$PWD

rm xrt.ini
printf '[Runtime]\n verbosity = 7\n runtime_log = "myrun.log"' > sdaccel.ini
echo "RUN 1: Logging in myrun.log"
: $(./host.exe -vd 2 -k hello.xclbin)

printf '[Runtime]\n verbosity = 7\n runtime_log = console' > sdaccel.ini
echo "RUN 2: Logging on console"
./host.exe -vd 2 -k hello.xclbin

rm sdaccel.ini

printf '[Runtime]\n verbosity = 7\n runtime_log = syslog' > sdaccel.ini
echo "RUN 2: Logging in syslog"
./host.exe -vd 2 -k hello.xclbin

rm sdaccel.ini

mkdir -p tdir
cd tdir
printf '[Runtime]\n verbosity = 7\n runtime_log = "myrun.log"' > sdaccel.ini
echo "RUN 3: Picking .ini from current directory $PWD and logging in myrun.log"
../host.exe -vd 2 -k ../hello.xclbin
rm sdaccel.ini

cd $ROOT

printf '[Runtime]\n verbosity = 7\n runtime_log = console' > xrt.ini
echo "RUN 2: Logging on console"
./host.exe -vd 2 -k hello.xclbin

rm xrt.ini
