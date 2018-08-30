#!/bin/bash

# This script creates rpm and deb packages for applications.
#
# The script is assumed to run on a host or docker that has all the
# necessary rpm/deb tools installed.
#
# Usage:
#   pkgapp.sh -app <path to app> -dep <dependencies>
#     -name <name>:  Name of the application, this will be package name
#         so should be something like 'xilinx-gzip'
#     -version <version>: Version of the application.  Must be in the form
#         of "<major>.<minor>.<revision>"
#     -install <root>: Is a full path to a directory that is the install
#         root of the application.  The directory 'root' and everyhing
#         under this directory will be packaged up for delivery.
#     -depend <file>:  A file listing all the dependencies required by
#         this application.  The dependencies will be encoded in the
#         package so that they are automatically installed if necessary.
#
# Examples:
#
#  Package GZip app
#  % pkgapp.sh \
#       -name xilinx-gzip-app \
#       -version 1.0.0 \
#       -install /home/soeren/apt/opt \
#       -depend /home/soeren/apt/gzip-deb.dep

opt_name=""
opt_version=""
opt_install=""
opt_depend=""


opt_pkgdir="/tmp/pkgapp"

usage()
{
    echo "pkgapp"
    echo
    echo "-name <name>               Name of the application"
    echo "-version <version>         Version of the application"
    echo "-install <root>            Full path to the root directory of the application to be packaged"
    echo "-depend <file>             File with application dependencies, one pr line"
    echo "[-help]                    List this help"

    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        -name)
            shift
            opt_name=$1
            shift
            ;;
        -version)
            shift
            opt_version=$1
            shift
            ;;
        -install)
            shift
            opt_install=$1
            shift
            ;;
        -depend)
            shift
            opt_depend=$1
            shift
            ;;
        *)
            echo "$1 invalid argument."
            usage
            ;;
    esac
done

if [[ "X$opt_name" == "X" ]]; then
   echo "Must specify -name"
   usage
   exit 1
fi

if [[ "X$opt_version" == "X" ]]; then
   echo "Must specify -version"
   usage
   exit 1
fi

if [[ "X$opt_install" == "X" ]]; then
   echo "Must specify -install"
   usage
   exit 1
fi

# get dsa, version, and revision
app_name=$opt_name
app_root=$opt_install
app_ver=${opt_version%.*}
app_rev=${opt_version##*.}
app_dep=$opt_depend


echo "================================================================"
echo "APP     : $app_name"
echo "VERSION : $app_ver"
echo "REVISION: $app_rev"
echo "ROOT    : $app_root"
echo "DEPEND  : $app_dep"
echo "================================================================"

dependencies=""
while IFS='' read -r line || [[ -n "$line" ]]; do
    if [[ "X$dependencies" = "X" ]]; then
        dependencies=$line
    else
        dependencies="$dependencies, $line"
    fi
done < "$app_dep"

dodeb()
{
    dir=debbuild/$app_name-$app_ver
    mkdir -p $opt_pkgdir/$dir/DEBIAN
cat <<EOF > $opt_pkgdir/$dir/DEBIAN/control

package: $app_name
architecture: all
version: $app_ver-$app_rev
priority: optional
depends: $dependencies
description: Xilinx $app_name application
maintainer: soren.soe@xilinx.com

EOF

    rsync -avz $app_root $opt_pkgdir/$dir/
    dpkg-deb --build $opt_pkgdir/$dir

    echo "================================================================"
    echo "* Please locate dep for $dsa in: $opt_pkgdir/$dir"
    echo "================================================================"
}

dorpm()
{
    dir=rpmbuild
    mkdir -p $opt_pkgdir/$dir/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
    approot=$(basename $app_root)

cat <<EOF > $opt_pkgdir/$dir/SPECS/$opt_name.spec

buildroot:  %{_topdir}
summary: Xilinx $app_name application
name: $app_name
version: $app_ver
release: $app_rev
license: apache
vendor: Xilinx Inc.

requires: $dependencies

%description
Xilinx $opt_name application.

%prep

%install
rsync -avz $app_root %{buildroot}/

%files
%defattr(-,root,root,-)
/$approot

%changelog
* Fri May 18 2018 Soren Soe <soren.soe@xilinx.com>
  Created by script

EOF

    echo "rpmbuild --define '_topdir $opt_pkgdir/$dir' -ba $opt_pkgdir/$dir/SPECS/$opt_name.spec"
    $dir --define '_topdir '"$opt_pkgdir/$dir" -ba $opt_pkgdir/$dir/SPECS/$opt_name.spec

    echo "================================================================"
    echo "* Please locate rpm for dsa in: $opt_pkgdir/$dir/RPMS/x86_64"
    echo "================================================================"
}

FLAVOR=`lsb_release -i |awk -F: '{print tolower($2)}' | tr -d ' \t'`

if [[ $FLAVOR == "centos" ]]; then
    dorpm
fi

if [[ $FLAVOR == "ubuntu" ]]; then
    dodeb
fi
