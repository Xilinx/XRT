make

printf '[Runtime]\n verbosity = 7\n runtime_log = "myrun.log"' > sdaccel.ini
echo "RUN 1: Logging in myrun.log"
: $(./host.exe -vd 2 -k hello.xclbin)

printf '[Runtime]\n verbosity = 7\n runtime_log = console' > sdaccel.ini
echo "RUN 2: Logging on console"
./host.exe -vd 2 -k hello.xclbin

rm sdaccel.ini

printf '[Runtime]\n verbosity = 7\n runtime_log = "myrun.log"' > ../sdaccel.ini
echo "RUN 3: Picking .ini from current directory (XRT/tests/) and logging in myrun.log"
cd ../
: $(./message-mgmt/host.exe -vd 2 -k hello.xclbin)

rm sdaccel.ini
