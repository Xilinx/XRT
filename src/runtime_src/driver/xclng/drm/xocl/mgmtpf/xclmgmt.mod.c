#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x2005612d, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xb7b4110d, __VMLINUX_SYMBOL_STR(device_remove_file) },
	{ 0xbdc6b187, __VMLINUX_SYMBOL_STR(cpu_tss) },
	{ 0xb04e4a38, __VMLINUX_SYMBOL_STR(cdev_del) },
	{ 0xffc92130, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x253b16ed, __VMLINUX_SYMBOL_STR(pci_bus_read_config_byte) },
	{ 0xd2b09ce5, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0xe7bfc44a, __VMLINUX_SYMBOL_STR(cdev_init) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0xd6ee688f, __VMLINUX_SYMBOL_STR(vmalloc) },
	{ 0xec1b975b, __VMLINUX_SYMBOL_STR(param_ops_int) },
	{ 0xb78a3d33, __VMLINUX_SYMBOL_STR(device_release_driver) },
	{ 0x43a53735, __VMLINUX_SYMBOL_STR(__alloc_workqueue_key) },
	{ 0x263ed23b, __VMLINUX_SYMBOL_STR(__x86_indirect_thunk_r12) },
	{ 0x6b9b4244, __VMLINUX_SYMBOL_STR(pci_get_slot) },
	{ 0x4be867d2, __VMLINUX_SYMBOL_STR(boot_cpu_data) },
	{ 0x3d5383d0, __VMLINUX_SYMBOL_STR(pci_disable_device) },
	{ 0x5f98cea5, __VMLINUX_SYMBOL_STR(pci_disable_msix) },
	{ 0xc415df13, __VMLINUX_SYMBOL_STR(hwmon_device_unregister) },
	{ 0x88bfa7e, __VMLINUX_SYMBOL_STR(cancel_work_sync) },
	{ 0x7bf1aab1, __VMLINUX_SYMBOL_STR(device_destroy) },
	{ 0x69a0ca7d, __VMLINUX_SYMBOL_STR(iowrite16be) },
	{ 0xeae3dfd6, __VMLINUX_SYMBOL_STR(__const_udelay) },
	{ 0x22e5a4e3, __VMLINUX_SYMBOL_STR(pci_release_regions) },
	{ 0x9580deb, __VMLINUX_SYMBOL_STR(init_timer_key) },
	{ 0x47256491, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x7485e15e, __VMLINUX_SYMBOL_STR(unregister_chrdev_region) },
	{ 0x999e8297, __VMLINUX_SYMBOL_STR(vfree) },
	{ 0x91715312, __VMLINUX_SYMBOL_STR(sprintf) },
	{ 0xb64e9b91, __VMLINUX_SYMBOL_STR(sysfs_remove_group) },
	{ 0x5b5c803a, __VMLINUX_SYMBOL_STR(kthread_create_on_node) },
	{ 0x2befa3be, __VMLINUX_SYMBOL_STR(__platform_driver_register) },
	{ 0x15ba50a6, __VMLINUX_SYMBOL_STR(jiffies) },
	{ 0x75dca459, __VMLINUX_SYMBOL_STR(i2c_add_adapter) },
	{ 0x9e88526, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0x4f8b5ddb, __VMLINUX_SYMBOL_STR(_copy_to_user) },
	{ 0x64ab0e98, __VMLINUX_SYMBOL_STR(wait_for_completion) },
	{ 0x706d051c, __VMLINUX_SYMBOL_STR(del_timer_sync) },
	{ 0xfdb9b629, __VMLINUX_SYMBOL_STR(ioread32be) },
	{ 0xf10de535, __VMLINUX_SYMBOL_STR(ioread8) },
	{ 0xce674d02, __VMLINUX_SYMBOL_STR(pci_enable_msix) },
	{ 0x254ec381, __VMLINUX_SYMBOL_STR(pci_restore_state) },
	{ 0x322643b7, __VMLINUX_SYMBOL_STR(pci_iounmap) },
	{ 0x8180b3bd, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0x1916e38c, __VMLINUX_SYMBOL_STR(_raw_spin_unlock_irqrestore) },
	{ 0xbf2b6129, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0x14f2edb, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x20c55ae0, __VMLINUX_SYMBOL_STR(sscanf) },
	{ 0xd8e8bc9a, __VMLINUX_SYMBOL_STR(kthread_stop) },
	{ 0xd160a17c, __VMLINUX_SYMBOL_STR(sysfs_create_group) },
	{ 0x449ad0a7, __VMLINUX_SYMBOL_STR(memcmp) },
	{ 0xbca4604c, __VMLINUX_SYMBOL_STR(platform_device_alloc) },
	{ 0x7d96cea3, __VMLINUX_SYMBOL_STR(wait_for_completion_interruptible) },
	{ 0x85398fca, __VMLINUX_SYMBOL_STR(platform_device_add) },
	{ 0xedaa01e6, __VMLINUX_SYMBOL_STR(pci_bus_write_config_dword) },
	{ 0xbfe8ed5b, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x8c03d20c, __VMLINUX_SYMBOL_STR(destroy_workqueue) },
	{ 0xdb1eb143, __VMLINUX_SYMBOL_STR(device_attach) },
	{ 0x1e6d26a8, __VMLINUX_SYMBOL_STR(strstr) },
	{ 0x8f87f89e, __VMLINUX_SYMBOL_STR(platform_get_resource) },
	{ 0xdba3fa1c, __VMLINUX_SYMBOL_STR(device_create) },
	{ 0x16e5c2a, __VMLINUX_SYMBOL_STR(mod_timer) },
	{ 0x48c7611f, __VMLINUX_SYMBOL_STR(platform_device_unregister) },
	{ 0xce8b1878, __VMLINUX_SYMBOL_STR(__x86_indirect_thunk_r14) },
	{ 0xd2ced311, __VMLINUX_SYMBOL_STR(kill_pid) },
	{ 0x2072ee9b, __VMLINUX_SYMBOL_STR(request_threaded_irq) },
	{ 0x990145da, __VMLINUX_SYMBOL_STR(i2c_unregister_device) },
	{ 0x223ff585, __VMLINUX_SYMBOL_STR(devm_kfree) },
	{ 0x868784cb, __VMLINUX_SYMBOL_STR(__symbol_get) },
	{ 0xfe5d4bb2, __VMLINUX_SYMBOL_STR(sys_tz) },
	{ 0x8f8aea3f, __VMLINUX_SYMBOL_STR(device_create_file) },
	{ 0xbf4a2832, __VMLINUX_SYMBOL_STR(cdev_add) },
	{ 0x355508c2, __VMLINUX_SYMBOL_STR(platform_device_add_resources) },
	{ 0xc6cbbc89, __VMLINUX_SYMBOL_STR(capable) },
	{ 0x11fcb1e5, __VMLINUX_SYMBOL_STR(i2c_del_adapter) },
	{ 0xcd3109b5, __VMLINUX_SYMBOL_STR(_dev_info) },
	{ 0xb601be4c, __VMLINUX_SYMBOL_STR(__x86_indirect_thunk_rdx) },
	{ 0xbd574d89, __VMLINUX_SYMBOL_STR(i2c_smbus_xfer) },
	{ 0x6acb973d, __VMLINUX_SYMBOL_STR(iowrite32be) },
	{ 0x42c8de35, __VMLINUX_SYMBOL_STR(ioremap_nocache) },
	{ 0xee1e8c21, __VMLINUX_SYMBOL_STR(pci_bus_read_config_word) },
	{ 0x5944d015, __VMLINUX_SYMBOL_STR(__cachemode2pte_tbl) },
	{ 0xab57206c, __VMLINUX_SYMBOL_STR(pci_bus_read_config_dword) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x727c4f3, __VMLINUX_SYMBOL_STR(iowrite8) },
	{ 0x2ea2c95c, __VMLINUX_SYMBOL_STR(__x86_indirect_thunk_rax) },
	{ 0xe3e10c68, __VMLINUX_SYMBOL_STR(wake_up_process) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0x557eba0c, __VMLINUX_SYMBOL_STR(pci_cfg_access_lock) },
	{ 0x18883aa8, __VMLINUX_SYMBOL_STR(pci_unregister_driver) },
	{ 0x44221035, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x7a69f521, __VMLINUX_SYMBOL_STR(__dynamic_dev_dbg) },
	{ 0x680ec266, __VMLINUX_SYMBOL_STR(_raw_spin_lock_irqsave) },
	{ 0xb3f7646e, __VMLINUX_SYMBOL_STR(kthread_should_stop) },
	{ 0x4f68e5c9, __VMLINUX_SYMBOL_STR(do_gettimeofday) },
	{ 0x8575910b, __VMLINUX_SYMBOL_STR(pci_bus_write_config_byte) },
	{ 0x8c183cbe, __VMLINUX_SYMBOL_STR(iowrite16) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x69ad2f20, __VMLINUX_SYMBOL_STR(kstrtouint) },
	{ 0x2161831f, __VMLINUX_SYMBOL_STR(remap_pfn_range) },
	{ 0x69acdf38, __VMLINUX_SYMBOL_STR(memcpy) },
	{ 0x8ca13db2, __VMLINUX_SYMBOL_STR(pci_request_regions) },
	{ 0xedc03953, __VMLINUX_SYMBOL_STR(iounmap) },
	{ 0xa03f14d2, __VMLINUX_SYMBOL_STR(__pci_register_driver) },
	{ 0x8b8dd7ac, __VMLINUX_SYMBOL_STR(class_destroy) },
	{ 0x3720aaad, __VMLINUX_SYMBOL_STR(request_firmware) },
	{ 0x93b34116, __VMLINUX_SYMBOL_STR(pci_get_device) },
	{ 0x6e9dd606, __VMLINUX_SYMBOL_STR(__symbol_put) },
	{ 0x2e0d2f7f, __VMLINUX_SYMBOL_STR(queue_work_on) },
	{ 0xb2d5a552, __VMLINUX_SYMBOL_STR(complete) },
	{ 0x28318305, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0xe637e9c8, __VMLINUX_SYMBOL_STR(pci_iomap) },
	{ 0xe84a02e9, __VMLINUX_SYMBOL_STR(platform_driver_unregister) },
	{ 0x436c2179, __VMLINUX_SYMBOL_STR(iowrite32) },
	{ 0xd60ea379, __VMLINUX_SYMBOL_STR(pci_enable_device) },
	{ 0x62a0145d, __VMLINUX_SYMBOL_STR(devm_kmalloc) },
	{ 0x4f6b400b, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0xf6185bb8, __VMLINUX_SYMBOL_STR(__class_create) },
	{ 0xe1ca498f, __VMLINUX_SYMBOL_STR(i2c_new_device) },
	{ 0x74bd0b5f, __VMLINUX_SYMBOL_STR(pci_cfg_access_unlock) },
	{ 0x624bc590, __VMLINUX_SYMBOL_STR(hwmon_device_register) },
	{ 0x67c4f41f, __VMLINUX_SYMBOL_STR(pci_find_ext_capability) },
	{ 0x92acc7c, __VMLINUX_SYMBOL_STR(release_firmware) },
	{ 0x29537c9e, __VMLINUX_SYMBOL_STR(alloc_chrdev_region) },
	{ 0xe484e35f, __VMLINUX_SYMBOL_STR(ioread32) },
	{ 0x8a5f2fc1, __VMLINUX_SYMBOL_STR(pcie_capability_read_word) },
	{ 0xf20dabd8, __VMLINUX_SYMBOL_STR(free_irq) },
	{ 0xb175f01e, __VMLINUX_SYMBOL_STR(pci_save_state) },
	{ 0x38a0422c, __VMLINUX_SYMBOL_STR(platform_device_put) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("pci:v000010EEd00004A47sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00004A87sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00004B47sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00004B87sv*sd00004350bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00004B87sv*sd00004351bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd0000684Fsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd0000A883sv*sd00001351bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd0000688Fsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd0000694Fsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd0000698Fsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006A4Fsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006A8Fsv*sd00004350bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006A8Fsv*sd00004351bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006A8Fsv*sd00004352bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006A9Fsv*sd00004360bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006A9Fsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006E4Fsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006B0Fsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006E8Fsv*sd00004352bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd0000888Fsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd0000898Fsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd0000788Fsv*sd00004351bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd0000788Fsv*sd00004352bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd0000798Fsv*sd00004352bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00005000sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00005004sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000013FEd0000006Csv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "63EC66C1B4AABDFC1DD5B1B");
