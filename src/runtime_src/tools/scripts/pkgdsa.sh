#!/bin/bash

# This script creates rpm and deb packages for dsabin and mcs files that
# installed to /lib/firmware/xilinx
#
# The script is assumed to run on a host or docker that has all the
# necessary rpm/deb tools installed.
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
#  Package DSA from specified DSA platform dir 
#  % pkgdsa.sh -dsa xilinx_vcu1525_dynamic_5_1 \
#       -dsadir <workspace>2018.2/prep/rdi/sdx/platforms/xilinx_vcu1525_dynamic_5_1 \
#       -xrt 2.1.0 \
#       -cl 12345678

if [ "X${XILINX_XRT}" == "X" ]; then
  echo "Environment variable XILINX_XRT is not set.  Please source the XRT setup script."
  exit 1;
fi

opt_xrt=""
parseVersionFile()
{
  # Need to encapsulte this proc so that it doesn't interfer with the usage
  version_json="${XILINX_XRT}/version.json"
    if [ -f "${version_json}" ]; then
    # Get the XRT version
    set -- `cat "${version_json}" | python -c "import sys, json; print json.load(sys.stdin).get('BUILD_VERSION','')"`
    opt_xrt="${1}"
  fi
}

parseVersionFile

opt_dsa=""
opt_dsadir=""
opt_pkgdir="/tmp/pkgdsa"
opt_sdx="/proj/xbuilds/2018.2_daily_latest/installs/lin64/SDx/2018.2"
opt_cl=0
opt_dev=0
opt_deploy=0
license_dir=""

dsa_version="5.1"
build_date=`date +"%a %b %d %Y"`

usage()
{
    echo "package-dsa"
    echo
    echo "-dsa <name>                Name of dsa, e.g. xilinx-vcu1525-dynamic_5_1"
    echo "-sdx <path>                Full path to SDx install (default: 2018.2_daily_latest)"
    echo "[-xrt <version>]           Requires xrt >= <version>"
    echo "[-cl <changelist>]         Changelist for package revision"
    echo "[-dsadir <path>]           Full path to directory with platforms (default: <sdx>/platforms/<dsa>)"
    echo "[-pkgdir <path>]           Full path to direcory used by rpm,dep,xbins (default: /tmp/pkgdsa)"
    echo "[-dev]                     Build development package"
    echo "[-deploy]                  Build deployment package [default]"
    echo "[-license <path>]          Include license file(s) from the <path> in the package"
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
        -deploy)
            opt_deploy=1
            shift
            ;;
        -dsa)
            shift
            opt_dsa=$1
            shift
            ;;
        -license)
            shift
	    license_dir=$1
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
# Parse the platform into the basic parts
#    Syntax: <vender>_<board>_<name>_<versionMajor>_<versionMinor>
set -- `echo $opt_dsa | tr '_' ' '`
dsa="${1}-${2}-${3}"
version="${4}.${5}"
revision=$opt_cl

echo "================================================================"
echo "DSA       : $dsa"
echo "DSADIR    : $opt_dsadir"
echo "PKGDIR    : $opt_pkgdir"
echo "XRT       : $opt_xrt"
echo "VERSION   : $version"
echo "REVISION  : $revision"
echo "XILINX_XRT: $XILINX_XRT"
echo "================================================================"

# DSABIN variables
dsaFile=""
mcsPrimary=""
mcsSecondary=""
fullBitFile=""
clearBitstreamFile=""
metaDataJSONFile=""
dsaXmlFile="dsa.xml"
featureRomTimestamp="0"
fwScheduler=""
fwManagement=""
fwBMC=""
fwBMCMetaData=""
vbnv=""
pci_vendor_id="0x0000"
pci_device_id="0x0000"
pci_subsystem_id="0x0000"
dsabinOutputFile=""
SatelliteControllerFamily=""
CardMgmtControllerFamily=""
SchedulerFamily=""

XBUTIL=/opt/xilinx/xrt/bin/xbutil
post_inst_msg="DSA package installed successfully.
Please flash card manually by running below command:
sudo ${XBUTIL} flash -a ${opt_dsa} -t"

