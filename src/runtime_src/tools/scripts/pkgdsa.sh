#!/bin/bash

# This script creates rpm and deb packages for dsabin and mcs files that
# installed to /lib/firmware/xilinx
#
# The script is assumed to run on a host or docker that has all the
# necessary rpm/deb tools installed.
#
# The script uses xbinst to extract the platform deployment files
#   - dsabin, msc, bit
# xbinst is invoked from the specified or default sdx location
#
# Examples:
#
#  Package DSA from platform dir in default sdx install
#  % pkgdsa.sh \
#       -dsa xilinx_vcu1525_dynamic_5_1 \
#       -xrt 2.1.0 \
#       -cl 12345678
#
#  Package DSA from platform dir in specified sdx install
#  % pkgdsa.sh \
#       -dsa xilinx_vcu1525_dynamic_5_1 \
#       -sdx <workspace>/2018.2/prep/rdi/sdx \
#       -xrt 2.1.0 \
#       -cl 12345678
#
#  Package DSA from specified DSA platform dir using xbinst from default sdx
#  % pkgdsa.sh -dsa xilinx_vcu1525_dynamic_5_1 \
#       -dsadir <workspace>2018.2/prep/rdi/sdx/platforms/xilinx_vcu1525_dynamic_5_1 \
#       -xrt 2.1.0 \
#       -cl 12345678

opt_dsa=""
opt_dsadir=""
opt_pkgdir="/tmp/pkgdsa"
opt_sdx="/proj/xbuilds/2018.2_daily_latest/installs/lin64/SDx/2018.2"
opt_xrt=""
opt_cl=0
opt_dev=0

dsa_version="5.1"

usage()
{
    echo "package-dsa"
    echo
    echo "-dsa <name>                Name of dsa, e.g. xilinx-vcu1525-dynamic_5_1"
    echo "-sdx <path>                Full path to SDx install (default: 2018.2_daily_latest)"
    echo "-xrt <version>             Requires xrt >= <version>"
    echo "-cl <changelist>           Changelist for package revision"
    echo "[-dsadir <path>]           Full path to directory with platform (default: <sdx>/platform/<dsa>)"
    echo "[-pkgdir <path>]           Full path to direcory used by rpm,dep,xbins (default: /tmp/pkgdsa)"
    echo "[-dev]                     Build development package"
    echo "[-help]                    List this help"

    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        -cl)
            shift
            opt_cl=$1
            shift
            ;;
        -dev)
            opt_dev=1
            shift
            ;;
        -dsa)
            shift
            opt_dsa=$1
            shift
            ;;
        -dsadir)
            shift
            opt_dsadir=$1
            shift
            ;;
        -pkgdir)
            shift
            opt_pkgdir=$1
            shift
            ;;
        -xrt)
            shift
            opt_xrt=$1
            shift
            ;;
        -sdx)
            shift
            opt_sdx=$1
            shift
            ;;
        *)
            echo "$1 invalid argument."
            usage
            ;;
    esac
done

if [ "X$opt_sdx" == "X" ]; then
   echo "Must specify -sdx"
   usage
   exit 1
fi

if [ "X$opt_dsa" == "X" ]; then
   echo "Must specify -dsa"
   usage
   exit 1
fi

if [ "X$opt_dsadir" == "X" ]; then
   opt_dsadir=$opt_sdx/platforms/$opt_dsa
fi

if [ ! -d $opt_dsadir ]; then
  echo "Specified dsa "$dsa" does not exist in '$opt_dsadir'"
  usage
  exit 1
fi

if [ "X$opt_xrt" == "X" ]; then
  echo "Must specify -xrt"
  usage
  exit 1;
fi

