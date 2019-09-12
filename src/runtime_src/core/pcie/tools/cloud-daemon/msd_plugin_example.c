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

/*
 * This is a sample implementation of the user plugin for xilinx FPGA software
 * mailbox(a.k.a mpd/msd for user/mgmt PF).
 * The cloud vendors need to implement their own plugin if they want to do something like
 * xclbin protection. 
 * For example, cloud vendor can strip the bitstream section off the 
 * real xclbin file to create a fake xclbin, and save the real xclbin in a private database
 * in which the real xclbin can be indexed by the md5sum of the fake xclbin. In this case,
 * the user app run as normal loading xclbin(with fake xclbin file). The mpd/msd will call
 * into the plugin to get the real xclbin and load it on user's behalf.
 *
 * The user plugin is a shared library which is built with, eg.
 *
 * #gcc -shared -o msd_plugin.so -Wall -Werror -fpic msd_plugin_example.c -l crypto
 *
 * and the plugin is put on host machinche at location
 *
 * /lib/firmware/xilinx/msd_plugin.so
 */ 

#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <syslog.h>
#include <openssl/md5.h>
#include <unistd.h>
#include "msd_plugin.h"

/*
 * Functions each plguin must provide
 */
int init(struct msd_plugin_callbacks *cbs);
void fini(void *mpc_cookie);
void retrieve_xclbin_cb(void *arg, char *xclbin, size_t len);
int retrieve_xclbin(char *orig_xclbin, size_t orig_xclbin_len,
    char **xclbin, size_t *xclbin_len, retrieve_xclbin_fini_fn *cb, void **arg);

/*
 * Internal functions this sample plugin uses
 */ 
static int example_get_xclbin(char *orig_xclbin, size_t orig_xclbin_len,
    char **xclbin, size_t *xclbin_len);
static void calculate_md5(char *md5, char *buf, size_t len);
static void read_xclbin_file(const char* filename, char **xclbin, size_t *xclbin_len);

/*
 * Internal data structures this sample plugin uses
 */ 
/*
 * Entry in the xclbin repository. 
 * This is only for the sample code usage. Cloud vendor has freedom to define
 * its own in terms of their own implementation
 */ 
struct xclbin_repo {
    const char *md5; //md5 of the xclbin metadata. should be the primary key of DB of the repo
    const char *path; //path to xclbin file
};

/* The fake xclbin file transmitted through mailbox is achieved by
 * #xclbinutil --input path_to_xclbin --remove-section BITSTREAM --output path_to_fake_xclbin
 * --skip-uuid-insertion
 * this new fake xclbin has same uuid to the real xclbin
 *
 * md5 of the fake xclbin can be achieved by
 * #md5sum path_to_fake_xclbin
 *
 * This md5 is the primary key of the repo database to get the real xclbin
 */ 
struct xclbin_repo repo[2] = {
    {
        .md5 = "7523f10fc420edcc2b3c90093dc738df",
        .path = "/opt/xilinx/dsa/xilinx_u250_xdma_201830_1/test/verify.xclbin",
    },
    {
        .md5 = "56e9325876700cf246826bd2c718f6be",
        .path = "/opt/xilinx/dsa/xilinx_u250_xdma_201830_1/test/bandwidth.xclbin",
    },
}; // there are only 2 xclbins in the sample code

/*
 * Init function of the plugin that is used to hook the required functions.
 * So far there is only one hook function required to get real xclbin from the fake one.
 * The cookie is used by fini (see below). Can be NULL if not required.
 */ 
int init(struct msd_plugin_callbacks *cbs)
{
    int ret = 1;
    if (cbs) {
        // hook functions
        cbs->mpc_cookie = NULL;
        cbs->retrieve_xclbin = retrieve_xclbin;
        ret = 0;
    }
    syslog(LOG_INFO, "plugin init called\n");
    return ret;
}

/*
 * Fini function of the plugin
 */ 
void fini(void *mpc_cookie)
{
     syslog(LOG_INFO, "plugin fini called\n");
}

/*
 * callback function after the xclbin is loaded. eg. free the buf of the xclbin
 */ 
void retrieve_xclbin_cb(void *arg, char *xclbin, size_t len)
{
    syslog(LOG_INFO, "plugin callback called\n");
    free(xclbin);
}

