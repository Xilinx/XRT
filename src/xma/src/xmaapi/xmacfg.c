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
#include <yaml.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include "app/xmaerror.h"
#include "lib/xmacfg.h"

/* Data structure used by state transition functions */
typedef struct XmaData
{
    int              state_idx;
    int              key_no;
    yaml_document_t *document;
    yaml_node_t     *node;
    int              node_idx;
    XmaSystemCfg    *systemcfg;
    int              imagecfg_idx;
    int              kernelcfg_idx;
} XmaData;

/* Prototypes for local state transition functions */
static int validate_node_key(char *key, yaml_node_t *node, int key_no);
static int check_systemcfg(XmaData *data);
static int set_logfile(XmaData *data);
static int set_loglevel(XmaData *data);
static int set_dsa(XmaData *data);
static int set_pluginpath(XmaData *data);
static int set_xclbinpath(XmaData *data);
static int check_imagecfg(XmaData *data);
static int set_xclbin(XmaData *data);
static int set_zerocopy(XmaData *data);
static int set_device_id_map(XmaData *data);
static int check_kernelcfg(XmaData *data);
static int set_instances(XmaData *data);
static int set_function(XmaData *data);
static int set_plugin(XmaData *data);
static int set_vendor(XmaData *data);
static int set_name(XmaData *data);
static int set_ddr_map(XmaData *data);

static
yaml_node_t *get_next_scalar_node(yaml_document_t *document,
                                  int             *node_idx);

static bool is_end_of_num_sequence(yaml_node_t *node);

static
int run_state_machine(yaml_document_t *document,
                      XmaSystemCfg    *systemcfg);

static int find_state_entry(char *key);

/* State transition data structure */
typedef struct XmaSystemCfgSM
{
    char    *key;
    int      (*transition)(XmaData *data);
    bool     is_required;
} XmaSystemCfgSM;

/* State table used to populate XmaSystemCfg data structure from YAML */
static XmaSystemCfgSM systemcfg_sm[] = {
{ "SystemCfg",     &check_systemcfg,   true },
{ "logfile",       &set_logfile,       false },
{ "loglevel",      &set_loglevel,      false },
{ "dsa",           &set_dsa,           true },
{ "pluginpath",    &set_pluginpath,    true },
{ "xclbinpath",    &set_xclbinpath,    true },
{ "ImageCfg",      &check_imagecfg,    true },
{ "xclbin",        &set_xclbin,        true },
{ "zerocopy",      &set_zerocopy,      true },
{ "device_id_map", &set_device_id_map, true },
{ "KernelCfg",     &check_kernelcfg,   true },
{ "instances",     &set_instances,     true },
{ "function",      &set_function,      true },
{ "plugin",        &set_plugin,        true },
{ "vendor",        &set_vendor,        true },
{ "name",          &set_name,          true },
{ "ddr_map",       &set_ddr_map,       false },
{ NULL,            NULL},
};

#define xma_cfg_log_err(fmt,args...) fprintf(stderr, "XMA CFG: " fmt, ##args)

int check_systemcfg(XmaData *data)
{
    data->state_idx++;

    return XMA_SUCCESS;
}

int set_logfile(XmaData *data)
{
    yaml_node_t *next_node;

    next_node = get_next_scalar_node(data->document, &data->node_idx);
    strncpy(data->systemcfg->logfile,
           (const char*)next_node->data.scalar.value, (NAME_MAX-1));
    data->state_idx++;
    data->systemcfg->logger_initialized = true;

    return XMA_SUCCESS;
}

int set_loglevel(XmaData *data)
{
    yaml_node_t *next_node;

    next_node = get_next_scalar_node(data->document, &data->node_idx);
    data->systemcfg->loglevel =
           atoi((const char*)next_node->data.scalar.value);
    data->state_idx++;

    return XMA_SUCCESS;
}

int set_dsa(XmaData *data)
{
    yaml_node_t *next_node;

    next_node = get_next_scalar_node(data->document, &data->node_idx);
    strncpy(data->systemcfg->dsa,
           (const char*)next_node->data.scalar.value, (MAX_DSA_NAME-1));
    data->state_idx++;

    return XMA_SUCCESS;
}

int set_pluginpath(XmaData *data)
{
    yaml_node_t *next_node;

    next_node = get_next_scalar_node(data->document, &data->node_idx);
    strncpy(data->systemcfg->pluginpath,
           (const char*)next_node->data.scalar.value, (NAME_MAX-1));
    data->state_idx++;

    return XMA_SUCCESS;
}

int set_xclbinpath(XmaData *data)
{
    yaml_node_t *next_node;

    next_node = get_next_scalar_node(data->document, &data->node_idx);
    strncpy(data->systemcfg->xclbinpath,
           (const char*)next_node->data.scalar.value, (NAME_MAX-1));
    data->state_idx++;

    return XMA_SUCCESS;
}

