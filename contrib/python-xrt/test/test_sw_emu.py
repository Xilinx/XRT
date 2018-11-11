import os
from util import *
from xrt.core import sw_emu_hal as hal

def test_sw_emu():
    print('Initializing ...')
    target_device = 'xilinx_u200_xdma_201830_1'
    env = RuntimeEnv('sw_emu', target_device)
    env.setup()
    device_cnt = hal.probe()
    print('INFO: found ' + str(device_cnt) + ' device')
    assert device_cnt == 1
    device_id = 'sw_emu_test_device'
    verbosity_level = 'info'
    hal.open(0, device_id, verbosity_level)
    device_info = hal.info(device_id)
    device_name = device_info['name']
    print('INFO: device name => ' + device_name)
    assert device_name == target_device
    hal.lock(device_id)

    hal.unlock(device_id)
    # buffer_type = "device_ram"
    # array_size = 10
    # byte_size = array_size * 4
    # buffer_a_handle = hal.allocate_buffer(device_id, buffer_type, 0, byte_size)
    # print('INFO: buffer a handle => ' + buffer_a_handle)
    env.cleanup()