# get dsa, version, and revision
dsa=$(echo ${opt_dsa:0:${#opt_dsa}-4} | tr '_' '-')
version=$(echo ${opt_dsa:(-3)} | tr '_' '.')
revision=$opt_cl

echo "================================================================"
echo "DSA     : $dsa"
echo "DSADIR  : $opt_dsadir"
echo "PKGDIR  : $opt_pkgdir"
echo "XRT     : $opt_xrt"
echo "VERSION : $version"
echo "REVISION: $revision"
echo "================================================================"

doxbinst()
{
    mkdir -p $opt_pkgdir/xbinst
    pushd $opt_pkgdir > /dev/null
    cd $opt_pkgdir
    if [ -d $opt_pkgdir/xbinst/$opt_dsa ]; then
        /bin/rm -rf $opt_pkgdir/xbinst/$opt_dsa
    fi
    $opt_sdx/bin/xbinst -f $opt_dsadir/$opt_dsa.xpfm -d $opt_pkgdir/xbinst/$opt_dsa
    test=$?
    popd >/dev/null
    if [ "$test" != 0 ]; then
	echo
	echo
	echo "There was an unexpected ERROR executing: "
	echo "$opt_sdx/bin/xbinst -f $opt_dsadir/$opt_dsa.xpfm -d $opt_pkgdir/xbinst/$opt_dsa"
	echo "################ xbinst failed! ###############"
	exit $test
    fi
}

dodebdev()
{
    dir=debbuild/$dsa-$version-dev
    mkdir -p $opt_pkgdir/$dir/DEBIAN
cat <<EOF > $opt_pkgdir/$dir/DEBIAN/control

package: $dsa-dev
architecture: amd64
version: $version-$revision
priority: optional
depends: $dsa (>= $version)
description: Xilinx development DSA
maintainer: soren.soe@xilinx.com

EOF

    mkdir -p $opt_pkgdir/$dir/opt/xilinx/platform/$opt_dsa/hw
    mkdir -p $opt_pkgdir/$dir/opt/xilinx/platform/$opt_dsa/sw
    rsync -avz $opt_dsadir/$opt_dsa.xpfm $opt_pkgdir/$dir/opt/xilinx/platform/$opt_dsa/
    rsync -avz $opt_dsadir/hw/$opt_dsa.dsa $opt_pkgdir/$dir/opt/xilinx/platform/$opt_dsa/hw/
    rsync -avz $opt_dsadir/sw/$opt_dsa.spfm $opt_pkgdir/$dir/opt/xilinx/platform/$opt_dsa/sw/
    dpkg-deb --build $opt_pkgdir/$dir

    echo "================================================================"
    echo "* Please locate dep for $dsa in: $opt_pkgdir/$dir"
    echo "================================================================"
}

dodeb()
{
    dir=debbuild/$dsa-$version
    mkdir -p $opt_pkgdir/$dir/DEBIAN
cat <<EOF > $opt_pkgdir/$dir/DEBIAN/control

package: $dsa
architecture: amd64
version: $version-$revision
priority: optional
depends: xrt (>= $opt_xrt)
description: Xilinx deployment DSA
 This DSA depends on xrt >= $opt_xrt.
maintainer: soren.soe@xilinx.com

EOF

    mkdir -p $opt_pkgdir/$dir/lib/firmware/xilinx
    rsync -avz $opt_pkgdir/xbinst/$opt_dsa/xbinst/firmware/ $opt_pkgdir/$dir/lib/firmware/xilinx
    mkdir -p $opt_pkgdir/$dir/opt/xilinx/dsa/$opt_dsa/test
    rsync -avz $opt_pkgdir/xbinst/$opt_dsa/xbinst/test/ $opt_pkgdir/$dir/opt/xilinx/dsa/$opt_dsa/test
    dpkg-deb --build $opt_pkgdir/$dir

    echo "================================================================"
    echo "* Please locate dep for $dsa in: $opt_pkgdir/$dir"
    echo "================================================================"
}

dorpmdev()
{
    dir=rpmbuild
    mkdir -p $opt_pkgdir/$dir/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

cat <<EOF > $opt_pkgdir/$dir/SPECS/$opt_dsa-dev.spec

buildroot:  %{_topdir}
summary: Xilinx development DSA
name: $dsa-dev
version: $version
release: $revision
license: apache
vendor: Xilinx Inc

requires: $dsa >= $version

%description
Xilinx development DSA.

%prep

%install
mkdir -p %{buildroot}/opt/xilinx/platform/$opt_dsa/hw
mkdir -p %{buildroot}/opt/xilinx/platform/$opt_dsa/sw
rsync -avz $opt_dsadir/$opt_dsa.xpfm %{buildroot}/opt/xilinx/platform/$opt_dsa/
rsync -avz $opt_dsadir/hw/$opt_dsa.dsa %{buildroot}/opt/xilinx/platform/$opt_dsa/hw/
rsync -avz $opt_dsadir/sw/$opt_dsa.spfm %{buildroot}/opt/xilinx/platform/$opt_dsa/sw/

%files
%defattr(-,root,root,-)
/opt/xilinx

%changelog
* Fri May 18 2018 Soren Soe <soren.soe@xilinx.com> - 5.1-1
  Created by script

EOF

    echo "rpmbuild --define '_topdir $opt_pkgdir/$dir' -ba $opt_pkgdir/$dir/SPECS/$opt_dsa-dev.spec"
    $dir --define '_topdir '"$opt_pkgdir/$dir" -ba $opt_pkgdir/$dir/SPECS/$opt_dsa-dev.spec

    echo "================================================================"
    echo "* Please locate rpm for dsa in: $opt_pkgdir/$dir/RPMS/x86_64"
    echo "================================================================"
}

dorpm()
{
    dir=rpmbuild
    mkdir -p $opt_pkgdir/$dir/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

cat <<EOF > $opt_pkgdir/$dir/SPECS/$opt_dsa.spec

buildroot:  %{_topdir}
summary: Xilinx deployment DSA
name: $dsa
version: $version
release: $revision
license: apache
vendor: Xilinx Inc
autoreqprov: no
requires: xrt >= $opt_xrt

%description
Xilinx deployment DSA.  This DSA depends on xrt >= $opt_xrt.

%prep

%install
mkdir -p %{buildroot}/lib/firmware/xilinx
cp $opt_pkgdir/xbinst/$opt_dsa/xbinst/firmware/* %{buildroot}/lib/firmware/xilinx
mkdir -p %{buildroot}/opt/xilinx/dsa/$opt_dsa/test
cp $opt_pkgdir/xbinst/$opt_dsa/xbinst/test/* %{buildroot}/opt/xilinx/dsa/$opt_dsa/test

%files
%defattr(-,root,root,-)
/lib/firmware/xilinx
/opt/xilinx/dsa/$opt_dsa/test

%changelog
* Fri May 18 2018 Soren Soe <soren.soe@xilinx.com> - 5.1-1
  Created by script

EOF

    echo "rpmbuild --define '_topdir $opt_pkgdir/$dir' -ba $opt_pkgdir/$dir/SPECS/$opt_dsa.spec"
    rpmbuild --define '_topdir '"$opt_pkgdir/$dir" -ba $opt_pkgdir/$dir/SPECS/$opt_dsa.spec

    echo "================================================================"
    echo "* Please locate rpm for dsa in: $opt_pkgdir/$dir/RPMS/x86_64"
    echo "================================================================"
}

FLAVOR=`grep '^ID=' /etc/os-release | awk -F= '{print $2}'`
FLAVOR=`echo $FLAVOR | tr -d '"'`

if [ $FLAVOR == "centos" ]; then
 if [ $opt_dev == 1 ]; then
     dorpmdev
 else
     doxbinst
     dorpm
 fi
fi

if [ $FLAVOR == "ubuntu" ]; then
 if [ $opt_dev == 1 ]; then
     dodebdev
 else
     doxbinst
     dodeb
 fi
fi