int check_imagecfg(XmaData *data)
{
    data->imagecfg_idx++;
    data->systemcfg->num_images++;
    data->systemcfg->imagecfg[data->imagecfg_idx].num_kernelcfg_entries = 0;
    data->state_idx++;

    return XMA_SUCCESS;
}

int set_xclbin(XmaData *data)
{
    yaml_node_t *next_node;
    int          i = data->imagecfg_idx;

    next_node = get_next_scalar_node(data->document, &data->node_idx);
    strncpy(data->systemcfg->imagecfg[i].xclbin,
           (const char*)next_node->data.scalar.value, (NAME_MAX-1));
    data->state_idx++;

    return XMA_SUCCESS;
}

int set_zerocopy(XmaData *data)
{
    yaml_node_t *next_node;
    int          i = data->imagecfg_idx;

    next_node = get_next_scalar_node(data->document, &data->node_idx);
    if (strcmp((const char*)next_node->data.scalar.value, "enable") == 0)
        data->systemcfg->imagecfg[i].zerocopy = true;
    else
        data->systemcfg->imagecfg[i].zerocopy = false;
    data->state_idx++;

    return XMA_SUCCESS;
}

int set_device_id_map(XmaData *data)
{
    int          i = data->imagecfg_idx;
    int          m = 0;

    while (1)
    {
        yaml_node_t *next_node = get_next_scalar_node(data->document,
                                                     &data->node_idx);
        if (is_end_of_num_sequence(next_node))
        {
            data->node_idx--;
            break;
        }
        data->systemcfg->imagecfg[i].device_id_map[m] =
            atoi((const char*)next_node->data.scalar.value);
        data->systemcfg->imagecfg[i].num_devices++;
        m++;
    }
    data->state_idx++;

    return XMA_SUCCESS;
}

int check_kernelcfg(XmaData *data)
{
    int i = data->imagecfg_idx;

    data->kernelcfg_idx = 0;
    data->systemcfg->imagecfg[i].num_kernelcfg_entries++;
    data->state_idx++;

    return XMA_SUCCESS;
}

int set_instances(XmaData *data)
{
    yaml_node_t *next_node;
    int          i = data->imagecfg_idx;
    int          k = data->kernelcfg_idx;

    next_node = get_next_scalar_node(data->document, &data->node_idx);
    data->systemcfg->imagecfg[i].kernelcfg[k].instances =
        atoi((const char*)next_node->data.scalar.value);
    data->state_idx++;

    return XMA_SUCCESS;
}

int set_function(XmaData *data)
{
    yaml_node_t *next_node;
    int          i = data->imagecfg_idx;
    int          k = data->kernelcfg_idx;

    next_node = get_next_scalar_node(data->document, &data->node_idx);
    strncpy(data->systemcfg->imagecfg[i].kernelcfg[k].function,
           (const char*)next_node->data.scalar.value, (MAX_FUNCTION_NAME-1));
    data->state_idx++;

    return XMA_SUCCESS;
}

int set_plugin(XmaData *data)
{
    yaml_node_t *next_node;
    int          i = data->imagecfg_idx;
    int          k = data->kernelcfg_idx;

    next_node = get_next_scalar_node(data->document, &data->node_idx);
    strncpy(data->systemcfg->imagecfg[i].kernelcfg[k].plugin,
           (const char*)next_node->data.scalar.value, (MAX_PLUGIN_NAME-1));
    data->state_idx++;

    return XMA_SUCCESS;
}

int set_vendor(XmaData *data)
{
    yaml_node_t *next_node;
    int          i = data->imagecfg_idx;
    int          k = data->kernelcfg_idx;

    next_node = get_next_scalar_node(data->document, &data->node_idx);
    strncpy(data->systemcfg->imagecfg[i].kernelcfg[k].vendor,
           (const char*)next_node->data.scalar.value, (MAX_VENDOR_NAME-1));
    data->state_idx++;

    return XMA_SUCCESS;
}

int set_name(XmaData *data)
{
    yaml_node_t *next_node;
    int          i = data->imagecfg_idx;
    int          k = data->kernelcfg_idx;

    next_node = get_next_scalar_node(data->document, &data->node_idx);
    strncpy(data->systemcfg->imagecfg[i].kernelcfg[k].name,
           (const char*)next_node->data.scalar.value,
           (MAX_KERNEL_NAME-1));
    data->state_idx++;

    return XMA_SUCCESS;
}

int set_ddr_map(XmaData *data)
{
    xma_cfg_log_err("ddr_map field found in cfg file. This is depricated\n");
    xma_cfg_log_err("This will be ignored and it will be derived from xclbin!\n");
    yaml_node_t *next_node;
    int          i = data->imagecfg_idx;
    int          k = data->kernelcfg_idx;
    int          m, instances;

    instances = data->systemcfg->imagecfg[i].kernelcfg[k].instances;
    for (m = 0; m < instances; m++)
    {
        next_node = get_next_scalar_node(data->document, &data->node_idx);
        if (next_node == NULL)
        {
            data->node_idx--;
            break;
        }

        if (is_end_of_num_sequence(next_node))
            break;

        // data->systemcfg->imagecfg[i].kernelcfg[k].ddr_map[m] =
        //     atoi((const char*)next_node->data.scalar.value);
    }

    if (m < instances - 1)
    {
        xma_cfg_log_err("Number of items in ddr_map less than expected\n");
        xma_cfg_log_err("   Expected %d found %d\n", instances -1, m);
        return XMA_ERROR_INVALID;
    }

    return XMA_SUCCESS;
}

