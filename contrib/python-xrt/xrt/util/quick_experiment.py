from ..core import hal

def query_device(device_index):
    device_cnt = hal.probe()
    if device_cnt <= 0:
        print("No device is attached to the system")
        return None, None
    hal.open(device_index, "tmp_device", "info")
    device_info = hal.info("tmp_device")
    device_usage = hal.usage("tmp_device")
    hal.close("tmp_device")
    return device_info, device_usage
