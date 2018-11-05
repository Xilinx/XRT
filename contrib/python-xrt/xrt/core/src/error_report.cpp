#include "error_report.h"

void check_reclock_device_error(int error_code)
{
	if (error_code == 0)
	{
		return;
	}
	else
	{
		throw runtime_error("Reclock device failed");
	}
}

void check_lock_device_error(int error_code)
{
	if (error_code == 0)
	{
		return;
	}
	else
	{
		throw runtime_error("Lock device failed");
	}
}

void check_unlock_device_error(int error_code)
{
	if (error_code == 0)
	{
		return;
	}
	else
	{
		throw runtime_error("Unlock device failed");
	}
}

void check_upgrade_frimware_bpi_error(int error_code)
{
	if (error_code == 0)
	{
		return;
	}
	else
	{
		throw runtime_error("Upgrade firmware BPI failed");
	}
}

void check_get_device_info_error(int error_code)
{
	if (error_code == 0)
	{
		return;
	}
	else
	{
		throw runtime_error("Reading device info failed");
	}
}

void check_get_device_usage_error(int error_code)
{
	if (error_code == 0)
	{
		return;
	}
	else
	{
		throw runtime_error("Reading device usage failed");
	}
}

void check_get_device_error_error(int error_code)
{
	if (error_code == 0)
	{
		return;
	}
	else
	{
		throw runtime_error("Reading device error failed");
	}
}