To build and run locally

% [run.sh] make CXX=/proj/xbuilds/2018.2_daily_latest/installs/lin64/SDx/2018.2/bin/xcpp debug=0 exe
% [run.sh] make debug=0 xclbin
% [run.sh] ../build/opt/100_ert_ncu/100_ert_ncu.exe -k kernel.xclbin --jobs 1024 --seconds 1 --cus 8 --ert
