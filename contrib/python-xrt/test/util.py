import os

class RuntimeEnv():
    def __init__(self, runtime_type, device_id):
        self.runtime_type = runtime_type
        self.device_id = device_id
        if 'PYTHON_XRT_TEST_SUITE' not in os.environ:
            raise RuntimeError('PYTHON_XRT_TEST_SUITE environment variable is not found')
        self.root = os.environ['PYTHON_XRT_TEST_SUITE']
    
    def set_emulation_path(self):
        if self.runtime_type == 'sw_emu':
            os.environ['XCL_EMULATION_MODE'] = 'sw_emu'
        if self.runtime_type == 'hw_emu':
            os.environ['XCL_EMULATION_MODE'] = 'sw_emu'
        if self.runtime_type == 'hw':
            del os.environ['XCL_EMULATION_MODE']
    
    def set_device_config_path(self):
        if self.runtime_type == 'sw_emu' or self.runtime_type == 'hw_emu':
            emu_config_path = os.path.join(self.root, 'devices/' + self.device_id)
            print(emu_config_path)
            os.environ['EMCONFIG_PATH'] = emu_config_path
        if self.runtime_type == 'hw':
            del os.environ['EMCONFIG_PATH']

    def setup(self):
        self.set_emulation_path()
        self.set_device_config_path()

    def cleanup(self):
        del os.environ['XCL_EMULATION_MODE']
        del os.environ['EMCONFIG_PATH']
