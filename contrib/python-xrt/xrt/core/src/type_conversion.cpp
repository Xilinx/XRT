#include "type_conversion.h"

string generate_log_filename(string &device_name)
{
	return device_name + "_activity.log";
}

xclVerbosityLevel convert_verbosity_level(string &level)
{
	xclVerbosityLevel res;
	if (level == "quiet")
	{
		res = xclVerbosityLevel::XCL_QUIET;
	}
	else if (level == "info")
	{
		res = xclVerbosityLevel::XCL_INFO;
	}
	else if (level == "warn")
	{
		res = xclVerbosityLevel::XCL_WARN;
	}
	else if (level == "error")
	{
		res = xclVerbosityLevel::XCL_ERROR;
	}
	else
	{
		throw runtime_error("Unkown verbosity level argument. Available arguments are quiet, info, warn and error");
	}
	return res;
}

xclResetKind convert_reset_kind(string &type)
{
	xclResetKind res;
	if (type == "kernel")
	{
		res = xclResetKind::XCL_RESET_KERNEL;
	}
	else if (type == "full")
	{
		res = xclResetKind::XCL_RESET_FULL;
	}
	else
	{
		throw runtime_error("Unknown reset type. Available types are kernel and full");
	}
	return res;
}

py::dict convert_device_info(xclDeviceInfo2 *info)
{
	py::dict res;
	std::cout << "Fetching device information ..." << std::endl;
	res["magic"] = info->mMagic;
	res["name"] = string(info->mName);
	res["major_version"] = info->mHALMajorVersion;
	res["minor_version"] = info->mHALMinorVersion;
	res["vendor_id"] = info->mVendorId;
	res["device_id"] = info->mDeviceId;
	res["system_id"] = info->mSubsystemId;
	res["subsystem_vendor_id"] = info->mSubsystemVendorId;
	res["device_version"] = info->mDeviceVersion;
	res["DDR_size"] = info->mDDRSize;
	res["data_alignment"] = info->mDataAlignment;
	res["DDR_free_size"] = info->mDDRFreeSize;
	res["min_transfer_size"] = info->mMinTransferSize;
	res["DDR_bank_count"] = info->mDDRBankCount;
	res["ocl_frequency_0"] = info->mOCLFrequency[0];
	res["ocl_frequency_1"] = info->mOCLFrequency[1];
	res["ocl_frequency_2"] = info->mOCLFrequency[2];
	res["ocl_frequency_3"] = info->mOCLFrequency[3];
	res["pcie_link_width"] = info->mPCIeLinkWidth;
	res["pcie_link_speed"] = info->mPCIeLinkSpeed;
	res["dma_threads"] = info->mDMAThreads;
	res["on_chip_temperature"] = info->mOnChipTemp;
	res["fan_temperature"] = info->mFanTemp;
	res["vint"] = info->mVInt;
	res["vaux"] = info->mVAux;
	res["vbram"] = info->mVBram;
	res["current"] = info->mCurrent;
	res["num_clock"] = info->mNumClocks;
	res["fan_speed"] = info->mFanSpeed;
	res["mig_calibration"] = info->mMigCalib;
	res["xmc_version"] = info->mXMCVersion;
	res["12v_pex"] = info->m12VPex;
	res["12v_aux"] = info->m12VAux;
	res["pex_current"] = info->mPexCurr;
	res["aux_current"] = info->mAuxCurr;
	res["fan_rpm"] = info->mFanRpm;
	res["dimm_temp_0"] = info->mDimmTemp[0];
	res["dimm_temp_1"] = info->mDimmTemp[1];
	res["dimm_temp_2"] = info->mDimmTemp[2];
	res["dimm_temp_3"] = info->mDimmTemp[3];
	res["se98_temp_0"] = info->mSE98Temp[0];
	res["se98_temp_1"] = info->mSE98Temp[1];
	res["se98_temp_2"] = info->mSE98Temp[2];
	res["se98_temp_3"] = info->mSE98Temp[3];
	res["3v3_pex"] = info->m3v3Pex;
	res["3v3_aux"] = info->m3v3Aux;
	res["DDR_vpp_bottom"] = info->mDDRVppBottom;
	res["DDR_vpp_top"] = info->mDDRVppTop;
	res["system_5v5"] = info->mSys5v5;
	res["1v2_top"] = info->m1v2Top;
	res["1v8_top"] = info->m1v8Top;
	res["0v85"] = info->m0v85;
	res["management_0v9"] = info->mMgt0v9;
	res["12vSW"] = info->m12vSW;
	res["management_vtt"] = info->mMgtVtt;
	res["1v2_bottom"] = info->m1v2Bottom;
	res["driver_version"] = info->mDriverVersion;
	res["pcie_slot"] = info->mPciSlot;
	res["is_xpr"] = info->mIsXPR;
	res["time_stamp"] = info->mTimeStamp;
	res["fpga"] = string(info->mFpga);
	res["pcie_link_max_width"] = info->mPCIeLinkWidthMax;
	res["pcie_link_max_speed"] = info->mPCIeLinkSpeedMax;
	return res;
}

