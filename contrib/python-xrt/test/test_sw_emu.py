import os
from util import *
from xrt.core import sw_emu_hal as hal

def test_probe():
    env = RuntimeEnv('sw_emu', 'xilinx_u200_xdma_201830_1')
    env.setup()
    device_cnt = hal.probe()
    print('INFO: found ' + str(device_cnt) + ' device')
    assert device_cnt == 1
    device_id = 'sw_emu_test_device'
    verbosity_level = 'info'
    hal.open(0, device_id, verbosity_level)
    device_info = hal.info(device_id)
    print(device_info)
    env.cleanup()