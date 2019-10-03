#!/bin/bash

# This script creates rpm and deb packages for xsabin archives
#
# The script is assumed to run on a host or docker that has all the
# necessary rpm/deb tools installed.
#
# Examples:
#
#  Package XSA from platform dir 
#  % pkg_hw_platform.sh \
#       -xsa xilinx_vcu1525_dynamic_5_1 \
#       -xrt 2.1.0 \
#       -cl 12345678
#
#  Package XSA from specified XSA platform dir 
#  % pkg_hw_platform.sh -xsa xilinx_vcu1525_dynamic_5_1 \
#       -xsadir <directory>/xilinx_vcu1525_dynamic_5_1 \
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

opt_xsa=""
opt_xsadir=""
opt_pkgdir="/tmp/pkgxsa"
opt_cl=0
opt_dev=0
opt_deploy=0
license_dir=""

xsa_version="6.1"
build_date=`date +"%a %b %d %Y"`

usage()
{
    echo "pkg_hw_platform"
    echo
    echo "-xsa <name>                Name of xsa, e.g. xilinx-vcu1525-dynamic_5_1"
    echo "-xsadir <path>             Full path to directory with platforms"
    echo "[-xrt <version>]           Requires xrt >= <version>"
    echo "[-cl <changelist>]         Changelist for package revision"
    echo "[-pkgdir <path>]           Full path to directory used by rpm,dep,xbins (default: /tmp/pkgxsa)"
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
        -xsa)
            shift
            opt_xsa=$1
            shift
            ;;
        -license)
            shift
	    license_dir=$1
	    shift
	    ;;
        -xsadir)
            shift
            opt_xsadir=$1
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
        *)
            echo "$1 invalid argument."
            usage
            ;;
    esac
done

if [ "X$opt_xsa" == "X" ]; then
   echo "Must specify -xsa"
   usage
   exit 1
fi

if [ "X$opt_xsadir" == "X" ]; then
   echo "Must specify -xsadir"
   usage
   exit 1
fi

if [ ! -d $opt_xsadir ]; then
  echo "Specified xsa "$ssa" does not exist in '$opt_xsadir'"
  usage
  exit 1
fi

if [ "X$opt_xrt" == "X" ]; then
  echo "Must specify -xrt"
  usage
  exit 1;
fi

# get xsa, version, and revision
# Parse the platform into the basic parts
#    Syntax: <vender>_<board>_<name>_<versionMajor>_<versionMinor>
set -- `echo $opt_xsa | tr '_' ' '`
xsa="${1}-${2}-${3}"
version="${4}.${5}"
revision=$opt_cl

echo "================================================================"
echo "XSA       : $xsa"
echo "XSADIR    : $opt_xsadir"
echo "PKGDIR    : $opt_pkgdir"
echo "XRT       : $opt_xrt"
echo "VERSION   : $version"
echo "REVISION  : $revision"
echo "XILINX_XRT: $XILINX_XRT"
echo "================================================================"

# XSABIN variables
xsaFile=""
mcsPrimary=""
mcsSecondary=""
fullBitFile=""
clearBitstreamFile=""
metaDataJSONFile=""
xsaXmlFile="xsa.xml"
featureRomTimestamp="0"
featureRomUUID=""
fwScheduler=""
fwManagement=""
fwBMC=""
fwBMCMetaData=""
vbnv=""
pci_vendor_id="0x0000"
pci_device_id="0x0000"
pci_subsystem_id="0x0000"
xsabinOutputFile=""
SatelliteControllerFamily=""
CardMgmtControllerFamily=""
SchedulerFamily=""


XBMGMT=/opt/xilinx/xrt/bin/xbmgmt
post_inst_msg="XSA package installed successfully.
Please flash card manually by running below command:
sudo ${XBMGMT} flash --update --shell ${opt_xsa}"

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

recordXsaFiles()
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

readXsaMetaData()
{
  # -- Extract the xsa.xml metadata file --
  unzip -q -d . "${xsaFile}" "${xsaXmlFile}"

  while readSAX; do
    # Record the data types
    if [ "${ENTITY_NAME}" == "File" ]; then
      createEntityAttributeArray
      recordXsaFiles
    fi    

    # Record the top level XSA information, including FeatureRomTimestamp
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

      # Overwrite previous value
      featureRomTimestamp="${ENTITY_ATTRIBUTES_ARRAY[TimeSinceEpoch]}"
      featureRomUUID="${ENTITY_ATTRIBUTES_ARRAY[UUID]}"
    fi    

  done < "${xsaXmlFile}"
}

