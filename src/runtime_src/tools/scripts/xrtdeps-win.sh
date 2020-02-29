#!/bin/bash

COMPILER="Visual Studio 15 2017 Win64"
BOOSTINC=/mnt/c/xrt/libs/boost/include/boost-1_70
BOOSTLIB=/mnt/c/xrt/libs/boost/lib
KHRONOS=/mnt/c/xrt/libs/KhronosGroupX

boost=0
git=0
cmake=0
opencl=0
icd=0
icd_debug=0
validate=0
usage()
{
    echo "Usage: xrtdeps-win.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[-boost]                   Install boost (default: $boost)"
    echo "[-opencl]                  Install OpenCL headers (default: $opencl)"
    echo "[-icd]                     Install OpenCL icd loader (default: $icd)"
    echo "[-all]                     Install all (default: $all)"
    echo "[-validate]                Validate that required packages are installed"
    exit 1
}


while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
	-boost)
	    boost=1
	    shift
	    ;;
	-cmake)
	    cmake=1
	    shift
	    ;;
        -git)
            git=1
            shift
            ;;
        -icd)
            icd=1
            shift
            ;;
        -icd_debug)
            icd_debug=1
            shift
            ;;
        -opencl)
            opencl=1
            shift
            ;;
        -all)
            boost=1
            cmake=1
            git=1
            icd=1
            opencl=1
            shift
            ;;
        -validate)
            validate=1
            shift
            ;;
        *)
            echo "unknown option $1"
            usage
            ;;
    esac
done

wsl2win()
{
    echo $(sed -e 's|/mnt/\([A-Za-z]\)/\(.*\)|\1:/\2|' -e 's|/|\\|g' <<< $1)
}

# $(1): direcotory
# $(2): filelist
checkexist()
{
    local dir=$1
    shift
    local files=("$@")
    local err=0
    for f in "${files[@]}"; do
        if [[ ! -e $dir/$f ]]; then
            printf '%s is missing in %s\n' "$f" "$dir" >&2
            err=1
        fi
    done

    echo $err
}

checkboost()
{
    local err=0
    echo "Checking boost installation ..." >&2
    if [[ ! -e $BOOSTINC ]] || [[ ! -e $BOOSTLIB ]]; then
        printf "No boost libraries found under $BOOSTINC or $BOOSTLIB" >&2
        err=1
    fi
    echo $err
}

checkgit()
{
    local err=0
    echo "Checking git installation ..." >&2
    GIT=`cmd.exe /c where git.exe`
    if [[ -z $GIT ]]; then
        echo "No git found in PATH" >&2
        err=1
    fi
    echo $err
}

checkcmake()
{
    local err=0
    echo "Checking cmake installation ..." >&2
    CMAKE=`cmd.exe /c where cmake.exe`
    if [[ -z $CMAKE ]]; then
        echo "No cmake found in PATH" >&2
        err=1
    fi
    echo $err
}

checkopencl()
{
    local err=0
    echo "Checking for Khronos OpenCL headers ..." >&2
    files=("include/CL/cl.h" "include/CL/cl_ext.h")
    if [[ $(checkexist "$KHRONOS" "${files[@]}") != 0 ]];  then
        err=1
    fi
    echo $err
}

checkicd()
{
    local err=0
    echo "Checking for Khronos ICD loader ...." >&2
    files=("include/icd.h" "include/icd_dispatch.h" "lib/OpenCL.lib" "bin/OpenCL.dll")
    if [[ $(checkexist "$KHRONOS" "${files[@]}") != 0 ]];  then
        err=1
    fi
    echo $err
}

validate()
{
    local err=0
    if [[ $(checkboost) != 0 ]] ; then err=1; fi
    if [[ $(checkgit) != 0 ]] ; then err=1; fi
    if [[ $(checkcmake) != 0 ]] ; then err=1; fi
    if [[ $(checkopencl) != 0 ]] ; then err=1; fi
    if [[ $(checkicd) != 0 ]] ; then err=1; fi
    echo $err

}

git_install()
{
    echo "git: Check README.md for installation instructions ..."
}

cmake_install()
{
    echo "cmake: Check README.md for installation instructions ..."
}

boost_install()
{
    echo "boost: Check README.md for installation instructions ..."
}

opencl_install()
{
    echo "Installing OpenCL headers"
    if [[ $(checkgit) != 0 ]] ; then return; fi

    # Clone https://github.com/KhronosGroup/OpenCL-Headers.git
    mkdir -p $KHRONOS
    git clone https://github.com/KhronosGroup/OpenCL-Headers.git $KHRONOS/OpenCL-Headers
    rsync -avz $KHRONOS/OpenCL-Headers/CL $KHRONOS/include
}

icd_install()
{
    echo "Installing ICD loader ..."
    if [[ $(checkgit) != 0 ]] ; then return; fi
    if [[ $(checkcmake) != 0 ]] ; then return; fi
    if [[ $(checkopencl) != 0 ]] ; then
        echo "OpenCL headers must be installed before ICD loader can be installed"
        return
    fi

    git clone https://github.com/KhronosGroup/OpenCL-ICD-Loader.git $KHRONOS/OpenCL-ICD-Loader
    rsync -avz $KHRONOS/OpenCL-Headers/CL $KHRONOS/OpenCL-ICD-Loader/inc
    local here=$PWD
    cd $KHRONOS/OpenCL-ICD-Loader
    mkdir build
    cd build
    local cmake=`which cmake.exe`
    "$cmake" -G "$COMPILER" -DCMAKE_INSTALL_PREFIX=$(wsl2win $KHRONOS) -DCMAKE_BUILD_TYPE=Debug ..
    "$cmake" --build . --verbose --config Debug
    "$cmake" --build . --verbose --config Debug --target install
    cd ..
    rsync -avz loader/*.h $KHRONOS/include/
    cd $here
}

icd_debug()
{
    cd $KHRONOS/OpenCL-ICD-Loader
    mkdir build
    cd build
    local cmake=`which cmake.exe`
    "$cmake" -G "$COMPILER" -DCMAKE_INSTALL_PREFIX=$(wsl2win $KHRONOS) -DCMAKE_BUILD_TYPE=Debug ..
    "$cmake" --build . --verbose --config Debug
    cd $here
}

if [[ $git == 1 ]]; then
    git_install
fi

if [[ $cmake == 1 ]]; then
    cmake_install
fi

if [[ $boost == 1 ]]; then
    boost_install
fi

if [[ $opencl == 1 ]] && [[ $(checkopencl) != 0 ]]; then
    opencl_install
fi

if [[ $icd == 1 ]] && [[ $(checkicd) != 0 ]]; then
    icd_install
fi

if [[ $icd_debug == 1 ]]; then
    icd_debug
fi

if [[ $validate == 1 ]]; then
    validate
fi
