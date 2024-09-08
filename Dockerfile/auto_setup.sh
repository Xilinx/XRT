#!/bin/bash

# Functions

#function to compare two versions
# 0 - =
# 1 - >
# 2 - <
vercomp () {
    if [[ $1 == $2 ]]
        then
            return 0
    fi
    local IFS=.
    local i ver1=($1) ver2=($2)
    # fill empty fields in ver1 with zeros
    for ((i=${#ver1[@]}; i<${#ver2[@]}; i++))
    do
        ver1[i]=0
    done
    for ((i=0; i<${#ver1[@]}; i++))
    do
        if [[ -z ${ver2[i]} ]]
            then
            # fill empty fields in ver2 with zeros
            ver2[i]=0
        fi
        if ((10#${ver1[i]} > 10#${ver2[i]}))
            then
            return 1
        fi
        if ((10#${ver1[i]} < 10#${ver2[i]}))
            then
            return 2
        fi
    done
    return 0
}
showRels () {
  if [ -z "$DEFAULT_Release" ];then
    rels=
    vers=
    for d in `ls -d /opt/xilinx/xrt_versions/xrt_*`; do
        rel=${d#/opt/xilinx/xrt_versions/xrt_}
        ver=$(grep BUILD_VERSION $d/version.json | cut -d'"' -f4 | head -n 1)
        rels="${rels}${rels:+|}${rel}"
        vers="${vers}${vers:+, }${rel} (${ver})"
    done
    echo "WARNING: Cannot find Host XRT Version: ${driver_ver}"
    echo "WARNING: Found ${vers} in container"
    echo "WARNING: Please install one of the supported versions XRT listed above on the host machine or set the environment variable XRT_VERSION=${rels} to force run."
  fi
}
processEnvAuto () {
    #get all the supported version from the xrt folders

    DEFAULT_BIG_Release=
    DEFAULT_SMALL_Release=
    DEFAULT_Release=
    NEAREST_GT_Release=
    NEAREST_GT_ReleaseInd=
    NEAREST_LE_Release=
    NEAREST_LE_ReleaseInd=

    #choose the completely matched xrt
    for d in `ls -d /opt/xilinx/xrt_versions/xrt_*`; do
        m=$(grep "\"BUILD_VERSION\" *: *\"${driver_ver}\"" ${d}/version.json)
        if [ -n "$m" ]; then
        XILINX_XRT=$d
        DEFAULT_Release=${d#/opt/xilinx/xrt_versions/xrt_}
        DEFAULT_BIG_Release=${DEFAULT_Release:0:6}
        DEFAULT_SMALL_Release=${DEFAULT_Release:7}
        break
        fi
    done
    # warning when no matched xrt
    showRels

    #if no matched xrt, choose nearest new version -> nearest old version
    if [ -z $XILINX_XRT ]
    then
        for ((i=0; i<${index}; i++))
        do
            vercomp ${driver_ver} ${SMALL_Release[i]}
            comp=$?
        if [ "$comp" == "2" ]
        then
            if [ -z $NEAREST_GT_Release ]
            then
                NEAREST_GT_Release=${SMALL_Release[i]}
                NEAREST_GT_ReleaseInd=$i
            else
                vercomp $NEAREST_GT_Release ${SMALL_Release[i]}
            if [ "$?" == "1" ]
            then
                NEAREST_GT_Release=${SMALL_Release[i]}
                NEAREST_GT_ReleaseInd=$i
            fi
            fi
        elif [ "$comp" == "1" ]
        then
            if [ -z $NEAREST_LE_Release ]
            then
                NEAREST_LE_Release=${SMALL_Release[i]}
                NEAREST_LE_ReleaseInd=$i
            else
            vercomp $NEAREST_LE_Release ${SMALL_Release[i]}
            if [ "$?" == "2" ]
            then
                NEAREST_LE_Release=${SMALL_Release[i]}
                NEAREST_LE_ReleaseInd=$i
            fi
            fi
        fi
        done
        if [ ! -z  $NEAREST_GT_Release ]
        then
            XILINX_XRT=/opt/xilinx/xrt_versions/xrt_${Release[$NEAREST_GT_ReleaseInd]}
        elif [ ! -z $NEAREST_LE_Release ]
        then
            XILINX_XRT=/opt/xilinx/xrt_versions/xrt_${Release[$NEAREST_LE_ReleaseInd]}
        fi
    fi
}

VER=()
MAJOR_VER=()
MINOR_VER=()
Release=()
index=0 
for d in `ls -d /opt/xilinx/xrt_versions/xrt_*`; do
    rel=${d#/opt/xilinx/xrt_versions/xrt_}
    VER[$index]=${rel:7}
    MAJOR_VER[$index]=$(echo ${rel:7} | cut -d'.' -f 2)
    MINOR_VER[$index]=$(echo ${rel:7} | cut -d'.' -f 3)
    Release[$index]=${rel}
    ((index++))
done

echo "XRT Versions detected on image: ${VER[@]}" 
driver_ver=$(cat /sys/bus/pci/drivers/xocl/module/version | cut -d, -f1)
echo "XRT Version detected on host: $driver_ver" 

index=0
for check_ver in ${VER[@]}; do
    echo "Checking version: $check_ver against Driver version: $driver_ver"
    if [ "$check_ver" == "$driver_ver" ]; then
        echo "Match found for XRT Version: $check_ver"
        source /opt/xilinx/xrt_versions/xrt_${Release[$index]}/setup.sh
        return
    fi
    ((index++))
done 

echo "No direct match found, looking for closest match"
export INTERNAL_BUILD=1
host_major_ver=$(echo $driver_ver | cut -d'.' -f 2)
host_minor_ver=$(echo $driver_ver | cut -d'.' -f 3)

major_diff=()
minor_diff=()
major_match_loc=()
major_match_counter=0

for ((i=0; i<${#MAJOR_VER[@]}; i++)) do
    major_diff[i]=$(($host_major_ver-${MAJOR_VER[i]}))
    #echo ${major_diff[i]}
    if [ "${major_diff[i]}" -eq 0 ]; then
        ((major_match_counter++))
        major_match_loc=(${major_match_loc[@]} $i)
    fi
done

if [ "$major_match_counter" -eq 0 ]; then
    # We want to find the closest new version

    echo "no major version matches found"
elif [ "$major_match_counter" -eq 1 ]; then
    echo "one major version match found"
    source /opt/xilinx/xrt_versions/xrt_${Release[$major_match_loc]}/setup.sh
elif [ "$major_match_counter" -ge 1 ]; then
    echo "Major version matches found at index: ${major_match_loc[@]}"
    curr_min_loc=${major_match_loc[0]}
    curr_min=$(($host_minor_ver-${MINOR_VER[${major_match_loc[0]}]}))
    for ((i=0; i<${#major_match_loc[@]}; i++)) do
        temp_diff=$(($host_minor_ver-${MINOR_VER[${major_match_loc[i]}]}))
        minor_diff[i]=${temp_diff#-}
        if [ "$curr_min" -gt "${minor_diff[i]}" ]; then
            curr_min=${minor_diff[i]}
            curr_min_loc=${major_match_loc[i]}
        fi
    done
    source /opt/xilinx/xrt_versions/xrt_${Release[curr_min_loc]}/setup.sh
fi
