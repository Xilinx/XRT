#ifndef __ERROR_REPORT__
#define __ERROR_REPORT__

#include <stdexcept>

using namespace std;

void check_reclock_device_error(int error_code);

void check_lock_device_error(int error_code);

void check_unlock_device_error(int error_code);

void check_upgrade_frimware_bpi_error(int error_code);

void check_get_device_info_error(int error_code);

void check_get_device_usage_error(int error_code);

void check_get_device_error_error(int error_code);

#endif