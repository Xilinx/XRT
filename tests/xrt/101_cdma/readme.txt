To build and run locally

% [run.sh] make CXX=/proj/xbuilds/2018.2_daily_latest/installs/lin64/SDx/2018.2/bin/xcpp debug=0 exe  [MYCFLAGS=-DVERBOSE]
% [run.sh] make CXX=/proj/xbuilds/2018.2_daily_latest/installs/lin64/SDx/2018.2/bin/xcpp debug=1 exe  [MYCFLAGS=-DVERBOSE]
% [run.sh] make debug=0 xclbin

# kds scheduling without waitlist
% [run.sh] ../build/opt/101_cdma/101_cdma.exe -k ../build/opt/101_cdma/kernel.xclbin --jobs 64 --seconds 10

# kds scheduling with waitlist
% [run.sh] ../build/opt/101_cdma/101_cdma.exe -k ../build/opt/101_cdma/kernel.xclbin --jobs 64 --seconds 10 --wl

# ert scheduling without waitlist
% [run.sh] ../build/opt/101_cdma/101_cdma.exe -k ../build/opt/101_cdma/kernel.xclbin --jobs 64 --seconds 10 --ert

# ert scheduling with waitlist
% [run.sh] ../build/opt/101_cdma/101_cdma.exe -k ../build/opt/101_cdma/kernel.xclbin --jobs 64 --seconds 10 --wl --ert

# to compile a list of results for (kds), (kds,--wl), (--ert), (-ert,--wl)
% ./runme.sh |& tee run.log
% grep kds run.log
% grep ert run.log

