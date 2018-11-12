import os
import pytest
import numpy as np
from util import RuntimeEnv
from xrt.core import hw_emu_hal as hal

target_device = 'xilinx_u200_xdma_201830_1'
device_id = 'hw_emu_test_device'
verbosity_level = 'info'
env = RuntimeEnv('hw_emu', target_device)
vadd_array_size = 10
vadd_byte_size = vadd_array_size * 4

@pytest.fixture(scope="module")
def global_variables():
    variable_set = {
        'vadd_buffer_a_handle': -1,
        'vadd_buffer_b_handle': -1,
        'vadd_buffer_res_handle': -1,
        'vadd_ert_command_buffer_handle': -1,
        'vadd_buffer_a_physical_addr': -1,
        'vadd_buffer_b_physical_addr': -1,
        'kernel_vadd_address': -1,
        'device_cnt': 0,
        'vadd_array_a': np.array([i for i in range(vadd_array_size)]).astype(np.int32),
        'vadd_array_b': np.array([i for i in range(vadd_array_size)]).astype(np.int32),
        'vadd_array_res': np.array([0 for i in range(vadd_array_size)]).astype(np.int32)
    }
    return variable_set

def test_hw_emu_setup_environment():
    env.setup()

def test_hw_emu_probe(global_variables):
    global_variables['device_cnt'] = hal.probe()
    print('INFO: found ' + str(global_variables['device_cnt']) + ' device')
    assert global_variables['device_cnt'] == 1

def test_hw_emu_open_device():
    hal.open(0, device_id, verbosity_level)

def test_hw_emu_get_device_info():
    device_info = hal.info(device_id)
    device_name = device_info['name']
    print('INFO: device name => ' + device_name)
    assert device_name == target_device

def test_hw_emu_lock_device():
    hal.lock(device_id)

def test_hw_emu_load_vadd_bitstream(global_variables):
    xclbin_path = env.get_bitstream_path('vadd')
    xclbin_info = hal.load(device_id, xclbin_path)
    global_variables['kernel_vadd_address'] = xclbin_info['ip_layout']['krnl_vadd:krnl_vadd_1']['address']
    assert global_variables['kernel_vadd_address'] >= 0

def test_hw_emu_unlock_device():
    hal.unlock(device_id)

def test_hw_emu_allocate_data_buffer_for_vadd(global_variables):
    buffer_type = "device_ram"
    global_variables['vadd_buffer_a_handle'] = hal.allocate_buffer(device_id, buffer_type, 0, vadd_byte_size)
    print('INFO: vadd_buffer_a_handle => ' + str(global_variables['vadd_buffer_a_handle']))
    assert global_variables['vadd_buffer_a_handle'] >= 0
    global_variables['vadd_buffer_b_handle'] = hal.allocate_buffer(device_id, buffer_type, 0, vadd_byte_size)
    print('INFO: vadd_buffer_b_handle => ' + str(global_variables['vadd_buffer_b_handle']))
    assert global_variables['vadd_buffer_b_handle'] >= 0
    global_variables['vadd_buffer_res_handle'] = hal.allocate_buffer(device_id, buffer_type, 0, vadd_byte_size)
    print('INFO: vadd_buffer_res_handle => ' + str(global_variables['vadd_buffer_res_handle']))
    assert global_variables['vadd_buffer_res_handle'] >= 0

def test_hw_emu_write_buffer_for_vadd(global_variables):
    array_a = global_variables['vadd_array_a']
    array_b = global_variables['vadd_array_b']
    array_res = global_variables['vadd_array_res']
    print('INFO: vadd_buffer_a_handle => ' + str(global_variables['vadd_buffer_a_handle']))
    print('INFO: vadd_buffer_b_handle => ' + str(global_variables['vadd_buffer_b_handle']))
    print('INFO: vadd_buffer_res_handle => ' + str(global_variables['vadd_buffer_res_handle']))
    hal.write_buffer(device_id, global_variables['vadd_buffer_a_handle'], array_a)
    hal.write_buffer(device_id, global_variables['vadd_buffer_b_handle'], array_b)
    hal.write_buffer(device_id, global_variables['vadd_buffer_res_handle'], array_res)

def test_hw_emu_read_buffer_back_for_vadd(global_variables):
    read_back_array_a = hal.read_buffer(device_id, global_variables['vadd_buffer_a_handle'], vadd_byte_size, 0, "int")
    assert np.array_equal(global_variables['vadd_array_a'], read_back_array_a)
    read_back_array_b = hal.read_buffer(device_id, global_variables['vadd_buffer_b_handle'], vadd_byte_size, 0, "int")
    assert np.array_equal(global_variables['vadd_array_b'], read_back_array_b)
    read_back_array_res = hal.read_buffer(device_id, global_variables['vadd_buffer_res_handle'], vadd_byte_size, 0, "int")
    assert np.array_equal(global_variables['vadd_array_res'], read_back_array_res)

