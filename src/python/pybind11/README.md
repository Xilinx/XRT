# Python package for the XRT Native C++ API

This python package can be used for binding the XRT Native C++ API into Python applications.  Installation of this 
package uses the same build dependencies for XRT and can be installed by running the following steps: 

```bash
git clone https://github.com/Xilinx/XRT
cd src/python/pybind11
pip install .
```

After installation, the package can be accessed by simply running from Python:

```python
import pyxrt
```

The below example was run in a hardware emulation environment using the simple kernel that can be built at:
https://github.com/Xilinx/XRT/tree/master/tests/xrt/02_simple


```python
import pyxrt

d = pyxrt.device(0)

xclbin = './simple.xclbin'
uuid = d.load_xclbin(xclbin)

simple = pyxrt.kernel(d, uuid.get(), "simple", False)

bo0 = pyxrt.bo(d, 1024, pyxrt.XCL_BO_FLAGS_NONE, simple.group_id(0))
bo1 = pyxrt.bo(d, 1024, pyxrt.XCL_BO_FLAGS_NONE, simple.group_id(1))

bo0.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, 1024, 0)
bo1.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, 1024, 0)

run = simple(*(bo0, bo1, 0x10))
run.wait()

bo0.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, 1024, 0)

```