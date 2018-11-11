import os
import pytest
from util import RuntimeEnv
from xrt.core import sw_emu_hal as hal

target_device = 'xilinx_u200_xdma_201830_1'
device_id = 'sw_emu_test_device'
verbosity_level = 'info'
env = RuntimeEnv('sw_emu', target_device)
device_cnt = 0

def test_sw_emu_setup_environment():
    env.setup()

def test_sw_emu_probe():
    device_cnt = hal.probe()
    print('INFO: found ' + str(device_cnt) + ' device')
    assert device_cnt == 1

def test_sw_emu_open_device():
    hal.open(0, device_id, verbosity_level)

def test_sw_emu_get_device_info():
    device_info = hal.info(device_id)
    device_name = device_info['name']
    print('INFO: device name => ' + device_name)
    assert device_name == target_device

def test_sw_emu_allocate_data_buffer_for_vadd():
    buffer_type = "device_ram"
    array_size = 10
    byte_size = array_size * 4
    buffer_a_handle = hal.allocate_buffer(device_id, buffer_type, 0, byte_size)
    print('INFO: buffer a handle => ' + str(buffer_a_handle))
    assert buffer_a_handle >= 0
    buffer_b_handle = hal.allocate_buffer(device_id, buffer_type, 0, byte_size)
    print('INFO: buffer a handle => ' + str(buffer_b_handle))
    assert buffer_b_handle >= 0

def test_sw_emu_write_buffer_for_vadd():
    pass

def test_sw_emu_allocate_command_buffer_for_vadd():
    pass

def test_sw_emu_close_device():
    hal.close(device_id)

def test_sw_emu_cleanup_environment():
    env.cleanup()