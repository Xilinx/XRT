.. _sysfs.rst:

Linux Sys FileSystem Nodes
**************************

``xocl`` and ``xclmgmt`` drivers expose several ``sysfs`` nodes under
the ``pci`` device root node. The sysfs nodes are populated by
platform drivers present in the respective drivers.

xocl
====

The ``xocl`` driver exposes various sections of the ``xclbin`` image
including the ``xclbinuuid`` on ``sysfs``. This makes it very
convenient for tools (such as ``xbutil``) to discover characteristics
of the image currently loaded on the FPGA. The data layout of ``xclbin``
sections are defined in file ``xclbin.h`` which can be found under
``runtime/driver/include`` directory. Platform drivers XDMA, ICAP,
MB Scheduler, Mailbox, XMC, XVC, FeatureROM export their nodes on sysfs.

Sample output of tree command below::

  dx4300:/<1>devices/pci0000:00/0000:00:15.0>tree -n 0000:04:00.1
  0000:04:00.1
  ├── broken_parity_status
  ├── class
  ├── config
  ├── config_mailbox_channel_switch
  ├── config_mailbox_comm_id
  ├── consistent_dma_mask_bits
  ├── current_link_speed
  ├── current_link_width
  ├── d3cold_allowed
  ├── device
  ├── dev_offline
  ├── dma_mask_bits
  ├── dma.xdma.u.1025
  │   ├── channel_stat_raw
  │   ├── driver -> ../../../../../bus/platform/drivers/xocl_xdma
  │   ├── driver_override
  │   ├── modalias
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── subsystem -> ../../../../../bus/platform
  │   └── uevent
  ├── driver -> ../../../../bus/pci/drivers/xocl
  ├── driver_override
  ├── drm
  │   ├── card1
  │   │   ├── dev
  │   │   ├── device -> ../../../0000:04:00.1
  │   │   ├── power
  │   │   │   ├── async
  │   │   │   ├── autosuspend_delay_ms
  │   │   │   ├── control
  │   │   │   ├── runtime_active_kids
  │   │   │   ├── runtime_active_time
  │   │   │   ├── runtime_enabled
  │   │   │   ├── runtime_status
  │   │   │   ├── runtime_suspended_time
  │   │   │   └── runtime_usage
  │   │   ├── subsystem -> ../../../../../../class/drm
  │   │   └── uevent
  │   └── renderD129
  │       ├── dev
  │       ├── device -> ../../../0000:04:00.1
  │       ├── power
  │       │   ├── async
  │       │   ├── autosuspend_delay_ms
  │       │   ├── control
  │       │   ├── runtime_active_kids
  │       │   ├── runtime_active_time
  │       │   ├── runtime_enabled
  │       │   ├── runtime_status
  │       │   ├── runtime_suspended_time
  │       │   └── runtime_usage
  │       ├── subsystem -> ../../../../../../class/drm
  │       └── uevent
  ├── enable
  ├── hwmon
  │   └── hwmon5
  │       ├── curr1_average
  │       ├── curr1_highest
  │       ├── curr1_input
  │       ├── curr2_average
  │       ├── curr2_highest
  │       ├── curr2_input
  │       ├── curr3_average
  │       ├── curr3_highest
  │       ├── curr3_input
  │       ├── curr4_average
  │       ├── curr4_highest
  │       ├── curr4_input
  │       ├── curr5_average
  │       ├── curr5_highest
  │       ├── curr5_input
  │       ├── curr6_average
  │       ├── curr6_highest
  │       ├── curr6_input
  │       ├── device -> ../../../0000:04:00.1
  │       ├── name
  │       ├── power
  │       │   ├── async
  │       │   ├── autosuspend_delay_ms
  │       │   ├── control
  │       │   ├── runtime_active_kids
  │       │   ├── runtime_active_time
  │       │   ├── runtime_enabled
  │       │   ├── runtime_status
  │       │   ├── runtime_suspended_time
  │       │   └── runtime_usage
  │       ├── subsystem -> ../../../../../../class/hwmon
  │       └── uevent
  ├── icap.u.1025
  │   ├── cache_expire_secs
  │   ├── clock_freqs
  │   ├── clock_freq_topology
  │   ├── connectivity
  │   ├── debug_ip_layout
  │   ├── driver -> ../../../../../bus/platform/drivers/icap.u
  │   ├── driver_override
  │   ├── idcode
  │   ├── ip_layout
  │   ├── mem_topology
  │   ├── modalias
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── subsystem -> ../../../../../bus/platform
  │   └── uevent
  ├── iommu -> ../../0000:00:00.2/iommu/ivhd0
  ├── iommu_group -> ../../../../kernel/iommu_groups/11
  ├── irq
  ├── kdsstat
  ├── link_speed
  ├── link_speed_max
  ├── link_width
  ├── link_width_max
  ├── local_cpulist
  ├── local_cpus
  ├── mailbox_connect_state
  ├── mailbox.u.1025
  │   ├── connection
  │   ├── driver -> ../../../../../bus/platform/drivers/mailbox.u
  │   ├── driver_override
  │   ├── mailbox
  │   ├── mailbox_ctl
  │   ├── mailbox_pkt
  │   ├── modalias
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── subsystem -> ../../../../../bus/platform
  │   ├── uevent
  │   └── xrt_user
  │       └── mailbox.u1025
  │           ├── dev
  │           ├── device -> ../../../mailbox.u.1025
  │           ├── power
  │           │   ├── async
  │           │   ├── autosuspend_delay_ms
  │           │   ├── control
  │           │   ├── runtime_active_kids
  │           │   ├── runtime_active_time
  │           │   ├── runtime_enabled
  │           │   ├── runtime_status
  │           │   ├── runtime_suspended_time
  │           │   └── runtime_usage
  │           ├── subsystem -> ../../../../../../../class/xrt_user
  │           └── uevent
  ├── max_link_speed
  ├── max_link_width
  ├── mb_scheduler.u.1025
  │   ├── driver -> ../../../../../bus/platform/drivers/xocl_mb_sche
  │   ├── driver_override
  │   ├── kds_cucounts
  │   ├── kds_custat
  │   ├── kds_numcdmas
  │   ├── kds_numcus
  │   ├── modalias
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── subsystem -> ../../../../../bus/platform
  │   └── uevent
  ├── memstat
  ├── memstat_raw
  ├── mig_calibration
  ├── modalias
  ├── msi_bus
  ├── msi_irqs
  │   ├── 75
  │   ├── 76
  │   ├── 77
  │   ├── 78
  │   ├── 79
  │   ├── 80
  │   ├── 81
  │   ├── 82
  │   ├── 83
  │   ├── 84
  │   ├── 85
  │   ├── 86
  │   ├── 87
  │   ├── 88
  │   ├── 89
  │   ├── 90
  │   ├── 91
  │   ├── 92
  │   ├── 93
  │   └── 94
  ├── numa_node
  ├── p2p_enable
  ├── power
  │   ├── async
  │   ├── autosuspend_delay_ms
  │   ├── control
  │   ├── runtime_active_kids
  │   ├── runtime_active_time
  │   ├── runtime_enabled
  │   ├── runtime_status
  │   ├── runtime_suspended_time
  │   └── runtime_usage
  ├── ready
  ├── remove
  ├── rescan
  ├── resource
  ├── resource0
  ├── resource0_wc
  ├── resource2
  ├── resource2_wc
  ├── resource4
  ├── resource4_wc
  ├── revision
  ├── rom.u.1025
  │   ├── ddr_bank_count_max
  │   ├── ddr_bank_size
  │   ├── dr_base_addr
  │   ├── driver -> ../../../../../bus/platform/drivers/rom.u
  │   ├── driver_override
  │   ├── FPGA
  │   ├── modalias
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── subsystem -> ../../../../../bus/platform
  │   ├── timestamp
  │   ├── uevent
  │   └── VBNV
  ├── root_dev -> ../../0000:00:15.0
  ├── subsystem -> ../../../../bus/pci
  ├── subsystem_device
  ├── subsystem_vendor
  ├── uevent
  ├── userbar
  ├── user_pf
  ├── vendor
  ├── xclbinuuid
  ├── xmc.u.1025
  │   ├── cache_expire_secs
  │   ├── capability
  │   ├── driver -> ../../../../../bus/platform/drivers/xmc.u
  │   ├── driver_override
  │   ├── error
  │   ├── host_msg_error
  │   ├── host_msg_header
  │   ├── host_msg_offset
  │   ├── id
  │   ├── modalias
  │   ├── pause
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── power_checksum
  │   ├── power_flag
  │   ├── reset
  │   ├── sensor
  │   ├── status
  │   ├── subsystem -> ../../../../../bus/platform
  │   ├── temp_by_mem_topology
  │   ├── uevent
  │   ├── version
  │   ├── xmc_0v85
  │   ├── xmc_12v_aux_curr
  │   ├── xmc_12v_aux_vol
  │   ├── xmc_12v_pex_curr
  │   ├── xmc_12v_pex_vol
  │   ├── xmc_12v_sw
  │   ├── xmc_1v2_top
  │   ├── xmc_1v8
  │   ├── xmc_3v3_aux_vol
  │   ├── xmc_3v3_pex_vol
  │   ├── xmc_cage_temp0
  │   ├── xmc_cage_temp1
  │   ├── xmc_cage_temp2
  │   ├── xmc_cage_temp3
  │   ├── xmc_ddr_vpp_btm
  │   ├── xmc_ddr_vpp_top
  │   ├── xmc_dimm_temp0
  │   ├── xmc_dimm_temp1
  │   ├── xmc_dimm_temp2
  │   ├── xmc_dimm_temp3
  │   ├── xmc_fan_rpm
  │   ├── xmc_fan_temp
  │   ├── xmc_fpga_temp
  │   ├── xmc_mgt0v9avcc
  │   ├── xmc_mgtavtt
  │   ├── xmc_se98_temp0
  │   ├── xmc_se98_temp1
  │   ├── xmc_se98_temp2
  │   ├── xmc_sys_5v5
  │   ├── xmc_vcc1v2_btm
  │   ├── xmc_vccint_curr
  │   └── xmc_vccint_vol
  └── xvc_pub.u.1025
      ├── driver -> ../../../../../bus/platform/drivers/xvc.u
      ├── driver_override
      ├── modalias
      ├── power
      │   ├── async
      │   ├── autosuspend_delay_ms
      │   ├── control
      │   ├── runtime_active_kids
      │   ├── runtime_active_time
      │   ├── runtime_enabled
      │   ├── runtime_status
      │   ├── runtime_suspended_time
      │   └── runtime_usage
      ├── subsystem -> ../../../../../bus/platform
      ├── uevent
      └── xrt_user
          └── xvc_pub.u1025
              ├── dev
              ├── device -> ../../../xvc_pub.u.1025
              ├── power
              │   ├── async
              │   ├── autosuspend_delay_ms
              │   ├── control
              │   ├── runtime_active_kids
              │   ├── runtime_active_time
              │   ├── runtime_enabled
              │   ├── runtime_status
              │   ├── runtime_suspended_time
              │   └── runtime_usage
              ├── subsystem -> ../../../../../../../class/xrt_user
              └── uevent

  59 directories, 306 files


