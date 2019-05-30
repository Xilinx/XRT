/**
 * Copyright (C) 2019 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

/* Declaring interfaces for msd plugin. */
/* For all functions return int, 0 = success, negative value indicate error. */

#ifndef	MSD_PLUGIN_H
#define	MSD_PLUGIN_H

typedef void (*retrieve_xclbin_fini_fn)(void *arg, char *xclbin, size_t len);
typedef int (*retrieve_xclbin_fn)(char *orig_xclbin, size_t orig_xclbin_len,
    char **xclbin, size_t *xclbin_len, retrieve_xclbin_fini_fn *cb, void **arg);

struct msd_plugin_callbacks {
    void *mpc_cookie;
    retrieve_xclbin_fn retrieve_xclbin; 
};

#define INIT_FN_NAME    "init"
#define FINI_FN_NAME    "fini"
typedef int (*init_fn)(struct msd_plugin_callbacks *cbs);
typedef void (*fini_fn)(void *mpc_cookie);

#endif	// MSD_PLUGIN_H
