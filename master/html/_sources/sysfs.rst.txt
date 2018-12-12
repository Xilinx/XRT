Linux Sys FileSystem Nodes
--------------------------

``xocl`` and ``xclmgmt`` drivers expose several ``sysfs`` nodes under
the ``pci`` device root node.

xocl
~~~~

The ``xocl`` driver exposes various sections of the ``xclbin`` image
including the ``xclbin`` ``Id`` on ``sysfs``. This makes it very
convenient for tools (such as ``xbutil``) to discover characteristics
of the image currently loaded on the FPGA. The data layout of ``xclbin``
sections are defined in file ``xclbin.h`` which can be found under
``runtime/driver/include`` directory.

``ip_layout``
  Exposes IP LAYOUT section of ``xclbin``
``connectivity``
  Exposes CONNECTIVITY section of ``xclbin``
``mem_topology``
  Exposes MEM TOPOLOGY section of ``xclbin``
``xclbinid``
  Exposes ``xclbin`` unique identifier

xclmgmt
~~~~~~~

``xclmgmt`` driver exposes the instance number (suffix used in
``/dev/xclmgmt%d``) on ``sysfs``. This makes it convenient to uniquely map a
PCIe slot on ``sysfs`` to ``/dev/xclmgmt%d`` device node created by the
driver.

Device sensors are exposed as standard ``hwmon`` file hierarchy. Two
``hwmon`` nodes are created: ``sysmon`` and ``microblaze``. ``sysmon``
exposes device temperature and voltages. ``microblaze`` exposes device
currents on various rails by using an embedded board management
firmware. Values with ``_input`` suffix represent live values. The
values are compatible with the Linux standard ``lm-sensors`` tool.

For example if the bus address of a physical function is
``0000:01:00.1`` then ``hwmon`` would show up under
``/sys/bus/pci/devices/0000:01:00.1``. See sample session log below::

   dx4300:~>tree -L 1 /sys/bus/pci/devices/0000:01:00.1/hwmon
   /sys/bus/pci/devices/0000:01:00.1/hwmon
   ├── hwmon3
   └── hwmon4

   2 directories, 0 files
   dx4300:~>cat /sys/bus/pci/devices/0000:01:00.1/hwmon/hwmon3/name
   xclmgmt_sysmon
   dx4300:~>cat /sys/bus/pci/devices/0000:01:00.1/hwmon/hwmon4/name
   xclmgmt_microblaze
   dx4300:~>sensors
   xclmgmt_microblaze-pci-0101
   Adapter: PCI adapter
   curr1:        +4.47 A  (avg =  +0.00 A, highest =  +4.63 A)
   curr2:        +4.52 A  (avg =  +0.00 A, highest =  +4.80 A)
   curr3:        +3.12 A  (avg =  +0.00 A, highest =  +3.36 A)
   curr4:        +0.00 A  (avg =  +0.00 A, highest =  +0.00 A)
   curr5:        +1.00 A  (avg =  +0.00 A, highest =  +1.00 A)
   curr6:        +0.50 A  (avg =  +0.00 A, highest =  +0.50 A)

   xclmgmt_sysmon-pci-0101
   Adapter: PCI adapter
   in0:          +0.93 V  (lowest =  +0.92 V, highest =  +0.95 V)
   in1:          +1.79 V  (lowest =  +1.78 V, highest =  +1.80 V)
   in2:          +0.94 V  (lowest =  +0.92 V, highest =  +0.95 V)
   temp1:        +46.6°C  (lowest = +36.1°C, highest = +48.8°C)