py::dict convert_usage_info(xclDeviceUsage *info)
{
	py::dict res;
	res["host_to_card_channel_0"] = (unsigned long)info->h2c[0];
	res["host_to_card_channel_1"] = (unsigned long)info->h2c[1];
	res["card_to_host_channel_0"] = (unsigned long)info->c2h[0];
	res["card_to_host_channel_1"] = (unsigned long)info->c2h[1];
	res["DDR_Bank0_memory_used"] = (unsigned long)info->ddrMemUsed[0];
	res["DDR_Bank1_memory_used"] = (unsigned long)info->ddrMemUsed[1];
	res["DDR_Bank2_memory_used"] = (unsigned long)info->ddrMemUsed[2];
	res["DDR_Bank3_memory_used"] = (unsigned long)info->ddrMemUsed[3];
	res["DDR_Bank0_buffer_allocated"] = (unsigned)info->ddrBOAllocated[0];
	res["DDR_Bank1_buffer_allocated"] = (unsigned)info->ddrBOAllocated[1];
	res["DDR_Bank2_buffer_allocated"] = (unsigned)info->ddrBOAllocated[2];
	res["DDR_Bank3_buffer_allocated"] = (unsigned)info->ddrBOAllocated[3];
	res["total_context_count"] = (unsigned)info->totalContexts;
	res["xclbin_id_0"] = (unsigned long)info->xclbinId[0];
	res["xclbin_id_1"] = (unsigned long)info->xclbinId[1];
	res["xclbin_id_2"] = (unsigned long)info->xclbinId[2];
	res["xclbin_id_3"] = (unsigned long)info->xclbinId[3];
	res["dma_channel_count"] = (unsigned)info->dma_channel_cnt;
	res["mm_channel_count"] = (unsigned)info->mm_channel_cnt;
	res["memory_size_0"] = (unsigned long)info->memSize[0];
	res["memory_size_1"] = (unsigned long)info->memSize[1];
	res["memory_size_2"] = (unsigned long)info->memSize[2];
	res["memory_size_3"] = (unsigned long)info->memSize[3];
	return res;
}

py::dict convert_error_info(xclErrorStatus *info)
{
	py::dict res;
	res["firewall_count"] = (unsigned)info->mNumFirewalls;
	res["firewall_level"] = (unsigned)info->mFirewallLevel;
	for (int i = 0; i < 8; ++i)
	{
		py::dict axi_error;
		axi_error["firewall_time"] = (unsigned long)info->mAXIErrorStatus[i].mErrFirewallTime;
		axi_error["firewall_status"] = (unsigned)info->mAXIErrorStatus[i].mErrFirewallStatus;
		axi_error["firewall_type"] = (int)info->mAXIErrorStatus[i].mErrFirewallID;
		res["axi_error_" + to_string(i)] = axi_error;
	}
	py::dict pcie_error;
	pcie_error["device_status"] = (unsigned)info->mPCIErrorStatus.mDeviceStatus;
	pcie_error["uncorrelated_error_status"] = (unsigned)info->mPCIErrorStatus.mUncorrErrStatus;
	pcie_error["correlated_error_status"] = (unsigned)info->mPCIErrorStatus.mCorrErrStatus;
	res["pcie_error"] = pcie_error;
	return res;
}

std::string convert_ip_type(IP_TYPE type)
{
	if (type == IP_TYPE::IP_MB)
	{
		return "mb";
	}
	if (type == IP_TYPE::IP_KERNEL)
	{
		return "kernel";
	}
	if (type == IP_TYPE::IP_DNASC)
	{
		return "dnasc";
	}
	return "unknown";
}

std::string convert_debug_ip_layout(DEBUG_IP_TYPE type)
{
	if (type == DEBUG_IP_TYPE::UNDEFINED)
	{
		return "undefined";
	}
	if (type == DEBUG_IP_TYPE::LAPC)
	{
		return "lapc";
	}
	if (type == DEBUG_IP_TYPE::ILA)
	{
		return "ila";
	}
	if (type == DEBUG_IP_TYPE::AXI_MM_MONITOR)
	{
		return "axi mm monitor";
	}
	if (type == DEBUG_IP_TYPE::AXI_TRACE_FUNNEL)
	{
		return "axi trace funnel";
	}
	if (type == DEBUG_IP_TYPE::AXI_MONITOR_FIFO_LITE)
	{
		return "axi monitor fifo lite";
	}
	if (type == DEBUG_IP_TYPE::AXI_MONITOR_FIFO_FULL)
	{
		return "axi monitor fifo full";
	}
	if (type == DEBUG_IP_TYPE::ACCEL_MONITOR)
	{
		return "accel monitor";
	}
	return "unknown";
}

