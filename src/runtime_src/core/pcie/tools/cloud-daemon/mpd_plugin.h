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

/* Declaring interfaces for mpd plugin. */
/* For all functions return int, 0 = success, negative value indicate error. */

#ifndef MPD_PLUGIN_H
#define MPD_PLUGIN_H

#include "xrt/detail/xclbin.h"

typedef int (*get_remote_msd_fd_fn)(size_t index, int *fd);
typedef int (*mb_notify_fn)(size_t index, int fd, bool online);
typedef int (*hot_reset_fn)(size_t index, int *resp);
typedef int (*load_xclbin_fn)(size_t index, const axlf *buf, int *resp);
typedef int (*load_slot_xclbin_fn)(size_t index, const char *buf, int *resp);
typedef int (*reclock2_fn)(size_t index, const struct xclmgmt_ioc_freqscaling *obj, int *resp);
typedef int (*get_icap_data_fn)(size_t index, struct xcl_pr_region *resp);
typedef int (*get_sensor_data_fn)(size_t index, struct xcl_sensor *resp);
typedef int (*get_board_info_fn)(size_t index, struct xcl_board_info *resp);
typedef int (*get_mig_data_fn)(size_t index, char *resp, size_t resp_len);
typedef int (*get_firewall_data_fn)(size_t index, struct xcl_mig_ecc *resp);
typedef int (*get_dna_data_fn)(size_t index, struct xcl_dna *resp);
typedef int (*get_subdev_data_fn)(size_t index, char *resp, size_t resp_len);
typedef int (*user_probe_fn)(size_t index, struct xcl_mailbox_conn_resp *resp);
typedef int (*program_shell_fn)(size_t index, int *resp);
typedef int (*read_p2p_bar_addr_fn)(size_t index, const struct xcl_mailbox_p2p_bar_addr *addr, int *resp);

/*
 * hook functions or cookie set by the plugin
 */
struct mpd_plugin_callbacks {
    /*
     * cookie is set by init_fn and used by fini_fn to do any cleaning
     * work specific to the plugin. See below for init_fn and fini_fn
     */
    void *mpc_cookie;
    /*
     * The hook function that is used to setup communication channel to
     * the msd daemon. By default, msd and mpd talk to each other through
     * a tcp socket. If the vendors want to setup a different type of
     * socket, they can implemente this hook function. If they don't want
     * to leverage the msd daemon, they can just run return a -1 to the
     * fd -- this is the case for most public cloud vendors so that they
     * have more controls on the xclbin downloading.
     */
    get_remote_msd_fd_fn get_remote_msd_fd;
    /*
     * Function to notify software mailbox online/offline.
     * For those without xclmgmt driver, this hook function is used to notify
     * the xocl that imagined mgmt is online/offline. 
     */
    mb_notify_fn mb_notify;
    /*
     * The following are all hook functions handling software mailbox msg
     * that initialized from xocl driver
     * Now all hook functions are mandatory. For example, vendors using
     * Xilinx FPGA boards have all mailbox msg other than xclbin download
     * gone through HW mailbox, then they only need to implement the
     * 'load_xclbin_fn' below. For vendors using their own FPGA boards,
     * say AWS, they may need to implement more hook functions depending
     * on how many features their own HW have.
     */
    struct {
        hot_reset_fn hot_reset; //5 optional
        load_xclbin_fn load_xclbin; //8 mandatory
        reclock2_fn reclock2; //9 optional
        struct {
        	get_icap_data_fn get_icap_data;
        	get_sensor_data_fn get_sensor_data;
        	get_board_info_fn get_board_info;
        	get_mig_data_fn get_mig_data;
        	get_firewall_data_fn get_firewall_data;
        	get_dna_data_fn get_dna_data;
        	get_subdev_data_fn get_subdev_data;
        } peer_data; //10 optional
        user_probe_fn user_probe; //11 mandatory for customized HW
        program_shell_fn program_shell; //14 optional
        read_p2p_bar_addr_fn read_p2p_bar_addr; //15 optional
        load_slot_xclbin_fn load_slot_xclbin; //18 mandatory
    } mb_req;
};

#define INIT_FN_NAME    "init"
#define FINI_FN_NAME    "fini"
/*
 * These 2 functions are mandatory for all mpd plugins.
 * init_fn is used to hook all functions the plugin implements.
 * fini_fn is used to do any cleaning work required when the plugin
 * exits
 */
typedef int (*init_fn)(struct mpd_plugin_callbacks *cbs);
typedef void (*fini_fn)(void *mpc_cookie);

#endif // MPD_PLUGIN_H