def test_hw_emu_sync_buffer_for_vadd(global_variables):
    print('INFO: vadd_buffer_a_handle => ' + str(global_variables['vadd_buffer_a_handle']))
    print('INFO: vadd_buffer_b_handle => ' + str(global_variables['vadd_buffer_b_handle']))
    print('INFO: vadd_buffer_res_handle => ' + str(global_variables['vadd_buffer_res_handle']))
    hal.sync_buffer(device_id, global_variables['vadd_buffer_a_handle'], 'host_to_device', vadd_byte_size, 0)
    hal.sync_buffer(device_id, global_variables['vadd_buffer_b_handle'], 'host_to_device', vadd_byte_size, 0)
    hal.sync_buffer(device_id, global_variables['vadd_buffer_res_handle'], 'host_to_device', vadd_byte_size, 0)

def test_hw_emu_allocate_command_buffer_for_vadd(global_variables):
    global_variables['vadd_ert_command_buffer_handle'] = hal.allocate_buffer(device_id, 'shared_virtual', int("0x80000000", 0), 1024*4)
    assert global_variables['vadd_ert_command_buffer_handle'] >= 0

def test_hw_emu_get_buffer_property_for_vadd(global_variables):
    print('Start fetching buffer properties ...')
    buffer_a_property = hal.buffer_property(device_id, global_variables['vadd_buffer_a_handle'])
    print('INFO: vadd buffer a physical address => ' + hex(buffer_a_property["physical_addr"]))
    global_variables['vadd_buffer_a_physical_addr'] = buffer_a_property["physical_addr"]
    buffer_b_property = hal.buffer_property(device_id, global_variables['vadd_buffer_b_handle'])
    print('INFO: vadd buffer b physical address => ' + hex(buffer_b_property["physical_addr"]))
    global_variables['vadd_buffer_b_physical_addr'] = buffer_b_property["physical_addr"]
    buffer_res_property = hal.buffer_property(device_id, global_variables['vadd_buffer_res_handle'])
    print('INFO: vadd buffer res physical address => ' + hex(buffer_res_property["physical_addr"]))
    global_variables['vadd_buffer_res_physical_addr'] = buffer_res_property["physical_addr"]

def test_hw_emu_map_command_buffer_for_vadd(global_variables):
    hal.map_buffer(device_id, global_variables['vadd_ert_command_buffer_handle'], True)

def test_hw_emu_configure_ert_for_vadd(global_variables):
    ert_config = {
        "slot_size": 1024,
        "num_compute_unit": 1,
        "compute_unit_shift": 16,
        "compute_unit_base_addr": global_variables['kernel_vadd_address'],
        "enable_ert": True,
        "compute_unit_dma": 1,
        "compute_unit_isr": 1,
        "state": "new",
        "opcode": "configure"
    }
    hal.configure_ert(device_id, global_variables['vadd_ert_command_buffer_handle'], ert_config)

def test_hw_emu_execute_ert_configuration_for_vadd(global_variables):
    hal.execute_buffer(device_id, global_variables['vadd_ert_command_buffer_handle'])

def test_hw_emu_execute_wait_ert_configuration_for_vadd(global_variables):
    hal.execute_wait(device_id, 1000)

def test_hw_emu_configure_start_kernel_for_vadd(global_variables):
    start_config = {
        "state": "new",
        "opcode": "start_compute_unit",
        "compute_unit_mask": int("0x1", 0),
        "argument_addr": [
            global_variables['vadd_buffer_a_physical_addr'],
            global_variables['vadd_buffer_b_physical_addr'],
            global_variables['vadd_buffer_res_physical_addr'],
            vadd_array_size
        ]
    }
    hal.start_kernel(device_id, global_variables['vadd_ert_command_buffer_handle'], start_config)

def test_hw_emu_execute_start_kernel_configuration_for_vadd(global_variables):
    hal.execute_buffer(device_id, global_variables['vadd_ert_command_buffer_handle'])

def test_hw_emu_execute_wait_start_kernel_configuration_for_vadd(global_variables):
    hal.execute_wait(device_id, 1000)

def test_hw_emu_sync_result_buffer_for_vadd(global_variables):
    # hal.sync_buffer(device_id, global_variables['vadd_buffer_res_handle'], 'device_to_host', vadd_byte_size, 0)
    pass

def test_hw_emu_compare_result_for_vadd(global_variables):
    pass

def test_hw_emu_close_device():
    hal.close(device_id)

def test_hw_emu_cleanup_environment():
    env.cleanup()