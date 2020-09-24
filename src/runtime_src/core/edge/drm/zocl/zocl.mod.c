#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(.gnu.linkonce.this_module) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cxlnx,ospi_versal");
MODULE_ALIAS("of:N*T*Cxlnx,ospi_versalC*");
MODULE_ALIAS("of:N*T*Cxlnx,zocl");
MODULE_ALIAS("of:N*T*Cxlnx,zoclC*");
MODULE_ALIAS("of:N*T*Cxlnx,zoclsvm");
MODULE_ALIAS("of:N*T*Cxlnx,zoclsvmC*");
MODULE_ALIAS("of:N*T*Cxlnx,zocl-ert");
MODULE_ALIAS("of:N*T*Cxlnx,zocl-ertC*");
MODULE_ALIAS("of:N*T*Cxlnx,zocl-versal");
MODULE_ALIAS("of:N*T*Cxlnx,zocl-versalC*");
MODULE_ALIAS("of:N*T*Cxlnx,embedded_sched");
MODULE_ALIAS("of:N*T*Cxlnx,embedded_schedC*");
MODULE_ALIAS("of:N*T*Cxlnx,embedded_sched_versal");
MODULE_ALIAS("of:N*T*Cxlnx,embedded_sched_versalC*");

MODULE_INFO(srcversion, "A6017F674A6E30B503B7979");