int find_state_entry(char *key)
{
    int i = 0;
    XmaSystemCfgSM *state_entry = &systemcfg_sm[i];

    while (state_entry->key)
    {
        state_entry = &systemcfg_sm[i];
        if (strcmp(key, state_entry->key) == 0)
            break;
        i++;
    }

    return i;
}

yaml_node_t *get_next_scalar_node(yaml_document_t *document,
                                  int             *node_idx)
{
    yaml_node_t      *node;
    int               i = *node_idx + 1;
    while (1)
    {
        node = yaml_document_get_node(document, i);
        if (!node)
            break;
        if (node->type == YAML_SCALAR_NODE)
        {
            break;
        }
        i++;
    }

    *node_idx = i;
    return node;
}

static bool is_end_of_num_sequence(yaml_node_t *node)
{
    int i;

    /* We only expect two digit numbers */
    if (node->data.scalar.length > 2)
        return true;

    /* Every character must be a digit 0-9 */
    for (i = 0; i < node->data.scalar.length; i++)
    {
        if (!isdigit(node->data.scalar.value[i]))
            return true;
    }

    return false;

}

int run_state_machine(yaml_document_t *document,
                      XmaSystemCfg    *systemcfg)
{
    int               rc = -1;
    XmaSystemCfgSM   *state_entry;
    XmaData           data;
    bool              get_next = true;

    data.document = document;
    data.state_idx = 0;
    data.key_no = 1;
    data.node_idx = 1;
    data.systemcfg = systemcfg;
    data.imagecfg_idx = -1;
    data.kernelcfg_idx = -1;

    state_entry = &systemcfg_sm[data.state_idx];


    while (state_entry->key)
    {
        if (get_next)
        {
            data.node = get_next_scalar_node(data.document, &data.node_idx);
            if (!data.node)
                return XMA_ERROR;
        }
        if (validate_node_key(state_entry->key, data.node, data.key_no))
        {
            if (!state_entry->is_required)
            {
                get_next = false;
                data.key_no++;
                data.state_idx++;
                state_entry = &systemcfg_sm[data.state_idx];
                continue;
            }
                return XMA_ERROR;
        }
        get_next = true;
        rc = state_entry->transition(&data);
        if (rc == XMA_ERROR)
            break;

        /* The next node could be any of the following possibilities:
         * NULL = end of configuration - we are done
         * key = "instances" - we have one or more kernels to configure
         * key = "ImageCfg" - we have one or more images to configure
         */
        yaml_node_t *next_node;
        int i = data.imagecfg_idx;
        if(!strcmp(state_entry->key,"name") || !strcmp(state_entry->key,"ddr_map"))
        {
            next_node = get_next_scalar_node(data.document, &data.node_idx);
            if (!next_node)
                data.state_idx++;
            else
            {
                data.state_idx =
                    find_state_entry((char*)next_node->data.scalar.value);

                if (strcmp("instances",
                        (const char*)next_node->data.scalar.value) == 0)
                {
                    data.kernelcfg_idx++;
                    if (i >= 0) {
                        data.systemcfg->imagecfg[i].num_kernelcfg_entries++;
                    }
                }
                /* Backup to previous node */
                data.node_idx--;
            }
        }
        data.key_no++;
        state_entry = &systemcfg_sm[data.state_idx];
    }

    return rc;
}

int xma_cfg_parse(char *fname, XmaSystemCfg *systemcfg)
{
    yaml_parser_t    parser;
    yaml_document_t  document;
    int              rc;

    printf("Loading '%s': \n", fname);

    FILE *file = fopen(fname, "rb");
    if (file == NULL)
    {
        xma_cfg_log_err("Failed to open file %s\n", fname);
        return XMA_ERROR;
    }

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, file);

    if (!yaml_parser_load(&parser, &document))
    {
        printf("Failed to load document in %s\n", fname);
        printf("Error: %s at line %lu column %lu\n",
               parser.problem,
               parser.problem_mark.line, parser.problem_mark.column);
        printf("     : %s\n", parser.context);
        return XMA_ERROR;
    }

    rc = run_state_machine(&document, systemcfg);
    yaml_document_delete(&document);
    yaml_parser_delete(&parser);

    fclose(file);

    return rc;
}

int validate_node_key(char *key, yaml_node_t *node, int key_no)
{
    if (strcmp(key, (const char*)node->data.scalar.value) != 0) {
        xma_cfg_log_err("Missing %s property on key %d in yaml config file\n",
                        key, key_no);
        return XMA_ERROR_INVALID;
    }
    return XMA_SUCCESS;
}
