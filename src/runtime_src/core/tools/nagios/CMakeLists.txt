# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#

set(XRT_NAGIOS_INSTALL "${XRT_INSTALL_DIR}/etc/nagios-plugins")

# Determine the name of the executable
set(NAGIOS_LOADER_SCRIPTS "nagios_plugin.sh")

if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  install (PROGRAMS ${NAGIOS_LOADER_SCRIPTS}
    DESTINATION ${XRT_NAGIOS_INSTALL} COMPONENT ${XRT_BASE_COMPONENT})
endif()
