/*
 * Copyright (C) 2018, Xilinx Inc - All rights reserved
 * Xilinx SDAccel Media Accelerator API
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

#include <stdio.h>
#include <stdlib.h>
#include "app/xmabuffers.h"
#include "app/xmaerror.h"
#include "lib/xmaapi.h"
#include "lib/xmacfg.h"
#include "lib/xmaconnect.h"
#include "lib/xmahw.h"

extern XmaSingleton *g_xma_singleton;

// Helper functions
bool
is_zerocopy_enabled(int32_t dev_id);

bool
is_connect_compatible(XmaEndpoint *endpt1,
                      XmaEndpoint *endpt2);

int32_t
xma_connect_alloc(XmaEndpoint *endpt, XmaConnectType type)
{
    int32_t i;
    int32_t c_handle = -1;
    XmaConnect *conntbl = g_xma_singleton->connections;

    // Don't add an entry if zerocopy is disabled
    if (!is_zerocopy_enabled(endpt->dev_id))
        return c_handle;

    // An endpoint is added based on the direction provided
    // If this is a sender, find the first unused connection
    // entry and set the state to pending.
    // NOTE: The connection table is only local to a process
    //       and not kept in shared system memory
    // TODO: It may be necessary to obtain a lock on the connection
    //       table; however, assume for now that connections are
    //       established during initialization and initialization
    //       is all performed from a single thread.  This assumption
    //       may not be true and could create a problem.

    // Find an unused entry for a sender
    if (type == XMA_CONNECT_SENDER)
    {
        for (i = 0; i < MAX_CONNECTION_ENTRIES; i++)
        {
            if (conntbl[i].state == XMA_CONNECT_UNUSED)
            {
                c_handle = i;
                conntbl[i].sender = endpt;
                conntbl[i].state = XMA_CONNECT_PENDING_ACTIVE;
                break;
            }
        }
    }
    // Find a compatible pending entry for a receiver
    if (type == XMA_CONNECT_RECEIVER)
    {
        for (i = 0; i < MAX_CONNECTION_ENTRIES; i++)
        {
            if (conntbl[i].state == XMA_CONNECT_PENDING_ACTIVE)
            {
                XmaEndpoint *cmp_endpt = conntbl[i].sender;
                if (is_connect_compatible(endpt, cmp_endpt))
                {
                    printf("xmaconnect: compatible connection found\n");
                    c_handle = i;
                    conntbl[i].receiver = endpt;
                    conntbl[i].state = XMA_CONNECT_ACTIVE;
                    break;
                }
            }
        }
    }
    return c_handle;
}

int32_t
xma_connect_free(int32_t c_handle, XmaConnectType type)
{
    XmaConnect *conntbl = g_xma_singleton->connections;

    if (c_handle == -1)
        return XMA_SUCCESS;

    if (type == XMA_CONNECT_SENDER &&
        conntbl[c_handle].sender != NULL)
    {
        free(conntbl[c_handle].sender);
        conntbl[c_handle].sender = NULL;
        if (conntbl[c_handle].state == XMA_CONNECT_ACTIVE &&
            conntbl[c_handle].receiver == NULL)
            conntbl[c_handle].state = XMA_CONNECT_UNUSED;
        else
            conntbl[c_handle].state = XMA_CONNECT_PENDING_DELETE;
    }

    if (type == XMA_CONNECT_RECEIVER &&
        conntbl[c_handle].receiver != NULL)
    {
        free(conntbl[c_handle].receiver);
        conntbl[c_handle].receiver = NULL;
        if (conntbl[c_handle].state == XMA_CONNECT_PENDING_DELETE &&
            conntbl[c_handle].sender == NULL)
            conntbl[c_handle].state = XMA_CONNECT_UNUSED;
        else
            conntbl[c_handle].state = XMA_CONNECT_PENDING_DELETE;
    }

   return XMA_SUCCESS;
}

bool
is_zerocopy_enabled(int32_t dev_id)
{
    int32_t i, j;
    bool zerocopy = false;
    bool found = false;
    XmaSystemCfg *systemcfg = &g_xma_singleton->systemcfg;

    // Iterate through the images looking for a matching
    // device id and check if zerocopy is enabled
    for (i = 0; i < systemcfg->num_images; i++)
    {
        for (j = 0; j < systemcfg->imagecfg[i].num_devices; j++)
        {
            if (systemcfg->imagecfg[i].device_id_map[j] == dev_id)
            {
                zerocopy = systemcfg->imagecfg[i].zerocopy;
                found = true;
                break;
            }
        }
        if (found)
            break;
    }

    printf("xmaconnect: zerocopy enable = %d\n", zerocopy);
    return zerocopy;
}

bool
is_connect_compatible(XmaEndpoint *endpt1,
                      XmaEndpoint *endpt2)
{
    XmaHwSession *hw1 = &endpt1->session->hw_session;
    XmaHwSession *hw2 = &endpt2->session->hw_session;

    // Can't check format because of scaler plugin BUG
    //       "endpt1->format  %d, endpt2->format  %d\n"
    //        endpt1->format,  endpt2->format,
    printf("hw1->dev_handle %p, hw2->dev_handle %p\n"
           "hw1->ddr_bank   %d, hw2->ddr_bandk  %d\n"
           "endpt1->bpp     %d, endpt2->bpp     %d\n"
           "endpt1->width   %d, endpt2->width   %d\n"
           "endpt1->height  %d, endpt2->height  %d\n",
            hw1->dev_handle, hw2->dev_handle,
            hw1->ddr_bank,   hw2->ddr_bank,
            endpt1->bits_per_pixel, endpt2->bits_per_pixel,
            endpt1->width,   endpt2->width,
            endpt1->height,  endpt2->height);

    // Can't check format because of scaler plugin BUG
    //        endpt1->format         == endpt2->format         &&
    return (hw1->dev_handle        == hw2->dev_handle        &&
            hw1->ddr_bank          == hw2->ddr_bank          &&
            endpt1->bits_per_pixel == endpt2->bits_per_pixel &&
            endpt1->width          == endpt2->width          &&
            endpt1->height         == endpt2->height);
}
