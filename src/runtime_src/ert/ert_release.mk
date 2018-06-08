ifndef ert_release.mk
ert_release.mk := defined

# Release execs to specified directory
# $(1) : releasedir
# $(2) : full path to file to release
define ERT_release_execs_template
 RELEASE_EXECS_DIR := $(1)
 RELEASE_EXECS_EXACT := $(2)
 RELEASE_TYPE := customer nostrip
 include $(RDI_MAKEROOT)/release.mk
endef

#same as above for internal
define ERT_internal_release_execs_template
 RELEASE_EXECS_DIR := $(1)
 RELEASE_EXECS_EXACT := $(2)
 RELEASE_TYPE := internal nostrip
 include $(RDI_MAKEROOT)/release.mk
endef

# Release data file specified directory
# $(1) : releasedir
# $(2) : file to release
define ERT_release_data_template
 RELEASE_DATA_DIR := $(1)
 RELEASE_DATA_EXACT := $(2)
 RELEASE_TYPE := customer
 include $(RDI_MAKEROOT)/release.mk
endef

# same as above for internal
define ERT_internal_release_data_template
 RELEASE_DATA_DIR := $(1)
 RELEASE_DATA_EXACT := $(2)
 RELEASE_TYPE := internal
 include $(RDI_MAKEROOT)/release.mk
endef

# Platorms with embedded scheduler, same as driver platforms
include $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/xclng/xclng_release.mk

# disable for now
ERT_RELEASE_DIRS := $(filter %_5_0 %_5_1 %_5_2,$(XCLNG_RELEASE_DIRS))
ERT_INTERNAL_RELEASE_DIRS := $(filter %_5_0 %_5_1 %_5_2,$(XCLNG_INTERNAL_RELEASE_DIRS))

endif