/*
 * hook function to get the real xclbin from the fake one.
 * Input:
 *     orig_xclbin:  pointer to fake xclbin
 *     orig_xclbin_len: length of fake xclbin
 * Output:    
 *     xclbin: pointer to real xclbin
 *     xclbin_len: length of real xclbin
 *     cb: pointer to callback function
 *     arg: pointer to callback function arg0
 * Return value:
 *     0: success
 *     1: failure
 *
 * Note:
 *     buffer of the real xclbin is allocated by the plugin and is used by msd.
 *     callback function is used to free the buffer after xclbin is loaded.             
 */ 
int retrieve_xclbin(char *orig_xclbin, size_t orig_xclbin_len,
    char **xclbin, size_t *xclbin_len, retrieve_xclbin_fini_fn *cb, void **arg)
{
    syslog(LOG_INFO, "plugin retrieve_xclbin called(orig_xclbin_len = %ld)\n", orig_xclbin_len);
    if (!orig_xclbin || !orig_xclbin_len || !xclbin || !xclbin_len || !cb || !arg)
        return 1;

    if (example_get_xclbin(orig_xclbin, orig_xclbin_len, xclbin, xclbin_len))
        return 1;

    *cb = retrieve_xclbin_cb;
    *arg = NULL;
    return 0;
}

/*
 * Internal functions that are used by this sample plugin implementation.
 * Users may implement their own.
 */ 

/*
 * Sample code to implement the hook function that is used to get real xclbin
 * To simplify, linear search is being used.
 */ 
static int example_get_xclbin(char *orig_xclbin, size_t orig_xclbin_len,
    char **xclbin, size_t *xclbin_len)
{
    char md5[33];
    calculate_md5(md5, orig_xclbin, orig_xclbin_len);
    for (int i= 0; i < sizeof(repo)/sizeof(struct xclbin_repo); i++) {
        if (strcmp(md5, repo[i].md5) == 0) {
                read_xclbin_file(repo[i].path, xclbin, xclbin_len);
                return 0;
        }
    }
    return 1;
}

/*
 * Calculate the md5sum of the fake xclbin. this checksum is acted as the primary
 * key to the real xclbin Database.
 */ 
static void calculate_md5(char *md5, char *buf, size_t len)
{
    unsigned char s[16];
    MD5_CTX context;
    MD5_Init(&context);
    MD5_Update(&context, buf, len);
    MD5_Final(s, &context);

    for (int i = 0; i < 16; i++)
        snprintf(&(md5[i*2]), 3,"%02x", s[i]);
    md5[33] = 0;
}

/*
 * Example of getting real xclbin from repo database
 */ 
static void read_xclbin_file(const char* filename, char **xclbin, size_t *xclbin_len)
{
    char *source = NULL;
    long bufsize = 0;
    size_t newLen = 0;
    FILE *fp = fopen(filename, "r");
    if (fp != NULL) {
        if (fseek(fp, 0L, SEEK_END) == 0) {
        /* Get the size of the file. */
            bufsize = ftell(fp);
            if (bufsize == -1) { /* Error */ }

            printf("%s length: %ld\n", filename, bufsize);
            /* Allocate our buffer to that size. */
            source = malloc(sizeof(char) * (bufsize + 1));

            /* Go back to the start of the file. */
            if (fseek(fp, 0L, SEEK_SET) != 0) { /* Error */ }

            /* Read the entire file into memory. */
            newLen = fread(source, sizeof(char), bufsize, fp);
            if ( ferror( fp ) != 0 ) {
                fputs("Error reading file", stderr);
            } else {
                source[newLen++] = '\0'; /* Just to be safe. */
            }
        }
    }
    fclose(fp);
    *xclbin = source;
    *xclbin_len = newLen-1;
}

#if 0
int main() 
{
    char *xclbin, *nxclbin;
    size_t len, nlen;
    char md5[33];
    int ret;

    read_xclbin_file("./fake_verify_201830_1.xclbin", &xclbin, &len);
    calculate_md5(md5, xclbin, len);
    printf("verify xclbin: len(%ld) md5(%s)\n", len, md5);

    ret = example_get_xclbin(xclbin, len, &nxclbin, &nlen);
    printf("ret = %d xclbin len = %ld\n",ret, nlen);
    retrieve_xclbin_cb(NULL, xclbin, len);

    read_xclbin_file("./fake_bandwidth_201830_1.xclbin", &xclbin, &len);
    calculate_md5(md5, xclbin, len);
    printf("bandwidth xclbin: len(%ld) md5(%s)\n", len, md5);

    ret = example_get_xclbin(xclbin, len, &nxclbin, &nlen);
    printf("ret = %d xclbin len = %ld\n",ret, nlen);
    retrieve_xclbin_cb(NULL, xclbin, len);

    return (0);
} 
#endif
