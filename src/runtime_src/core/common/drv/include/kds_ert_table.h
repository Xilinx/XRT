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

static const enum ert_cmd_state kds_ert_table[] = {
  [KDS_NEW] = ERT_CMD_STATE_NEW,
  [KDS_QUEUED] = ERT_CMD_STATE_QUEUED,
  [KDS_RUNNING] = ERT_CMD_STATE_RUNNING,
  [KDS_COMPLETED] = ERT_CMD_STATE_COMPLETED,
  [KDS_ERROR] = ERT_CMD_STATE_ERROR,
  [KDS_ABORT] = ERT_CMD_STATE_ABORT,
  [KDS_TIMEOUT] = ERT_CMD_STATE_TIMEOUT,
  [KDS_SKCRASHED] = ERT_CMD_STATE_SKCRASHED,
  [KDS_STAT_MAX] = ERT_CMD_STATE_MAX,
};

static const enum kds_status ert_kds_table[] = {
  [ERT_CMD_STATE_NEW] = KDS_NEW,
  [ERT_CMD_STATE_QUEUED] = KDS_QUEUED,
  [ERT_CMD_STATE_RUNNING] = KDS_RUNNING,
  [ERT_CMD_STATE_COMPLETED] = KDS_COMPLETED,
  [ERT_CMD_STATE_ERROR] = KDS_ERROR,
  [ERT_CMD_STATE_ABORT] = KDS_ABORT,
  [ERT_CMD_STATE_TIMEOUT] = KDS_TIMEOUT,
  [ERT_CMD_STATE_NORESPONSE] = KDS_TIMEOUT,
  [ERT_CMD_STATE_SKERROR] = KDS_ERROR,
  [ERT_CMD_STATE_SKCRASHED] = KDS_SKCRASHED,
  [ERT_CMD_STATE_MAX] = KDS_STAT_MAX,
};

#endif
