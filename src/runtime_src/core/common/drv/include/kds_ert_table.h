/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * KDS State / ERT State Lookup Table
 *
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors: jefflin@amd.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _KDS_ERT_TABLE_H
#define _KDS_ERT_TABLE_H

#include "ert.h"
#include "kds_command.h"

static int kds_ert_table[] = {
  [KDS_COMPLETED] = ERT_CMD_STATE_COMPLETED,
  [KDS_ERROR] = ERT_CMD_STATE_ERROR,
  [KDS_ABORT] = ERT_CMD_STATE_ABORT,
  [KDS_TIMEOUT] = ERT_CMD_STATE_TIMEOUT,
  [KDS_SKCRASHED] = ERT_CMD_STATE_SKCRASHED,
};

static int ert_kds_table[] = {
  [ERT_CMD_STATE_COMPLETED] = KDS_COMPLETED,
  [ERT_CMD_STATE_ERROR] = KDS_ERROR,
  [ERT_CMD_STATE_ABORT] = KDS_ABORT,
  [ERT_CMD_STATE_TIMEOUT] = KDS_TIMEOUT,
  [ERT_CMD_STATE_SKERROR] = KDS_ERROR,
  [ERT_CMD_STATE_SKCRASHED] = KDS_SKCRASHED,
};

#endif