xclBOKind convert_buffer_type(string type)
{
	if (type == "shared_virtual")
	{
		return xclBOKind::XCL_BO_SHARED_VIRTUAL;
	}
	if (type == "shared_physical")
	{
		return xclBOKind::XCL_BO_SHARED_PHYSICAL;
	}
	if (type == "mirrored_virtual")
	{
		return xclBOKind::XCL_BO_MIRRORED_VIRTUAL;
	}
	if (type == "device_ram")
	{
		return xclBOKind::XCL_BO_DEVICE_RAM;
	}
	if (type == "device_bram")
	{
		return xclBOKind::XCL_BO_DEVICE_BRAM;
	}
	if (type == "device_preallocated_bram")
	{
		return xclBOKind::XCL_BO_DEVICE_PREALLOCATED_BRAM;
	}
	throw runtime_error("Unknown buffer type " + type);
}

np::dtype convert_buffer_data_type(string type)
{
	if (type == "int")
	{
		return np::dtype::get_builtin<int>();
	}
	if (type == "float")
	{
		return np::dtype::get_builtin<float>();
	}
	throw runtime_error("Unknown buffer data type " + type);
}

ert_cmd_state convert_ert_command_state(py::object state_object) {
	string state = py::extract<string>(state_object);
	if (state == "new") {
		return ert_cmd_state::ERT_CMD_STATE_NEW;
	} else if (state == "queued") {
		return ert_cmd_state::ERT_CMD_STATE_QUEUED;
	} else if (state == "running") {
		return ert_cmd_state::ERT_CMD_STATE_RUNNING;
	} else if (state == "completed") {
		return ert_cmd_state::ERT_CMD_STATE_COMPLETED;
	} else if (state == "error") {
		return ert_cmd_state::ERT_CMD_STATE_ERROR;
	} else if (state == "abort") {
		return ert_cmd_state::ERT_CMD_STATE_ABORT;
	} else {
		throw runtime_error("Unknown ERT command state. Valid options are: new, queued, running, completed, error and abort");
	}
}

ert_cmd_opcode convert_ert_command_opcode(py::object opcode_object) {
	string opcode = py::extract<string>(opcode_object);
	if (opcode == "start_compute_unit") {
		return ert_cmd_opcode::ERT_START_CU;
	} else if (opcode == "start_kernel") {
		return ert_cmd_opcode::ERT_START_KERNEL;
	} else if (opcode == "configure") {
		return ert_cmd_opcode::ERT_CONFIGURE;
	} else if (opcode == "stop") {
		return ert_cmd_opcode::ERT_STOP;
	} else if (opcode == "abort") {
		return ert_cmd_opcode::ERT_ABORT;
	} else if (opcode == "write") {
		return ert_cmd_opcode::ERT_WRITE;
	} else {
		throw runtime_error("Unknown ERT command opcode. Valid opcodes are: start_compute_unit, start_kernel, configure, stop, abort and write");
	}
}

xclBOSyncDirection convert_sync_buffer_type(string type) {
	if (type == "host_to_device") {
		return xclBOSyncDirection::XCL_BO_SYNC_BO_TO_DEVICE;
	} else if (type == "device_to_host") {
		return xclBOSyncDirection::XCL_BO_SYNC_BO_FROM_DEVICE;
	} else {
		throw runtime_error("Unkown sync buffer type. Valid types are: device_to_host and host_to_device");
	}
}

xclAddressSpace convert_register_domain(string domain) {
	if (domain == "absolute") {
		return xclAddressSpace::XCL_ADDR_SPACE_DEVICE_FLAT;
	} else if (domain == "DDR") {
		return xclAddressSpace::XCL_ADDR_SPACE_DEVICE_RAM;
	} else if (domain == "control") {
		return xclAddressSpace::XCL_ADDR_KERNEL_CTRL;
	} else if (domain == "monitor") {
		return xclAddressSpace::XCL_ADDR_SPACE_DEVICE_PERFMON;
	} else if (domain == "checker") {
		return xclAddressSpace::XCL_ADDR_SPACE_DEVICE_CHECKER;
	} else if (domain == "max") {
		return xclAddressSpace::XCL_ADDR_SPACE_MAX;
	} else {
		throw runtime_error("Unknown register domain. Valid domains are: absolute, DDR, control, monitor, checker and max");
	}
}

bool is_64bit_arch(vector<unsigned long>& kernel_args) {
	unsigned num_args = kernel_args.size();
	unsigned long threshold = 0xFFFFFFFF;
	bool is_64bit = false;
	for (unsigned arg_idx = 0; arg_idx < num_args; ++arg_idx) {
		if (kernel_args[arg_idx] > threshold) {
			is_64bit = true;
			break;
		}
	}
	return is_64bit;
}