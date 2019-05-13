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