xclmgmt
=======

The ``xclmgmt`` driver exposes various sections of the ``xclbin`` image
including the ``xclbinuuid`` on ``sysfs``. This makes it very
convenient for tools (such as ``xbutil``) to discover characteristics
of the image currently loaded on the FPGA. The data layout of ``xclbin``
sections are defined in file ``xclbin.h`` which can be found under
``runtime/driver/include`` directory. Platform drivers ICAP, FPGA Manager,
AXI Firewall, Mailbox, XMC, XVC, FeatureROM export their nodes on sysfs.

Sample output of tree command below::

  dx4300:/<1>devices/pci0000:00/0000:00:15.0>tree 0000:04:00.0
  0000:04:00.0
  ├── board_name
  ├── broken_parity_status
  ├── class
  ├── config
  ├── config_mailbox_channel_switch
  ├── config_mailbox_comm_id
  ├── consistent_dma_mask_bits
  ├── current_link_speed
  ├── current_link_width
  ├── d3cold_allowed
  ├── device
  ├── dev_offline
  ├── dma_mask_bits
  ├── driver -> ../../../../bus/pci/drivers/xclmgmt
  ├── driver_override
  ├── enable
  ├── error
  ├── feature_rom_offset
  ├── firewall.m.1024
  │   ├── clear
  │   ├── detected_level
  │   ├── detected_status
  │   ├── detected_time
  │   ├── driver -> ../../../../../bus/platform/drivers/xocl_firewall
  │   ├── driver_override
  │   ├── inject
  │   ├── level
  │   ├── modalias
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── status
  │   ├── subsystem -> ../../../../../bus/platform
  │   └── uevent
  ├── flash_type
  ├── fmgr.m.1024
  │   ├── driver -> ../../../../../bus/platform/drivers/xocl_fmgr
  │   ├── driver_override
  │   ├── fpga_manager
  │   │   └── fpga0
  │   │       ├── device -> ../../../fmgr.m.1024
  │   │       ├── name
  │   │       ├── power
  │   │       │   ├── async
  │   │       │   ├── autosuspend_delay_ms
  │   │       │   ├── control
  │   │       │   ├── runtime_active_kids
  │   │       │   ├── runtime_active_time
  │   │       │   ├── runtime_enabled
  │   │       │   ├── runtime_status
  │   │       │   ├── runtime_suspended_time
  │   │       │   └── runtime_usage
  │   │       ├── state
  │   │       ├── subsystem -> ../../../../../../../class/fpga_manager
  │   │       └── uevent
  │   ├── modalias
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── subsystem -> ../../../../../bus/platform
  │   └── uevent
  ├── hwmon
  │   ├── hwmon3
  │   │   ├── device -> ../../../0000:04:00.0
  │   │   ├── in0_highest
  │   │   ├── in0_input
  │   │   ├── in0_lowest
  │   │   ├── in1_highest
  │   │   ├── in1_input
  │   │   ├── in1_lowest
  │   │   ├── in2_highest
  │   │   ├── in2_input
  │   │   ├── in2_lowest
  │   │   ├── name
  │   │   ├── power
  │   │   │   ├── async
  │   │   │   ├── autosuspend_delay_ms
  │   │   │   ├── control
  │   │   │   ├── runtime_active_kids
  │   │   │   ├── runtime_active_time
  │   │   │   ├── runtime_enabled
  │   │   │   ├── runtime_status
  │   │   │   ├── runtime_suspended_time
  │   │   │   └── runtime_usage
  │   │   ├── subsystem -> ../../../../../../class/hwmon
  │   │   ├── temp1_highest
  │   │   ├── temp1_input
  │   │   ├── temp1_lowest
  │   │   └── uevent
  │   └── hwmon4
  │       ├── curr1_average
  │       ├── curr1_highest
  │       ├── curr1_input
  │       ├── curr2_average
  │       ├── curr2_highest
  │       ├── curr2_input
  │       ├── curr3_average
  │       ├── curr3_highest
  │       ├── curr3_input
  │       ├── curr4_average
  │       ├── curr4_highest
  │       ├── curr4_input
  │       ├── curr5_average
  │       ├── curr5_highest
  │       ├── curr5_input
  │       ├── curr6_average
  │       ├── curr6_highest
  │       ├── curr6_input
  │       ├── device -> ../../../0000:04:00.0
  │       ├── name
  │       ├── power
  │       │   ├── async
  │       │   ├── autosuspend_delay_ms
  │       │   ├── control
  │       │   ├── runtime_active_kids
  │       │   ├── runtime_active_time
  │       │   ├── runtime_enabled
  │       │   ├── runtime_status
  │       │   ├── runtime_suspended_time
  │       │   └── runtime_usage
  │       ├── subsystem -> ../../../../../../class/hwmon
  │       └── uevent
  ├── icap.m.1024
  │   ├── cache_expire_secs
  │   ├── clock_freqs
  │   ├── clock_freq_topology
  │   ├── connectivity
  │   ├── debug_ip_layout
  │   ├── driver -> ../../../../../bus/platform/drivers/icap.m
  │   ├── driver_override
  │   ├── idcode
  │   ├── ip_layout
  │   ├── mem_topology
  │   ├── modalias
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── shell_program
  │   ├── subsystem -> ../../../../../bus/platform
  │   └── uevent
  ├── instance
  ├── iommu -> ../../0000:00:00.2/iommu/ivhd0
  ├── iommu_group -> ../../../../kernel/iommu_groups/11
  ├── irq
  ├── link_speed
  ├── link_speed_max
  ├── link_width
  ├── link_width_max
  ├── local_cpulist
  ├── local_cpus
  ├── mailbox.m.1024
  │   ├── connection
  │   ├── driver -> ../../../../../bus/platform/drivers/mailbox.m
  │   ├── driver_override
  │   ├── mailbox
  │   ├── mailbox_ctl
  │   ├── mailbox_pkt
  │   ├── modalias
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── subsystem -> ../../../../../bus/platform
  │   ├── uevent
  │   └── xrt_mgmt
  │       └── mailbox.m1024
  │           ├── dev
  │           ├── device -> ../../../mailbox.m.1024
  │           ├── power
  │           │   ├── async
  │           │   ├── autosuspend_delay_ms
  │           │   ├── control
  │           │   ├── runtime_active_kids
  │           │   ├── runtime_active_time
  │           │   ├── runtime_enabled
  │           │   ├── runtime_status
  │           │   ├── runtime_suspended_time
  │           │   └── runtime_usage
  │           ├── subsystem -> ../../../../../../../class/xrt_mgmt
  │           └── uevent
  ├── max_link_speed
  ├── max_link_width
  ├── mfg
  ├── mgmt_pf
  ├── mig_calibration
  ├── modalias
  ├── msi_bus
  ├── msi_irqs
  │   ├── 52
  │   ├── 53
  │   ├── 54
  │   ├── 55
  │   ├── 56
  │   ├── 57
  │   ├── 58
  │   ├── 59
  │   ├── 60
  │   ├── 61
  │   ├── 62
  │   ├── 63
  │   ├── 64
  │   ├── 65
  │   ├── 66
  │   ├── 67
  │   ├── 68
  │   ├── 69
  │   ├── 70
  │   └── 71
  ├── nifd_pri.m.1024
  │   ├── driver -> ../../../../../bus/platform/drivers/nifd.m
  │   ├── driver_override
  │   ├── modalias
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── subsystem -> ../../../../../bus/platform
  │   └── uevent
  ├── numa_node
  ├── power
  │   ├── async
  │   ├── autosuspend_delay_ms
  │   ├── control
  │   ├── runtime_active_kids
  │   ├── runtime_active_time
  │   ├── runtime_enabled
  │   ├── runtime_status
  │   ├── runtime_suspended_time
  │   └── runtime_usage
  ├── ready
  ├── remove
  ├── rescan
  ├── resource
  ├── resource0
  ├── resource0_wc
  ├── resource2
  ├── resource2_wc
  ├── revision
  ├── rom.m.1024
  │   ├── ddr_bank_count_max
  │   ├── ddr_bank_size
  │   ├── dr_base_addr
  │   ├── driver -> ../../../../../bus/platform/drivers/rom.m
  │   ├── driver_override
  │   ├── FPGA
  │   ├── modalias
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── subsystem -> ../../../../../bus/platform
  │   ├── timestamp
  │   ├── uevent
  │   └── VBNV
  ├── slot
  ├── subdev_offline
  ├── subdev_online
  ├── subsystem -> ../../../../bus/pci
  ├── subsystem_device
  ├── subsystem_vendor
  ├── sysmon.m.1024
  │   ├── driver -> ../../../../../bus/platform/drivers/xocl_sysmon
  │   ├── driver_override
  │   ├── modalias
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── subsystem -> ../../../../../bus/platform
  │   ├── temp
  │   ├── uevent
  │   ├── vcc_aux
  │   ├── vcc_bram
  │   └── vcc_int
  ├── uevent
  ├── userbar
  ├── vendor
  ├── version
  ├── xmc.m.1024
  │   ├── cache_expire_secs
  │   ├── capability
  │   ├── driver -> ../../../../../bus/platform/drivers/xmc.m
  │   ├── driver_override
  │   ├── error
  │   ├── host_msg_error
  │   ├── host_msg_header
  │   ├── host_msg_offset
  │   ├── id
  │   ├── modalias
  │   ├── pause
  │   ├── power
  │   │   ├── async
  │   │   ├── autosuspend_delay_ms
  │   │   ├── control
  │   │   ├── runtime_active_kids
  │   │   ├── runtime_active_time
  │   │   ├── runtime_enabled
  │   │   ├── runtime_status
  │   │   ├── runtime_suspended_time
  │   │   └── runtime_usage
  │   ├── power_checksum
  │   ├── power_flag
  │   ├── reset
  │   ├── sensor
  │   ├── status
  │   ├── subsystem -> ../../../../../bus/platform
  │   ├── temp_by_mem_topology
  │   ├── uevent
  │   ├── version
  │   ├── xmc_0v85
  │   ├── xmc_12v_aux_curr
  │   ├── xmc_12v_aux_vol
  │   ├── xmc_12v_pex_curr
  │   ├── xmc_12v_pex_vol
  │   ├── xmc_12v_sw
  │   ├── xmc_1v2_top
  │   ├── xmc_1v8
  │   ├── xmc_3v3_aux_vol
  │   ├── xmc_3v3_pex_vol
  │   ├── xmc_cage_temp0
  │   ├── xmc_cage_temp1
  │   ├── xmc_cage_temp2
  │   ├── xmc_cage_temp3
  │   ├── xmc_ddr_vpp_btm
  │   ├── xmc_ddr_vpp_top
  │   ├── xmc_dimm_temp0
  │   ├── xmc_dimm_temp1
  │   ├── xmc_dimm_temp2
  │   ├── xmc_dimm_temp3
  │   ├── xmc_fan_rpm
  │   ├── xmc_fan_temp
  │   ├── xmc_fpga_temp
  │   ├── xmc_mgt0v9avcc
  │   ├── xmc_mgtavtt
  │   ├── xmc_se98_temp0
  │   ├── xmc_se98_temp1
  │   ├── xmc_se98_temp2
  │   ├── xmc_sys_5v5
  │   ├── xmc_vcc1v2_btm
  │   ├── xmc_vccint_curr
  │   └── xmc_vccint_vol
  ├── xpr
  ├── xrt_mgmt
  │   └── xclmgmt1024
  │       ├── dev
  │       ├── device -> ../../../0000:04:00.0
  │       ├── power
  │       │   ├── async
  │       │   ├── autosuspend_delay_ms
  │       │   ├── control
  │       │   ├── runtime_active_kids
  │       │   ├── runtime_active_time
  │       │   ├── runtime_enabled
  │       │   ├── runtime_status
  │       │   ├── runtime_suspended_time
  │       │   └── runtime_usage
  │       ├── subsystem -> ../../../../../../class/xrt_mgmt
  │       └── uevent
  └── xvc_pri.m.1024
      ├── driver -> ../../../../../bus/platform/drivers/xvc.m
      ├── driver_override
      ├── modalias
      ├── power
      │   ├── async
      │   ├── autosuspend_delay_ms
      │   ├── control
      │   ├── runtime_active_kids
      │   ├── runtime_active_time
      │   ├── runtime_enabled
      │   ├── runtime_status
      │   ├── runtime_suspended_time
      │   └── runtime_usage
      ├── subsystem -> ../../../../../bus/platform
      ├── uevent
      └── xrt_mgmt
          └── xvc_pri.m66560
              ├── dev
              ├── device -> ../../../xvc_pri.m.1024
              ├── power
              │   ├── async
              │   ├── autosuspend_delay_ms
              │   ├── control
              │   ├── runtime_active_kids
              │   ├── runtime_active_time
              │   ├── runtime_enabled
              │   ├── runtime_status
              │   ├── runtime_suspended_time
              │   └── runtime_usage
              ├── subsystem -> ../../../../../../../class/xrt_mgmt
              └── uevent

  71 directories, 364 files


zocl
====

Similar to PCIe drivers ``zocl`` driver used in embedded platforms
exposes various sections of the ``xclbin`` image
including the ``xclbinuuid`` on ``sysfs``. This makes it very
convenient for tools (such as ``xbutil``) to discover characteristics
of the image currently loaded on the FPGA. The data layout of ``xclbin``
sections are defined in file ``xclbin.h`` which can be found under
``runtime/driver/include`` directory.

Sample output of tree command below::

  mpsoc:/sys/bus/platform/devices/amba>tree zyxclmm_drm
  zyxclmm_drm
  ├── connectivity
  ├── debug_ip_layout
  ├── ip_layout
  ├── kds_custat
  ├── memstat
  ├── memstat_raw
  ├── mem_topology
  ├── modalias
  ├── of_node
  │   ├── compatible
  │   ├── name
  │   ├── reg
  │   └── status
  ├── power
  │   ├── autosuspend_delay_ms
  │   ├── control
  │   ├── runtime_active_time
  │   ├── runtime_status
  │   └── runtime_suspended_time
  ├── uevent
  └── xclbinid
