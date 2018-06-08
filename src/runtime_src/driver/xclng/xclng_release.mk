ifndef xclng_release.mk
xclng_release.mk := defined

# Release libs to specified directory
# $(1) : releasedir
# $(2) : file to release
define XCLNG_release_libs_template
 RELEASE_LIBS_DIR := $(1)
 RELEASE_LIBS := $(2)
 RELEASE_TYPE := customer nostrip
 include $(RDI_MAKEROOT)/release.mk
endef

#same as above for internal
define XCLNG_internal_release_libs_template
 RELEASE_LIBS_DIR := $(1)
 RELEASE_LIBS := $(2)
 RELEASE_TYPE := internal
 include $(RDI_MAKEROOT)/release.mk
endef

# Release execs to specified directory
# $(1) : releasedir
# $(2) : full path to file to release
# $(3) : name of target
define XCLNG_release_execs_rename_template
 RELEASE_EXECS_DIR := $(1)
 RELEASE_EXECS_RENAME_EXACT_FROM := $(2)
 RELEASE_EXECS_RENAME_TO := $(3)
 RELEASE_TYPE := customer nostrip
 include $(RDI_MAKEROOT)/release.mk
endef

#same as above for internal
define XCLNG_internal_release_execs_rename_template
 RELEASE_EXECS_DIR := $(1)
 RELEASE_EXECS_RENAME_EXACT_FROM := $(2)
 RELEASE_EXECS_RENAME_TO := $(3)
 RELEASE_TYPE := internal
 include $(RDI_MAKEROOT)/release.mk
endef

# Release data file specified directory
# $(1) : releasedir
# $(2) : file to release
define XCLNG_release_data_template
 RELEASE_DATA_DIR := $(1)
 RELEASE_DATA_EXACT := $(2)
 RELEASE_TYPE := customer
 include $(RDI_MAKEROOT)/release.mk
endef

# same as above for internal
define XCLNG_internal_release_data_template
 RELEASE_DATA_DIR := $(1)
 RELEASE_DATA_EXACT := $(2)
 RELEASE_TYPE := internal
 include $(RDI_MAKEROOT)/release.mk
endef

XCLNG_RELEASE_DIRS := \
 platforms/xilinx_kcu1500_dynamic_5_0 \
 platforms/xilinx_vcu1525_dynamic_5_1 \

XCLNG_INTERNAL_RELEASE_DIRS := \
 platforms/xilinx_kcu1500_4ddr-xpr_5_0 \
 platforms/xilinx_vcu1526_4ddr-xpr_5_0 \
 platforms/xilinx_kcu1500_dynamic_5_1 \
 platforms/xilinx_vcu1525_dynamic_5_0 \
 platforms/xilinx_vcu1525_dynamic_5_2 \
 platforms/xilinx_vcu1525_dynamic_6_0 \
 platforms/xilinx_vcu1526_dynamic_5_2 \
 platforms/xilinx_vcu1550_dynamic_5_0 \
 platforms/xilinx_vcu1550_dynamic_5_1 \
 platforms/advantech_vega-4000_dynamic_5_1 \
 platforms/xilinx_twitch_dynamic_5_1 \
 platforms/xilinx_huawei-vu5p_dynamic_5_1 \
 platforms/xilinx_vcu1525_dynamic-thinshell_5_1 \
 
AWS_XCLNG_INTERNAL_RELEASE_DIRS := \
 platforms/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_5_0 \
 platforms/xilinx_aws-vu9p-f1_dynamic_5_0 \
 platforms/xilinx_aws-vu9p-f1-04261818_dynamic_5_0 \

endif