createEntityAttributeArray ()
{
  unset ENTITY_ATTRIBUTES_ARRAY
  declare -A -g ENTITY_ATTRIBUTES_ARRAY

  for kvp in $ENTITY_ATTRIBUTES; do
    set -- `echo $kvp | tr '=' ' '`
    # Remove leading and trailing quotes
    value=$2
    value="${value%\"}"
    value="${value#\"}"
    ENTITY_ATTRIBUTES_ARRAY[$1]=$value
  done
}

readSAX () {
  # Set Input Field Spearator to be local to this function and change it to
  # the '>' character
  local IFS=\>

  # Read the input from stdin and stop when the '<' character is seen.
  read -d \< ENTITY_LINE

  local ret=$?

  # Remove any trailing "/>" characters
  ENTITY_LINE="${ENTITY_LINE///>/}"

  # Remove any carriage returns
  ENTITY_LINE="${ENTITY_LINE//$'\n'/}"

  # Remove any training whitespaces
  ENTITY_LINE="${ENTITY_LINE%"${ENTITY_LINE##*[![:space:]]}"}" 

  ENTITY_NAME="${ENTITY_LINE%% *}"
  ENTITY_ATTRIBUTES="${ENTITY_LINE#* }"

  return $ret
}

recordDsaFiles()
{
   # Full Static Bitstream
   if [ "${ENTITY_ATTRIBUTES_ARRAY[Type]}" == "FULL_BIT" ]; then
     fullBitFile="${ENTITY_ATTRIBUTES_ARRAY[Name]}"
   fi

   # MCS Primary
   if [ "${ENTITY_ATTRIBUTES_ARRAY[Type]}" == "MCS" ]; then
     mcsPrimary="firmware/${ENTITY_ATTRIBUTES_ARRAY[Name]}"
   fi

   # MCS Secondary
   if [ "${ENTITY_ATTRIBUTES_ARRAY[Type]}" == "SECONDARY_MCS" ]; then
     mcsSecondary="firmware/${ENTITY_ATTRIBUTES_ARRAY[Name]}"
   fi

   # Clear Bitstream
   if [ "${ENTITY_ATTRIBUTES_ARRAY[Type]}" == "CLEAR_BIT" ]; then
     clearBitstreamFile="${ENTITY_ATTRIBUTES_ARRAY[Name]}"
   fi

   # Metadata
   if [ "${ENTITY_ATTRIBUTES_ARRAY[Type]}" == "META_JSON" ]; then
     metaDataJSONFile="${ENTITY_ATTRIBUTES_ARRAY[Name]}"
   fi
}

readDsaMetaData()
{
  # -- Extract the dsa.xml metadata file --
  unzip -q -d . "${dsaFile}" "${dsaXmlFile}"

  while readSAX; do
    # Record the data types
    if [ "${ENTITY_NAME}" == "File" ]; then
      createEntityAttributeArray
      recordDsaFiles
    fi    

    # Record the FeatureRomTimestamp
    if [ "${ENTITY_NAME}" == "DSA" ]; then
      createEntityAttributeArray

      featureRomTimestamp="${ENTITY_ATTRIBUTES_ARRAY[FeatureRomTimestamp]}"

      vendor="${ENTITY_ATTRIBUTES_ARRAY[Vendor]}"
      board="${ENTITY_ATTRIBUTES_ARRAY[BoardId]}"
      name="${ENTITY_ATTRIBUTES_ARRAY[Name]}"
      versionMajor="${ENTITY_ATTRIBUTES_ARRAY[VersionMajor]}"
      versionMinor="${ENTITY_ATTRIBUTES_ARRAY[VersionMinor]}"
      vbnv=$(printf "%s:%s:%s:%s.%s" "${vendor}" "${board}" "${name}" "${versionMajor}" "${versionMinor}")
    fi    

    # Record the PCIeID information 
    if [ "${ENTITY_NAME}" == "PCIeId" ]; then
      createEntityAttributeArray

      pci_vendor_id="${ENTITY_ATTRIBUTES_ARRAY[Vendor]}"
      pci_device_id="${ENTITY_ATTRIBUTES_ARRAY[Device]}"
      pci_subsystem_id="${ENTITY_ATTRIBUTES_ARRAY[Subsystem]}"
    fi    

    # FeatureRom Data
    if [ "${ENTITY_NAME}" == "FeatureRom" ]; then
      createEntityAttributeArray

      # Overright previous value
      featureRomTimestamp="${ENTITY_ATTRIBUTES_ARRAY[TimeSinceEpoch]}"
    fi    

  done < "${dsaXmlFile}"
}

initBMCVar()
{
    prefix=""
    if [ "${SatelliteControllerFamily}" != "" ]; then
      if [ "${SatelliteControllerFamily}" == "Alveo-Gen1" ]; then
         prefix="Alveo-Gen1:"
      elif [ "${SatelliteControllerFamily}" == "Alveo-Gen2" ]; then
         prefix="Alveo-Gen2:"
      else
         echo "ERROR: Unknown satellite controller family: ${SatelliteControllerFamily}"
         exit 1
      fi
    else
      # We are not meant to load MSP432 fWFW
      return
    fi

    # Looking for the MSP432 firmware image
    for file in ${XILINX_XRT}/share/fw/${prefix}*.txt; do
      [ -e "$file" ] || continue

      # Found "something" break it down into the basic parts
      baseFileName="${file%.*txt}"        # Remove suffix
      baseFileName="${baseFileName##*/}"  # Remove Path
      baseFileName=${baseFileName#"$prefix"}  # Remove prefix

      set -- `echo ${baseFileName} | tr '-' ' '`
      bmcImageName="${1}"
      bmcDeviceName="${2}"
      bmcVersion="${3}"
      bmcMd5Expected="${4}"

      # Calculate the md5 checksum
      set -- $(md5sum $file)
      bmcMd5Actual="${1}"

      if [ "${bmcMd5Expected}" == "${bmcMd5Actual}" ]; then
         echo "Info: Validated ${prefix}:${baseFileName} flash image MD5 value"
         fwBMC="${file}"
         fwBMCMetaData="${baseFileName}.json"
         echo "{\"bmc_metadata\": { \"m_image_name\": \"${bmcImageName}\", \"m_device_name\": \"${bmcDeviceName}\", \"m_version\": \"${bmcVersion}\", \"m_md5value\": \"${bmcMd5Expected}\"}}" > "${fwBMCMetaData}"
      else
         echo "ERROR: MSP432 Flash image failed MD5 varification."
         echo "       Expected: ${bmcMd5Expected}"
         echo "       Actual  : ${bmcMd5Actual}"
         echo "       File:   : $file"
         exit 1
      fi

      # We only go through this loop once
      return
    done
}

initDsaBinEnvAndVars()
{
    # Clean out the dsabin directory
    /bin/rm -rf "${opt_pkgdir}/dsabin"
    mkdir -p "${opt_pkgdir}/dsabin"
    mkdir -p "${opt_pkgdir}/dsabin/firmware"
    cd "${opt_pkgdir}/dsabin"

    # -- Get the DSA for this platform --
    dsaFile="${opt_dsadir}/hw/${opt_dsa}.dsa"
    if [ ! -f "${dsaFile}" ]; then
       echo "Error: DSA file does not exist: ${dsaFile}"
       popd >/dev/null
       exit 1
    fi
  
    # Read the metadata from the dsa.xml file 
    readDsaMetaData
  
    # -- Extract the MCS Files --
    if [ "${mcsPrimary}" != "" ]; then
       echo "Info: Extracting MCS Primary file: ${mcsPrimary}"
       unzip -q -d . "${dsaFile}" "${mcsPrimary}"
    fi

    if [ "${mcsSecondary}" != "" ]; then
       echo "Info: Extracting MCS Secondary file: ${mcsSecondary}"
       unzip -q -d . "${dsaFile}" "${mcsSecondary}"
    fi

    # -- Extract the bitstreams --
    if [ "${fullBitFile}" != "" ]; then
       echo "Info: Extracting Full Bitstream file: ${fullBitFile}"
       unzip -q -d "./firmware" "${dsaFile}" "${fullBitFile}"
    fi

    if [ "${clearBitstreamFile}" != "" ]; then
       echo "Info: Extracting Clear Bitstream file: ${clearBitstreamFile}"
       unzip -q -d "./firmware" "${dsaFile}" "${clearBitstreamFile}"
    fi

    # -- Extract the Metadata
    # Default values
    SatelliteControllerFamily=""
    CardMgmtControllerFamily="Legacy"
    SchedulerFamily="ERT-Gen1"

    if [[ ${opt_dsa} =~ "xdma" ]]; then
      CardMgmtControllerFamily="CMC-Gen1"
      SatelliteControllerFamily="Alveo-Gen1"
    fi

    if [ "${metaDataJSONFile}" != "" ]; then
       echo "Info: Extracting Metadata file: ${metaDataJSONFile}"
       unzip -q -d "." "${dsaFile}" "${metaDataJSONFile}"

       # Brute force to obtain this data
       # See if there is a dsabin section
       set -- `cat "${metaDataJSONFile}" | python -c "import sys, json; print json.load(sys.stdin).get('dsabin','NOT_DEFINED')"`
       if [ "${1}" != "NOT_DEFINED" ]; then
          # Satellite Controller Family (MSP432)
          set -- `cat "${metaDataJSONFile}" | python -c "import sys, json; print json.load(sys.stdin)['dsabin'].get('Satellite Controller Family','NOT_DEFINED')"`
          if [ "${1}" != "NOT_DEFINED" ]; then
            SatelliteControllerFamily="${1}"
          fi

          # Card Management Controller Family
          set -- `cat "${metaDataJSONFile}" | python -c "import sys, json; print json.load(sys.stdin)['dsabin'].get('Card Management Controller Family','NOT_DEFINED')"`
          if [ "${1}" != "NOT_DEFINED" ]; then
            CardMgmtControllerFamily="${1}"
          fi

          # Scheduler Family
          set -- `cat "${metaDataJSONFile}" | python -c "import sys, json; print json.load(sys.stdin)['dsabin'].get('Scheduler Family','NOT_DEFINED')"`
          if [ "${1}" != "NOT_DEFINED" ]; then
            SchedulerFamily="${1}"
          fi
       fi
    fi

    echo "Info: Satellite Controller Family: ${SatelliteControllerFamily}"
    echo "Info: Card Management Controller Family: ${CardMgmtControllerFamily}"
    echo "Info: Scheduler Family: ${SchedulerFamily}"

    # -- Determine scheduler firmware --
    fwScheduler=""
    if [ "${SchedulerFamily}" != "" ]; then
      if [ "${SchedulerFamily}" == "ERT-Gen1" ]; then
         fwScheduler="${XILINX_XRT}/share/fw/sched.bin"
      else
         echo "ERROR: Unknown scheduler firmware family: ${SchedulerFamily}"
         exit 1
      fi
    fi

    # -- Determine management firmware --
    fwManagement=""
    if [ "${CardMgmtControllerFamily}" != "" ]; then
      if [ "${CardMgmtControllerFamily}" == "Legacy" ]; then
         fwManagement="${XILINX_XRT}/share/fw/mgmt.bin"
      elif [ "${CardMgmtControllerFamily}" == "CMC-Gen1" ]; then
         fwManagement="${XILINX_XRT}/share/fw/cmc.bin"
      else
         echo "ERROR: Unknown card management controller family: ${CardMgmtControllerFamily}"
         exit 1
      fi
    fi

    # -- MSP432 --
    initBMCVar
}

docentos()
{
 echo "Packaging for CentOS..."

 # Requesting to create the development package
 if [ $opt_dev == 1 ]; then
     echo "Creating development package..."
     dorpmdev
 fi

 # If the development package is not be requested (default is deployment)
 # OR the deployment package is also being requested to be produced.
 if [[ $opt_dev == 0 ]] || [[ $opt_deploy == 1 ]]; then
     echo "Creating deployment package..."
     dodsabin
     dorpm
 fi
}

doubuntu()
{
 echo "Packaging for Ubuntu..."
 
 # Requesting to create the development package
 if [ $opt_dev == 1 ]; then
     echo "Creating development package..."
     dodebdev
 fi

 # If the development package is not be requested (default is deployment)
 # OR the deployment package is also being requested to be produced.
 if [[ $opt_dev == 0 ]] || [[ $opt_deploy == 1 ]]; then
     echo "Creating deployment package..."
     dodsabin
     dodeb
 fi
}

dodsabin()
{
    pushd $opt_pkgdir > /dev/null
    echo "Creating dsabin for: ${opt_dsa}"

    initDsaBinEnvAndVars

    # Build the xclbincat options
    xclbinOpts=""

    # -- MCS_PRIMARY image --
    if [ "$mcsPrimary" != "" ]; then
       xclbinOpts+=" --add-section MCS-PRIMARY:RAW:${mcsPrimary}"
    fi
    
    # -- MCS_SECONDARY image --
    if [ "$mcsSecondary" != "" ]; then
       xclbinOpts+=" --add-section MCS-SECONDARY:RAW:${mcsSecondary}"
    fi
    
    # -- Firmware: Scheduler --
    if [ "${fwScheduler}" != "" ]; then
       if [ -f "${fwScheduler}" ]; then
         xclbinOpts+=" --add-section SCHED_FIRMWARE:RAW:${fwScheduler}"
       else
         echo "Warning: Scheduler firmware does not exist: ${fwScheduler}"
       fi
    fi
    
    # -- Firmware: Management --
    if [ "${fwManagement}" != "" ]; then
       if [ -f "${fwManagement}" ]; then
         xclbinOpts+=" --add-section FIRMWARE:RAW:${fwManagement}"
       else
         echo "Warning: Management firmware does not exist: ${fwManagement}"
      fi
    fi

    # -- Firmware: MSP432 --
    if [ "${fwBMC}" != "" ]; then
       if [ -f "${fwBMC}" ]; then
         xclbinOpts+=" --add-section BMC-FW:RAW:${fwBMC}"
         xclbinOpts+=" --add-section BMC-METADATA:JSON:${fwBMCMetaData}"
       else
         echo "Warning: MSP432 firmware does not exist: ${fwBMC}"
      fi
    fi

    # -- Clear bitstream --
    if [ "${clearBitstreamFile}" != "" ]; then
       xclbinOpts+=" --add-section CLEARING_BITSTREAM:RAW:./firmware/${clearBitstreamFile}"
    fi

    # -- FeatureRom Timestamp --
    if [ "${featureRomTimestamp}" != "" ]; then
       xclbinOpts+=" --key-value SYS:FeatureRomTimestamp:${featureRomTimestamp}"
    else
       echo "Warning: Missing featureRomTimestamp"
    fi

    # -- VBNV --
    if [ "${vbnv}" != "" ]; then
       xclbinOpts+=" --key-value SYS:PlatformVBNV:${vbnv}"
    else
       echo "Warning: Missing Platform VBNV value"
    fi


    # -- Mode Hardware PR --
    xclbinOpts+=" --key-value SYS:mode:hw_pr"

    # -- Output filename --
    localFeatureRomTimestamp="${featureRomTimestamp}"
    if [ "${localFeatureRomTimestamp}" == "" ]; then
      localFeatureRomTimestamp="0"
    fi

    # Build output file and lowercase the name
    dsabinOutputFile=$(printf "%s-%s-%s-%016x.dsabin" "${pci_vendor_id#0x}" "${pci_device_id#0x}" "${pci_subsystem_id#0x}" "${localFeatureRomTimestamp}")
    dsabinOutputFile="${dsabinOutputFile,,}"
    xclbinOpts+=" --output ./firmware/${dsabinOutputFile}"    


    echo "${XILINX_XRT}/bin/xclbinutil ${xclbinOpts}"
    ${XILINX_XRT}/bin/xclbinutil ${xclbinOpts}
    retval=$?

    popd >/dev/null

    if [ $retval -ne 0 ]; then
       echo "ERROR: xclbinutil failed.  Exiting."
       exit
    fi
}

dodsabin_xclbincat()
{
    pushd $opt_pkgdir > /dev/null
    echo "Creating dsabin for: ${opt_dsa}"

    initDsaBinEnvAndVars

    # Build the xclbincat options
    xclbinOpts=""

    # -- MCS_PRIMARY image --
    if [ "$mcsPrimary" != "" ]; then
       xclbinOpts+=" -s MCS_PRIMARY ${mcsPrimary}"
    fi
    
    # -- MCS_SECONDARY image --
    if [ "$mcsSecondary" != "" ]; then
       xclbinOpts+=" -s MCS_SECONDARY ${mcsSecondary}"
    fi
    
    # -- Firmware: Scheduler --
    if [ "${fwScheduler}" != "" ]; then
       if [ -f "${fwScheduler}" ]; then
         xclbinOpts+=" -s SCHEDULER ${fwScheduler}"
       else
         echo "Warning: Scheduler firmware does not exist: ${fwScheduler}"
       fi
    fi
    
    # -- Firmware: Management --
    if [ "${fwManagement}" != "" ]; then
       if [ -f "${fwManagement}" ]; then
         xclbinOpts+=" -s FIRMWARE ${fwManagement}"
       else
         echo "Warning: Management firmware does not exist: ${fwManagement}"
      fi
    fi

    # -- Firmware: MSP432 --
    if [ "${fwBMC}" != "" ]; then
       if [ -f "${fwBMC}" ]; then
         xclbinOpts+=" -s BMC ${fwBMC}"
       else
         echo "Warning: MSP432 firmware does not exist: ${fwBMC}"
      fi
    fi

    # -- Clear bitstream --
    if [ "${clearBitstreamFile}" != "" ]; then
       xclbinOpts+=" -s CLEAR_BITSTREAM ./firmware/${clearBitstreamFile}"
    fi

    # -- FeatureRom Timestamp --
    if [ "${featureRomTimestamp}" != "" ]; then
       xclbinOpts+=" --kvp featureRomTimestamp:${featureRomTimestamp}"
    else
       echo "Warning: Missing featureRomTimestamp"
    fi

    # -- VBNV --
    if [ "${vbnv}" != "" ]; then
       xclbinOpts+=" --kvp platformVBNV:${vbnv}"
    else
       echo "Warning: Missing Platform VBNV value"
    fi


    # -- Mode Hardware PR --
    xclbinOpts+=" --kvp mode:hw_pr"

    # -- Output filename --
    localFeatureRomTimestamp="${featureRomTimestamp}"
    if [ "${localFeatureRomTimestamp}" == "" ]; then
      localFeatureRomTimestamp="0"
    fi

    # Build output file and lowercase the name
    dsabinOutputFile=$(printf "%s-%s-%s-%016x.dsabin" "${pci_vendor_id#0x}" "${pci_device_id#0x}" "${pci_subsystem_id#0x}" "${localFeatureRomTimestamp}")
    dsabinOutputFile="${dsabinOutputFile,,}"
    xclbinOpts+=" -o ./firmware/${dsabinOutputFile}"  

    echo "${XILINX_XRT}/bin/xclbincat ${xclbinOpts}"
    ${XILINX_XRT}/bin/xclbincat ${xclbinOpts}

    popd >/dev/null
}

dodebdev()
{
    uRel=`lsb_release -r -s`
    dir=debbuild/$dsa-$version-dev_${uRel}
    pkg_dirname=debbuild/$dsa-${version}-dev-${revision}_${uRel}
    pkgdir=$opt_pkgdir/$pkg_dirname

    # Clean the directory
    /bin/rm -rf "${pkgdir}"

    mkdir -p $pkgdir/DEBIAN

cat <<EOF > $pkgdir/DEBIAN/control

Package: $dsa-dev
Architecture: amd64
Version: $version-$revision
Priority: optional
Depends: $dsa (>= $version)
Description: Xilinx $dsa development DSA. Built on $build_date.
Maintainer: Xilinx Inc
Section: devel
EOF

    mkdir -p $pkgdir/opt/xilinx/platforms/$opt_dsa/hw
    mkdir -p $pkgdir/opt/xilinx/platforms/$opt_dsa/sw
    if [ "${license_dir}" != "" ] ; then
	if [ -d ${license_dir} ] ; then
	  mkdir -p $pkgdir/opt/xilinx/platforms/$opt_dsa/license
	  cp -f ${license_dir}/*  $pkgdir/opt/xilinx/platforms/$opt_dsa/license
	fi
    fi
    
    rsync -avz $opt_dsadir/$opt_dsa.xpfm $pkgdir/opt/xilinx/platforms/$opt_dsa/
    rsync -avz $opt_dsadir/hw/$opt_dsa.dsa $pkgdir/opt/xilinx/platforms/$opt_dsa/hw/
    rsync -avz $opt_dsadir/sw/$opt_dsa.spfm $pkgdir/opt/xilinx/platforms/$opt_dsa/sw/
    chmod -R +r $pkgdir/opt/xilinx/platforms/$opt_dsa
    chmod -R o=g $pkgdir/opt/xilinx/platforms/$opt_dsa
    dpkg-deb --build $pkgdir

    echo "================================================================"
    echo "* Please locate dep for $dsa in: $pkgdir"
    echo "================================================================"
}

dodeb()
{
    uRel=`lsb_release -r -s`
    dir=debbuild/$dsa-${version}_${uRel}
    pkg_dirname=debbuild/$dsa-${version}-${revision}_${uRel}
    pkgdir=$opt_pkgdir/$pkg_dirname

    # Clean the directory
    /bin/rm -rf "${pkgdir}"

    mkdir -p $pkgdir/DEBIAN

cat <<EOF > $pkgdir/DEBIAN/control

Package: $dsa
Architecture: all
Version: $version-$revision
Priority: optional
Depends: xrt (>= $opt_xrt)
Description: Xilinx $dsa deployment DSA. 
 This DSA depends on xrt >= $opt_xrt.
Maintainer: Xilinx Inc.
Section: devel
EOF

cat <<EOF > $pkgdir/DEBIAN/postinst
echo "${post_inst_msg} ${featureRomTimestamp}"
EOF
    chmod 755 $pkgdir/DEBIAN/postinst

    mkdir -p $pkgdir/lib/firmware/xilinx
    if [ "${license_dir}" != "" ] ; then
	if [ -d ${license_dir} ] ; then
	  mkdir -p $pkgdir/opt/xilinx/dsa/$opt_dsa/license
	  cp -f ${license_dir}/*  $pkgdir/opt/xilinx/dsa/$opt_dsa/license
	fi
    fi
   
    rsync -avz $opt_pkgdir/dsabin/firmware/ $pkgdir/lib/firmware/xilinx
    mkdir -p $pkgdir/opt/xilinx/dsa/$opt_dsa/test

    # Are there any verification tests
    if [ -d ${opt_dsadir}/test ] ; then
       rsync -avz ${opt_dsadir}/test/ $pkgdir/opt/xilinx/dsa/$opt_dsa/test
    fi

    chmod -R +r $pkgdir/opt/xilinx/dsa/$opt_dsa
    chmod -R o=g $pkgdir/opt/xilinx/dsa/$opt_dsa
    chmod -R o=g $pkgdir/lib/firmware/xilinx
    dpkg-deb --build $pkgdir

    echo "================================================================"
    echo "* Please locate dep for $dsa in: $pkgdir"
    echo "================================================================"
}

dorpmdev()
{
    dir=rpmbuild
    mkdir -p $opt_pkgdir/$dir/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

cat <<EOF > $opt_pkgdir/$dir/SPECS/$opt_dsa-dev.spec

%define _rpmfilename %%{ARCH}/%%{NAME}-%%{VERSION}.%%{ARCH}.rpm

buildroot:  %{_topdir}
summary: Xilinx $dsa development DSA
name: $dsa-dev
version: $version
release: $revision
license: apache
vendor: Xilinx Inc

requires: $dsa >= $version
%package devel
summary:  Xilinx $dsa development DSA. 

%description devel 
Xilinx $dsa development DSA. 

%description
Xilinx $dsa development DSA. Built on $build_date.

%prep

%post

%install
mkdir -p %{buildroot}/opt/xilinx/platforms/$opt_dsa/hw
mkdir -p %{buildroot}/opt/xilinx/platforms/$opt_dsa/sw
rsync -avz $opt_dsadir/$opt_dsa.xpfm %{buildroot}/opt/xilinx/platforms/$opt_dsa/
rsync -avz $opt_dsadir/hw/$opt_dsa.dsa %{buildroot}/opt/xilinx/platforms/$opt_dsa/hw/
rsync -avz $opt_dsadir/sw/$opt_dsa.spfm %{buildroot}/opt/xilinx/platforms/$opt_dsa/sw/
if [ "${license_dir}" != "" ] ; then
  if [ -d ${license_dir} ] ; then
    mkdir -p %{buildroot}/opt/xilinx/platforms/$opt_dsa/license
    cp -f ${license_dir}/*  %{buildroot}/opt/xilinx/platforms/$opt_dsa/license/
  fi
fi
chmod -R o=g %{buildroot}/opt/xilinx/platforms/$opt_dsa

%files
%defattr(-,root,root,-)
/opt/xilinx

%changelog
* $build_date Xilinx Inc - 5.1-1
  Created by script

EOF

    echo "rpmbuild --define '_topdir $opt_pkgdir/$dir' -ba $opt_pkgdir/$dir/SPECS/$opt_dsa-dev.spec"
    $dir --target=x86_64 --define '_topdir '"$opt_pkgdir/$dir" -ba $opt_pkgdir/$dir/SPECS/$opt_dsa-dev.spec
    #$dir --target=noarch --define '_topdir '"$opt_pkgdir/$dir" -ba $opt_pkgdir/$dir/SPECS/$opt_dsa-dev.spec
    echo "================================================================"
    echo "* Locate x86_64 rpm for the dsa in: $opt_pkgdir/$dir/RPMS/x86_64"
    echo "* Locate noarch rpm for the dsa in: $opt_pkgdir/$dir/RPMS/noarch"
    echo "================================================================"
}

dorpm()
{
    dir=rpmbuild
    mkdir -p $opt_pkgdir/$dir/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

cat <<EOF > $opt_pkgdir/$dir/SPECS/$opt_dsa.spec

buildroot:  %{_topdir}
summary: Xilinx $dsa deployment DSA
name: $dsa
version: $version
release: $revision
license: apache
vendor: Xilinx Inc
autoreqprov: no
requires: xrt >= $opt_xrt

%description
Xilinx $dsa deployment DSA. Built on $build_date. This DSA depends on xrt >= $opt_xrt.

%pre

%post
echo "${post_inst_msg} ${featureRomTimestamp}"

%install
mkdir -p %{buildroot}/lib/firmware/xilinx
cp $opt_pkgdir/dsabin/firmware/* %{buildroot}/lib/firmware/xilinx

if [ -d ${opt_dsadir}/test ] ; then
  mkdir -p %{buildroot}/opt/xilinx/dsa/$opt_dsa/test
  cp ${opt_dsadir}/test/* %{buildroot}/opt/xilinx/dsa/$opt_dsa/test
fi

if [ "${license_dir}" != "" ] ; then
  if [ -d ${license_dir} ] ; then
    mkdir -p %{buildroot}/opt/xilinx/dsa/$opt_dsa/license
    cp -f ${license_dir}/*  %{buildroot}/opt/xilinx/dsa/$opt_dsa/license/
  fi
fi
chmod -R o=g %{buildroot}/opt/xilinx/dsa/$opt_dsa
chmod -R o=g %{buildroot}/lib/firmware/xilinx

%files
%defattr(-,root,root,-)
/lib/firmware/xilinx
/opt/xilinx/dsa/$opt_dsa/


%changelog
* $build_date Xilinx Inc. - 5.1-1
  Created by script

EOF

    echo "rpmbuild --define '_topdir $opt_pkgdir/$dir' -ba $opt_pkgdir/$dir/SPECS/$opt_dsa.spec"
    rpmbuild --define '_topdir '"$opt_pkgdir/$dir" -ba $opt_pkgdir/$dir/SPECS/$opt_dsa.spec
    $dir --target=noarch --define '_topdir '"$opt_pkgdir/$dir" -ba $opt_pkgdir/$dir/SPECS/$opt_dsa.spec
    echo "================================================================"
    echo "* Locate x86_64 rpm for the dsa in: $opt_pkgdir/$dir/RPMS/x86_64"
    echo "* Locate noarch rpm for the dsa in: $opt_pkgdir/$dir/RPMS/noarch"
    echo "================================================================"
}

FLAVOR=`grep '^ID=' /etc/os-release | awk -F= '{print $2}'`
FLAVOR=`echo $FLAVOR | tr -d '"'`


case "$FLAVOR" in
  ("centos") docentos ;;
  ("rhel") docentos ;;
  ("ubuntu") doubuntu ;;
  (*) echo "Unsupported OS '${FLAVOR}'" && exit 1 ;;
esac

