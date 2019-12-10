#!/usr/bin/env bash
####################################################################
#                               Information
####################################################################
#  Background: When downloading the bitstream to the board using XRT,
#  XRT saves the process id that instantiates the download.
#  The process id is said to "lock the board". If the process is
#  not terminated (or becomes a zombie) process, then a new bitstream
#  cannot be downloaded to the board.
#
##  Step-by-step guide
#  The following steps describe how to determine the last process ID
#  that locked the FPGA board.
#
#    1) Run the script provided on this page
#         % ./processCardLock.sh
#         Validating Arguments... passed!
#         Checking prerequisites... passed!
#         Last process to lock the card is 36495.
#         Process 36495 belongs to jornt.
#         Process 36495 has been running for 08:57:55.
#  Note: The script uses the output from the dmesg command. If the XRT
#  drivers change the output format outputted by dmesg, this script
#  needs to be updated. Currently, this script works for machines
#  containing only one device.
####################################################################

#parameters
ZERO=0
ONE=1
FAILED_STATUS=${ONE}
SUCCESSFUL_STATUS=${ZERO}
TRUE=${ONE}
FALSE=${ZERO}
DEBUG=${FALSE}

# USAGE
function usage
{
	local rtn=${SUCCESSFUL_EXIT_STATUS}
	echo "usage: $0 [-h]"
	echo "   ";
	echo "   This script returns the last process that locked the FPGA to download a bitstream."
	echo "   ";
	echo "  -h | --help              : This message";
	return ${rtn}
}

# Execute
function execute
{
	local rtn=${SUCCESSFUL_EXIT_STATUS}
	# Get process ID
	pid=`dmesg | grep "icap_lock_bitstream" | cut -f 6 -d " " | tail -n1`
	if [ ! -z "${pid}" ] 
	then
		echo "Last process to lock the card is ${pid}".
		exists=`ps -p ${pid}`
		# Does process exists/Is running
		if [ "$?" -eq "0" ]
		then
			# Get UID of Process
			uid=`ps -o user= -p ${pid}`
			if [ ! -z "${uid}" ] 
			then
				echo "Process ${pid} belongs to ${uid}".
			fi
			# Get Runtime of Process
			time=`ps -o etime= -p ${pid}`
			if [ ! -z "${time}" ] 
			then
				echo "Process ${pid} has been running for ${time}".
			fi
		# Not running
		else
			echo "Process is not running."
		fi
	# No Process found.
	else
		echo "No Process ID found"
	fi
	return ${rtn}
}

# Prereq
function prereq
{
	local rtn=${SUCCESSFUL_EXIT_STATUS}
	echo -ne "Checking prerequisites... "
	echo "passed!"
	return ${rtn}
}

# Validate Arguments
function validate_args
{
	local rtn=${SUCCESSFUL_EXIT_STATUS}
	echo -ne "Validating Arguments... "
	echo "passed!"
	return ${rtn}
}

# Parse Arguments
function parse_args
{
	local rtn=${SUCCESSFUL_EXIT_STATUS}
	# positional args
	args=()

	# named args
	while [ "$1" != "" ]; do
		case "$1" in
			-h | --help )           usage
				                exit
				                ;;
			* )                     usage
				                exit 1
		esac
		shift
	done
	return ${rtn}
}

# Main
function main
{
	local rtn=${SUCCESSFUL_EXIT_STATUS}
	parse_args "$@"
	validate_args
	prereq
	execute
	return ${rtn}
}

main "$@";