initCMCVar()
{
    fwManagement=""
    prefix=""
    if [ "${CardMgmtControllerFamily}" != "" ]; then
      if [ "${CardMgmtControllerFamily}" == "Legacy" ]; then
         fwManagement="${XILINX_XRT}/share/fw/mgmt.bin"
         return
      elif [ "${CardMgmtControllerFamily}" == "CMC-Gen1" ]; then
         fwManagement="${XILINX_XRT}/share/fw/cmc.bin"
         return
      elif [ "${CardMgmtControllerFamily}" == "CMC-Gen2" ]; then
         prefix="CmcGen2-"
      elif [ "${CardMgmtControllerFamily}" == "CMC-NoSC-Gen1" ]; then
         prefix="CmcNoSCGen1-"
      else
         echo "ERROR: Unknown card management controller family: ${CardMgmtControllerFamily}"
         exit 1
      fi
    fi

    # Looking for the CMC firmware image
    for file in ${XILINX_XRT}/share/fw/${prefix}*.bin; do
      [ -e "$file" ] || continue

      # Found "something" break it down into the basic parts
      baseFileName="${file%.*bin}"        # Remove suffix
      baseFileName="${baseFileName##*/}"  # Remove Path
      baseFileName=${baseFileName#"$prefix"}  # Remove prefix

      set -- `echo ${baseFileName} | tr '-' ' '`
      cmcImageName="${1}"
      cmcVersion="${2}"
      cmcMd5Expected="${3}"

      # Calculate the md5 checksum
      set -- $(md5sum $file)
      cmcMd5Actual="${1}"

      if [ "${cmcMd5Expected}" == "${cmcMd5Actual}" ]; then
         echo "Info: Validated ${prefix}:${baseFileName} image MD5 value"
         fwManagement="${file}"
      else
         echo "ERROR: CMC image failed MD5 varification."
         echo "       Expected: ${cmcMd5Expected}"
         echo "       Actual  : ${cmcMd5Actual}"
         echo "       File:   : $file"
         exit 1
      fi

      # We only go through this loop once
      return
    done
}

initBMCVar()
{
    prefix=""
    if [ "${SatelliteControllerFamily}" != "" ]; then
      if [ "${SatelliteControllerFamily}" == "Alveo-Gen1" ]; then
         prefix="AlveoGen1-"
      elif [ "${SatelliteControllerFamily}" == "Alveo-Gen2" ]; then
         prefix="AlveoGen2-"
      elif [ "${SatelliteControllerFamily}" == "Alveo-Gen3" ]; then
         prefix="AlveoGen3-"
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

initXsaBinEnvAndVars()
{
    # Clean out the xsabin directory
    /bin/rm -rf "${opt_pkgdir}/xsabin"
    mkdir -p "${opt_pkgdir}/xsabin"
    mkdir -p "${opt_pkgdir}/xsabin/firmware"
    cd "${opt_pkgdir}/xsabin"

    # -- Get the XSA for this platform --
    xsaFile="${opt_xsadir}/hw/${opt_xsa}.xsa"
    if [ ! -f "${xsaFile}" ]; then
       echo "Error: XSA file does not exist: ${xsaFile}"
       popd >/dev/null
       exit 1
    fi
  
    # Read the metadata from the xsa.xml file 
    readXsaMetaData
  
    # -- Extract the MCS Files --
    if [ "${mcsPrimary}" != "" ]; then
       echo "Info: Extracting MCS Primary file: ${mcsPrimary}"
       unzip -q -d . "${xsaFile}" "${mcsPrimary}"
    fi

    if [ "${mcsSecondary}" != "" ]; then
       echo "Info: Extracting MCS Secondary file: ${mcsSecondary}"
       unzip -q -d . "${xsaFile}" "${mcsSecondary}"
    fi

    # -- Extract the bitstreams --
    if [ "${fullBitFile}" != "" ]; then
       echo "Info: Extracting Full Bitstream file: ${fullBitFile}"
       unzip -q -d "./firmware" "${xsaFile}" "${fullBitFile}"
    fi

    if [ "${clearBitstreamFile}" != "" ]; then
       echo "Info: Extracting Clear Bitstream file: ${clearBitstreamFile}"
       unzip -q -d "./firmware" "${xsaFile}" "${clearBitstreamFile}"
    fi

    # -- Extract the Metadata
    # Default values
    SatelliteControllerFamily=""
    CardMgmtControllerFamily="Legacy"
    SchedulerFamily="ERT-Gen1"

    if [[ ${opt_xsa} =~ "xdma" ]]; then
      CardMgmtControllerFamily="CMC-Gen1"
      SatelliteControllerFamily="Alveo-Gen1"
    fi

    if [ "${metaDataJSONFile}" != "" ]; then
       echo "Info: Extracting Metadata file: ${metaDataJSONFile}"
       unzip -q -d "." "${xsaFile}" "${metaDataJSONFile}"

       # Brute force to obtain this data
       # See if there is a xsabin section
       set -- `cat "${metaDataJSONFile}" | python -c "import sys, json; print json.load(sys.stdin).get('xsabin','NOT_DEFINED')"`
       if [ "${1}" != "NOT_DEFINED" ]; then
          # Satellite Controller Family (MSP432)
          set -- `cat "${metaDataJSONFile}" | python -c "import sys, json; print json.load(sys.stdin)['xsabin'].get('Satellite Controller Family','NOT_DEFINED')"`
          if [ "${1}" != "NOT_DEFINED" ]; then
            SatelliteControllerFamily="${1}"
          fi

          # Card Management Controller Family
          set -- `cat "${metaDataJSONFile}" | python -c "import sys, json; print json.load(sys.stdin)['xsabin'].get('Card Management Controller Family','NOT_DEFINED')"`
          if [ "${1}" != "NOT_DEFINED" ]; then
            CardMgmtControllerFamily="${1}"
          fi

          # Scheduler Family
          set -- `cat "${metaDataJSONFile}" | python -c "import sys, json; print json.load(sys.stdin)['xsabin'].get('Scheduler Family','NOT_DEFINED')"`
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
    initCMCVar

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
     doxsabin
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
     doxsabin
     dodeb
 fi
}

doxsabin()
{
    pushd $opt_pkgdir > /dev/null
    echo "Creating xsabin for: ${opt_xsa}"

    initXsaBinEnvAndVars

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

    # -- FeatureRom UUID --
    if [ "${featureRomUUID}" != "" ]; then
       xclbinOpts+=" --key-value SYS:FeatureRomUUID:${featureRomUUID}"
    else
       echo "Warning: Missing featureRomUUID"
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
    xsabinOutputFile=$(printf "%s-%s-%s-%016x.xsabin" "${pci_vendor_id#0x}" "${pci_device_id#0x}" "${pci_subsystem_id#0x}" "${localFeatureRomTimestamp}")
    xsabinOutputFile="${xsabinOutputFile,,}"
    xclbinOpts+=" --output ./firmware/${xsabinOutputFile}"    


    echo "${XILINX_XRT}/bin/xclbinutil ${xclbinOpts}"
    ${XILINX_XRT}/bin/xclbinutil ${xclbinOpts}
    retval=$?

    popd >/dev/null

    if [ $retval -ne 0 ]; then
       echo "ERROR: xclbinutil failed.  Exiting."
       exit
    fi
}

dodebdev()
{
    uRel=`lsb_release -r -s`
    dir=debbuild/$xsa-$version-dev_${uRel}
    pkg_dirname=debbuild/$xsa-dev-${version}-${revision}_${uRel}
    pkgdir=$opt_pkgdir/$pkg_dirname

    # Clean the directory
    /bin/rm -rf "${pkgdir}"

    mkdir -p $pkgdir/DEBIAN

cat <<EOF > $pkgdir/DEBIAN/control
Package: $xsa-dev
Architecture: amd64
Version: $version-$revision
Priority: optional
Depends: $xsa (>= $version)
Description: Xilinx $xsa development XSA. Built on $build_date.
Maintainer: Xilinx Inc
Section: devel
EOF

    mkdir -p $pkgdir/opt/xilinx/platforms/$opt_xsa/hw
    mkdir -p $pkgdir/opt/xilinx/platforms/$opt_xsa/sw
    if [ "${license_dir}" != "" ] ; then
	if [ -d ${license_dir} ] ; then
	  mkdir -p $pkgdir/opt/xilinx/platforms/$opt_xsa/license
	  cp -f ${license_dir}/*  $pkgdir/opt/xilinx/platforms/$opt_xsa/license
	fi
    fi
    
    rsync -avz $opt_xsadir/$opt_xsa.xpfm $pkgdir/opt/xilinx/platforms/$opt_xsa/
    rsync -avz $opt_xsadir/hw/$opt_xsa.xsa $pkgdir/opt/xilinx/platforms/$opt_xsa/hw/
    rsync -avz $opt_xsadir/sw/$opt_xsa.spfm $pkgdir/opt/xilinx/platforms/$opt_xsa/sw/

    # Support the ERT directory
    if [ -d ${opt_xsadir}/sw/ert ] ; then
       mkdir -p $pkgdir/opt/xilinx/platforms/$opt_xsa/sw/ert
       rsync -avz ${opt_xsadir}/sw/ert/ $pkgdir/opt/xilinx/platforms/$opt_xsa/sw/ert
    fi

    chmod -R +r $pkgdir/opt/xilinx/platforms/$opt_xsa
    chmod -R o=g $pkgdir/opt/xilinx/platforms/$opt_xsa
    dpkg-deb --build $pkgdir

    echo "================================================================"
    echo "* Debian package for $xsa generated in: $pkgdir"
    echo "================================================================"
}

dodeb()
{
    uRel=`lsb_release -r -s`
    dir=debbuild/$xsa-${version}_${uRel}
    pkg_dirname=debbuild/$xsa-${version}-${revision}_${uRel}
    pkgdir=$opt_pkgdir/$pkg_dirname

    # Clean the directory
    /bin/rm -rf "${pkgdir}"

    mkdir -p $pkgdir/DEBIAN

cat <<EOF > $pkgdir/DEBIAN/control
Package: $xsa
Architecture: all
Version: $version-$revision
Priority: optional
Depends: xrt (>= $opt_xrt)
Description: Xilinx $xsa deployment XSA. 
 This XSA depends on xrt >= $opt_xrt.
Maintainer: Xilinx Inc.
Section: devel
EOF

cat <<EOF > $pkgdir/DEBIAN/postinst
echo "${post_inst_msg}"
EOF
    chmod 755 $pkgdir/DEBIAN/postinst

    mkdir -p $pkgdir/lib/firmware/xilinx
    if [ "${license_dir}" != "" ] ; then
	if [ -d ${license_dir} ] ; then
	  mkdir -p $pkgdir/opt/xilinx/xsa/$opt_xsa/license
	  cp -f ${license_dir}/*  $pkgdir/opt/xilinx/xsa/$opt_xsa/license
	fi
    fi
   
    rsync -avz $opt_pkgdir/xsabin/firmware/ $pkgdir/lib/firmware/xilinx
    mkdir -p $pkgdir/opt/xilinx/xsa/$opt_xsa/test

    # Are there any verification tests
    if [ -d ${opt_xsadir}/test ] ; then
       rsync -avz ${opt_xsadir}/test/ $pkgdir/opt/xilinx/xsa/$opt_xsa/test
    fi

    chmod -R +r $pkgdir/opt/xilinx/xsa/$opt_xsa
    chmod -R o=g $pkgdir/opt/xilinx/xsa/$opt_xsa
    chmod -R o=g $pkgdir/lib/firmware/xilinx
    dpkg-deb --build $pkgdir

    echo "================================================================"
    echo "* Debian package for $xsa generated in: $pkgdir"
    echo "================================================================"
}

dorpmdev()
{
    dir=rpmbuild
    mkdir -p $opt_pkgdir/$dir/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

cat <<EOF > $opt_pkgdir/$dir/SPECS/$opt_xsa-dev.spec

%define _rpmfilename %%{ARCH}/%%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm

buildroot:  %{_topdir}
summary: Xilinx $xsa development XSA
name: $xsa-dev
version: $version
release: $revision
license: Xilinx EULA
vendor: Xilinx Inc

requires: $xsa >= $version
%package devel
summary:  Xilinx $xsa development XSA. 

%description devel 
Xilinx $xsa development XSA. 

%description
Xilinx $xsa development XSA. Built on $build_date.

%prep

%post

%install
mkdir -p %{buildroot}/opt/xilinx/platforms/$opt_xsa/hw
mkdir -p %{buildroot}/opt/xilinx/platforms/$opt_xsa/sw
rsync -avz $opt_xsadir/$opt_xsa.xpfm %{buildroot}/opt/xilinx/platforms/$opt_xsa/
rsync -avz $opt_xsadir/hw/$opt_xsa.xsa %{buildroot}/opt/xilinx/platforms/$opt_xsa/hw/
rsync -avz $opt_xsadir/sw/$opt_xsa.spfm %{buildroot}/opt/xilinx/platforms/$opt_xsa/sw/

# Support the ERT directory
  if [ -d ${opt_xsadir}/sw/ert ] ; then
    mkdir -p %{buildroot}/opt/xilinx/platforms/$opt_xsa/sw/ert
    rsync -avz ${opt_xsadir}/sw/ert/ %{buildroot}/opt/xilinx/platforms/$opt_xsa/sw/ert
fi

if [ "${license_dir}" != "" ] ; then
  if [ -d ${license_dir} ] ; then
    mkdir -p %{buildroot}/opt/xilinx/platforms/$opt_xsa/license
    cp -f ${license_dir}/*  %{buildroot}/opt/xilinx/platforms/$opt_xsa/license/
  fi
fi
chmod -R o=g %{buildroot}/opt/xilinx/platforms/$opt_xsa

%files
%defattr(-,root,root,-)
/opt/xilinx/platforms/$opt_xsa/

%changelog
* $build_date Xilinx Inc - 5.1-1
  Created by script

EOF

    echo "rpmbuild --define '_topdir $opt_pkgdir/$dir' -ba $opt_pkgdir/$dir/SPECS/$opt_xsa-dev.spec"
    $dir --target=x86_64 --define '_topdir '"$opt_pkgdir/$dir" -ba $opt_pkgdir/$dir/SPECS/$opt_xsa-dev.spec
    #$dir --target=noarch --define '_topdir '"$opt_pkgdir/$dir" -ba $opt_pkgdir/$dir/SPECS/$opt_xsa-dev.spec
    echo "================================================================"
    echo "* Locate x86_64 rpm for the xsa in: $opt_pkgdir/$dir/RPMS/x86_64"
    echo "* Locate noarch rpm for the xsa in: $opt_pkgdir/$dir/RPMS/noarch"
    echo "================================================================"
}

dorpm()
{
    dir=rpmbuild
    mkdir -p $opt_pkgdir/$dir/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

cat <<EOF > $opt_pkgdir/$dir/SPECS/$opt_xsa.spec

%define _rpmfilename %%{ARCH}/%%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm

buildroot:  %{_topdir}
summary: Xilinx $xsa deployment XSA
name: $xsa
version: $version
release: $revision
license: Apache
vendor: Xilinx Inc
autoreqprov: no
requires: xrt >= $opt_xrt

%description
Xilinx $xsa deployment XSA. Built on $build_date. This XSA depends on xrt >= $opt_xrt.

%pre

%post
echo "${post_inst_msg}"

%install
mkdir -p %{buildroot}/lib/firmware/xilinx
cp $opt_pkgdir/xsabin/firmware/* %{buildroot}/lib/firmware/xilinx

mkdir -p %{buildroot}/opt/xilinx/xsa/$opt_xsa

if [ -d ${opt_xsadir}/test ] ; then
  mkdir -p %{buildroot}/opt/xilinx/xsa/$opt_xsa/test
  cp ${opt_xsadir}/test/* %{buildroot}/opt/xilinx/xsa/$opt_xsa/test
fi

if [ "${license_dir}" != "" ] ; then
  if [ -d ${license_dir} ] ; then
    mkdir -p %{buildroot}/opt/xilinx/xsa/$opt_xsa/license
    cp -f ${license_dir}/*  %{buildroot}/opt/xilinx/xsa/$opt_xsa/license/
  fi
fi
chmod -R o=g %{buildroot}/opt/xilinx/xsa/$opt_xsa
chmod -R o=g %{buildroot}/lib/firmware/xilinx

%files
%defattr(-,root,root,-)
/lib/firmware/xilinx
/opt/xilinx/xsa/$opt_xsa/


%changelog
* $build_date Xilinx Inc. - 5.1-1
  Created by script

EOF

    echo "rpmbuild --define '_topdir $opt_pkgdir/$dir' -ba $opt_pkgdir/$dir/SPECS/$opt_xsa.spec"
    rpmbuild --define '_topdir '"$opt_pkgdir/$dir" -ba $opt_pkgdir/$dir/SPECS/$opt_xsa.spec
    $dir --target=noarch --define '_topdir '"$opt_pkgdir/$dir" -ba $opt_pkgdir/$dir/SPECS/$opt_xsa.spec
    echo "================================================================"
    echo "* Locate x86_64 rpm for the xsa in: $opt_pkgdir/$dir/RPMS/x86_64"
    echo "* Locate noarch rpm for the xsa in: $opt_pkgdir/$dir/RPMS/noarch"
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

