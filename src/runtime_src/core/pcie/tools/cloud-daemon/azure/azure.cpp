// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved

#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/sha.h>
#undef OPENSSL_SUPPRESS_DEPRECATED
#include <curl/curl.h>

#include <cstdio>
#include <cstring>
#include <cassert>
#include <stdlib.h>
#include <thread>
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <exception>
#include <regex>
#include <future>
#include "xrt/detail/xclbin.h"
#include "azure.h"

/*
 * Functions each plugin needs to provide
 */
extern "C" {
int init(mpd_plugin_callbacks *cbs);
void fini(void *mpc_cookie);
}

/*
 * This is the default Azure cloud wireserver IP.
 * Users debugging with a standalone server needs to edit /etc/mpd.conf to
 * specify its own IP, with format, eg
 * restip = 1.1.1.1
 */
static std::string restip_endpoint = "168.63.129.16";
/*
 * Maintain the serialNumber of cards.
 * This is required since during reset, the sysfs entry is not available 
 */
static std::vector<std::string> fpga_serial_number;
/*
 * Init function of the plugin that is used to hook the required functions.
 * The cookie is used by fini (see below). Can be NULL if not required.
 */
int init(mpd_plugin_callbacks *cbs)
{
    int ret = 1;
    auto total = xrt_core::pci::get_dev_total();
    if (total == 0) {
        syslog(LOG_INFO, "azure: no device found");
        return ret;
    }
    if (cbs) 
    {
        // init curl
        int curlInit = curl_global_init(CURL_GLOBAL_ALL);
        if (curlInit != 0)
            syslog(LOG_ERR, "mpd cannot initalize curl: %d", curlInit);
        std::string private_ip = AzureDev::get_wireserver_ip();
        if (!private_ip.empty())
            restip_endpoint = private_ip;
        syslog(LOG_INFO, "azure restserver ip: %s\n", restip_endpoint.c_str());
        fpga_serial_number = AzureDev::get_serial_number();
        // hook functions
        cbs->mpc_cookie = NULL;
        cbs->get_remote_msd_fd = get_remote_msd_fd;
        cbs->mb_req.load_xclbin = azureLoadXclBin;
        cbs->mb_req.hot_reset = azureHotReset;
        ret = 0;
    }
    syslog(LOG_INFO, "azure mpd plugin init called: %d\n", ret);
    return ret;
}

/*
 * Fini function of the plugin
 */
void fini(void *mpc_cookie)
{
     syslog(LOG_INFO, "azure mpd plugin fini called\n");
}

/*
 * callback function that is used to setup communication channel
 * we are going to handle mailbox ourself, no comm channel is required.
 * so just return -1 to the fd
 * Input:
 *        index: index of the user PF
 * Output:
 *        fd: socket handle of the communication channel
 * Return value:
 *        0: success
 *        1: failure
 */
int get_remote_msd_fd(size_t index, int* fd)
{
    *fd = -1;
    return 0;
}

/*
 * callback function that is used to handle MAILBOX_REQ_LOAD_XCLBIN msg
 *
 * Input:
 *        index: index of the FPGA device
 *        xclbin: the fake xclbin file
 * Output:
 *        resp: int as response msg    
 * Return value:
 *        0: success
 *        others: error code
 */
int azureLoadXclBin(size_t index, const axlf *xclbin, int *resp)
{
    AzureDev d(index);
    struct timeval tvStartLoadXclBin, tvEndLoadXclBin;
    gettimeofday(&tvStartLoadXclBin, NULL);
    *resp = d.azureLoadXclBin(xclbin);
    gettimeofday(&tvEndLoadXclBin, NULL);

    double loadxclTime = (tvEndLoadXclBin.tv_sec - tvStartLoadXclBin.tv_sec) * 1000.0; 
    loadxclTime += (tvEndLoadXclBin.tv_usec - tvStartLoadXclBin.tv_usec) / 1000.0;
    std::cout << "time LoadXclBin (" << index << ") = " << (loadxclTime/1000) << std::endl;
    return 0;
}

/*
 * Reset requires the mailbox msg return before the real reset
 * happens. So we run user special reset in async thread.
 */
std::future<void> nouse; //so far we don't care the return value of reset
static void azureHotResetAsync(size_t index)
{
    AzureDev d(index);
    struct timeval tvStartReset, tvEndReset;
    gettimeofday(&tvStartReset, NULL);
    d.azureHotReset();
    gettimeofday(&tvEndReset, NULL);

    double resetTime = (tvEndReset.tv_sec - tvStartReset.tv_sec) * 1000.0; 
    resetTime += (tvEndReset.tv_usec - tvStartReset.tv_usec) / 1000.0;
    std::cout << "time HotReset (" << index << ") = " << (resetTime/1000) << std::endl;
}

/*
 * callback function that is used to handle MAILBOX_REQ_HOT_RESET msg
 *
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        resp: int as response msg    
 * Return value:
 *        0: success
 *        others: error code
 */
int azureHotReset(size_t index, int *resp)
{
    /*
     * Tell xocl don't try to restore anything since we are going
     * to do hotplug in wireserver
     * If we can't get S/N of the card, we are not even going to issue the reset
     * to wireserver since this makes no sense and even hangs the instance.
     * Empty S/N may happen in this scenario,
     *  1. vm boots and is ready before the mgmt side is ready
     *  2. 'xbutil reset' tries to reset the card immediately after mgmt is ready
     *  in this case, there is no chance for mpd to get S/N info. so we just fails
     *  the reset
     */
    if (fpga_serial_number.at(index).empty()) {
        *resp = -E_EMPTY_SN;
    } else {
        *resp = -ESHUTDOWN;
        nouse = std::async(std::launch::async, &azureHotResetAsync, index);
    }
    return 0;
}

//azure specific parts 
static size_t read_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    int ret = 0;
    struct write_unit *unit = static_cast<struct write_unit *>(userp);
    std::string output;
    size_t isize = unit->sizeleft;
    if (!isize)
        return ret;

    ret = (isize < size * nmemb ? isize : size * nmemb);
    memcpy(contents, unit->uptr, ret);
    unit->uptr += ret;
    unit->sizeleft -= ret;

    return ret;
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

int AzureDev::azureLoadXclBin(const xclBin *buffer)
{
    char *xclbininmemory = reinterpret_cast<char*> (const_cast<xclBin*> (buffer));

    if (memcmp(xclbininmemory, "xclbin2", 8) != 0)
        return -1;
    std::string fpgaSerialNumber;
    get_fpga_serialNo(fpgaSerialNumber);

    if (fpgaSerialNumber.empty())
        return -E_EMPTY_SN;
    std::cout << "LoadXclBin FPGA serial No: " << fpgaSerialNumber << std::endl;

    bool allow_unattested_xclbin = false;
    char* env = getenv("ALLOW_UNATTESTED_XCLBIN");
    if (env && !strcmp(env, "true"))
        allow_unattested_xclbin = true;

    // check if the xclbin is valid
    if (!allow_unattested_xclbin &&
        (xclbin::get_axlf_section(buffer, BITSTREAM) != nullptr)) {
        std::cout << "xclbin is invalid, please provide azure xclbin" << std::endl;
        return -E_INVALID_XCLBIN;
    }

    int index = 0;
    std::string imageSHA;
    std::vector<std::string> chunks;
    size_t size = buffer->m_header.m_length;
    std::cout << "xclbin file size (" << fpgaSerialNumber << "): " << size << std::endl;

    // Generate SHA256 for the kernel and
    // separate in segments ready to upload
    int res = Sha256AndSplit(std::string(xclbininmemory, size), chunks, imageSHA);
    if (res) {
        std::cout << "xclbin split failed!" << std::endl;
        return -E_SPLIT;
    }
    std::cout << "xclbin file sha256 (" << fpgaSerialNumber << "): " << imageSHA << std::endl;

    struct timeval tvStartUpload, tvEndUpload;
    std::cout << "Start upload segment (" << fpgaSerialNumber << ")" << std::endl;
    gettimeofday(&tvStartUpload, NULL);
    for (auto &chunk: chunks) {
        if (goingTimeout())
            return -E_REST_TIMEOUT;
        //upload each segment individually
        std::cout << "upload segment (" << fpgaSerialNumber << "): " << index << " size: " << chunk.size() << std::endl;
        if (UploadToWireServer(
            restip_endpoint,
            "machine/plugins/?comp=FpgaController&type=SendImageSegment",
            fpgaSerialNumber,
            chunk,
            index,
            chunks.size(),
            imageSHA))
            return -E_UPLOAD;
        index++;
    }
    gettimeofday(&tvEndUpload, NULL);
    std::cout << "Done upload segment (" << fpgaSerialNumber << ")" << std::endl;
    double uploadTime = (tvEndUpload.tv_sec - tvStartUpload.tv_sec) * 1000.0; 
    uploadTime += (tvEndUpload.tv_usec - tvStartUpload.tv_usec) / 1000.0;
    std::cout << "time upload segment (" << fpgaSerialNumber << ") = " << (uploadTime/1000) << std::endl;

    //start the re-image process
    int retryCounter = 0;
    int sleepDelayStartReimagin[] = {1500, 1500, 1000, 1000, 1500, 1500, 1000, 1000, 1500, 1500, 1000, 1000, 1500, 1500, 1000, 1000};
    std::string delim = ":";
    std::string ret, key, value;

    struct timeval tvStartReimage, tvEndReimage;
    std::cout << "Start reimage process (" << fpgaSerialNumber << ")" << std::endl;
    gettimeofday(&tvStartReimage, NULL);
    do {
        if (goingTimeout())
            return -E_REST_TIMEOUT;
        ret = REST_Get(
            restip_endpoint,
            "machine/plugins/?comp=FpgaController&type=StartReimaging",
            fpgaSerialNumber
        );
        if (splitLine(ret, key, value, delim) != 0 ||
            key.compare("StartReimaging") != 0 ||
            value.compare("0") != 0) {
                msleep(sleepDelayStartReimagin[retryCounter]);
                retryCounter++;
                if (retryCounter >= upload_retry) {
                    std::cout << "Timeout trying to start reimging (" << fpgaSerialNumber << ")..." << std::endl;
                    return -E_START_REIMAGE;
                }
        } else {
            retryCounter = 0;
        }

    } while (retryCounter > 0);
    gettimeofday(&tvEndReimage, NULL);
    std::cout << "Done start reimage (" << fpgaSerialNumber << ")" << std::endl;
    double reimageTime = (tvEndReimage.tv_sec - tvStartReimage.tv_sec) * 1000.0; 
    reimageTime += (tvEndReimage.tv_usec - tvStartReimage.tv_usec) / 1000.0;
    std::cout << "time start reimage (" << fpgaSerialNumber << ") = " << (reimageTime/1000) << std::endl;

    // reconfig takes 8-10 secs as min, per measure, waiting 8000
    msleep(8000);
    //check the re-image status
    int sleepDelayReimageStatus[] = {3000, 2000, 2000 , 1500, 1500, 1500, 1000, 1000, 1500, 1500, 1000, 1000, 1500, 1500, 1000, 1000};
    struct timeval tvStartStatus, tvEndStatus;
    std::cout << "Start reimage Status (" << fpgaSerialNumber << ")" << std::endl;
    gettimeofday(&tvStartStatus, NULL);
    int wait = 0;
    do {
        if (goingTimeout())
            return -E_REST_TIMEOUT;
        ret = REST_Get(
            restip_endpoint,
            "machine/plugins/?comp=FpgaController&type=GetReimagingStatus",
            fpgaSerialNumber
        );
        if (splitLine(ret, key, value, delim) != 0 ||
            key.compare("GetReimagingStatus") != 0) {
            std::cout << "Retrying GetReimagingStatus ... " << std::endl;
            msleep(sleepDelayReimageStatus[wait%15]);
            wait++;
            continue;
        } else if (value.compare("3") != 0) {
            msleep(sleepDelayReimageStatus[wait%15]); 
            wait++;
            continue;
        } else {
            std::cout << "reimaging return status (" << fpgaSerialNumber << "): " << value << " within " << wait << "s" << std::endl;
            gettimeofday(&tvEndStatus, NULL);
            std::cout << "Done reimage status (" << fpgaSerialNumber << ")" << std::endl;
            double statusTime  = (tvEndStatus.tv_sec - tvStartStatus.tv_sec) * 1000.0; 
            statusTime += (tvEndStatus.tv_usec - tvStartStatus.tv_usec) / 1000.0;
            std::cout << "time reimage status (" << fpgaSerialNumber << ") = " << (statusTime/1000) << std::endl;
            return 0;
        }
    } while (wait < rest_timeout);
    std::cout << "Timeout GetImageStatus (" << fpgaSerialNumber << ")..." << std::endl;

    return -E_GET_REIMAGE_STATUS;
}

int AzureDev::azureHotReset()
{
    std::string fpgaSerialNumber;
    get_fpga_serialNo(fpgaSerialNumber);
    std::cout << "HotReset FPGA serial No: " << fpgaSerialNumber << std::endl;
    //start the reset process
    std::string delim = ":";
    std::string ret, key, value;
    int wait = 0;
    do {
        ret = REST_Get(
            restip_endpoint,
            "machine/plugins/?comp=FpgaController&type=Reset",
            fpgaSerialNumber
        );
        syslog(LOG_INFO, "obtained ret = %s from reset call", ret.c_str());
        if (splitLine(ret, key, value, delim) != 0 ||
            key.compare("Reset") != 0 ||
            value.compare("0") != 0) {
            syslog(LOG_INFO, "wasn't expected response...%s", ret.c_str());
            sleep(1);
            wait++;
            continue;
        }
        break;
    } while (wait < reset_retry);

    if (value.compare("0") != 0)
        return -E_RESET;

    // poll wireserver for response TBD
    //check the response
    syslog(LOG_INFO, "poll for reset status...");
    wait = 0;
    do {
        ret = REST_Get(
            restip_endpoint,
            "machine/plugins/?comp=FpgaController&type=GetResetStatus",
            fpgaSerialNumber
        );
        syslog(LOG_INFO, "obtained ret = %s from get reset status call", ret.c_str());
        if (splitLine(ret, key, value, delim) != 0 ||
            key.compare("GetResetStatus") != 0 ||
            value.compare("2") != 0) {
            sleep(1);
            wait++;
            continue;
        } else {
            std::cout << "get reset status return status: " << value << " within " << wait << "s" << std::endl;
            return 0;
        }
    } while (wait < rest_timeout);
    return -E_GET_RESET_STATUS;
}

AzureDev::~AzureDev()
{
}

AzureDev::AzureDev(size_t index) : index(index)
{
    dev = xrt_core::pci::get_dev(index, true);
    gettimeofday(&start, NULL);
}

//private methods
//REST operations using libcurl (-lcurl)
int AzureDev::UploadToWireServer(
    const std::string &ip,
    const std::string &endpoint,
    const std::string &target,
    const std::string &data,
    int index,
    int total,
    const std::string &hash)
{
    CURL *curl;
    CURLcode res;
    struct write_unit unit;
    uint8_t retryCounter = 0;
    uint8_t maxRetryCounter = 15;
    int sleepDelay[] = {1500, 1500, 1000, 1000, 1500, 1500, 1000, 1000, 1500, 1500, 1000, 1000, 1500, 1500, 1000, 1000};
    long responseCode=0;

    unit.uptr = data.c_str();
    unit.sizeleft = data.size();

    curl = curl_easy_init();

    if (curl) {
        std::stringstream urlStream;
        urlStream << "http://" << ip << "/" << endpoint << "&chipid=" << target;
        curl_easy_setopt(curl, CURLOPT_URL, urlStream.str().c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_READDATA, &unit);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

        // HTTP header section
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: octet-stream");

        std::stringstream headerLength;
        headerLength << "Content-Length: " <<  data.size();
        headers = curl_slist_append(headers, headerLength.str().c_str());

        std::stringstream headerChunk;
        headerChunk << "x-azr-chunk: " <<  index;
        headers = curl_slist_append(headers,  headerChunk.str().c_str());

        std::stringstream headerTotal;
        headerTotal << "x-azr-total: " <<  total;
        headers = curl_slist_append(headers,  headerTotal.str().c_str());

        std::stringstream headerHash;
        headerHash << "x-azr-hash: " <<  hash;
        headers = curl_slist_append(headers,  headerHash.str().c_str());

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        do {
            responseCode=0;
            res = curl_easy_perform(curl);

            if (res != CURLE_OK) {
                std::cout << "curl_easy_perform() failed: " <<  curl_easy_strerror(res) << std::endl;
                retryCounter++;
                if (retryCounter <  maxRetryCounter) {
                    std::cout << "Retrying an upload (" << target << ") ..." << retryCounter << std::endl;
                    msleep(sleepDelay[retryCounter-1]);
                } else {
                    std::cout << "Max number of retries reached upload (" << target << ")... givin up1" << std::endl;
                    curl_easy_cleanup(curl);
                    return 1;
                }
            } else {
                // check the return code
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
                std::cout << "DebugUpload: status code (" << target << ") " << responseCode << std::endl;
                if (responseCode >= 400) {
                    // error range
                    res = CURLE_HTTP_RETURNED_ERROR;
                    retryCounter++;
                    if (retryCounter <  maxRetryCounter) {
                        std::cout << "Retrying an upload after http error (" << target << ")..." << retryCounter << std::endl;
                        msleep(sleepDelay[retryCounter-1]);
                    } else {
                        std::cout << "Max number of retries reached upload (" << target << ")... givin up!" << std::endl;
                        curl_easy_cleanup(curl);
                        return 1;
                    }
                } //if (responseCode >= 400)
            } //if (res != CURLE_OK)
        } while (res != CURLE_OK);	

        // cleanup
        curl_easy_cleanup(curl);
        std::cout << "Upload segment (" << target << ") " << index + 1 << " of " << total  << std::endl;
    } else {
        std::cout << "Failed init (" << target << ")..." << std::endl;
    } //if (curl)

    return 0;
}

std::string AzureDev::REST_Get(
    const std::string &ip,
    const std::string &endpoint,
    const std::string &target
)
{
    CURL *curl;
    CURLcode res;
    std::string readbuff = "";
    long responseCode = 0;

    curl = curl_easy_init();
    if (curl) {
        std::stringstream urlStream;
        urlStream << "http://" << ip << "/" << endpoint << "&chipid=" << target;

        curl_easy_setopt(curl, CURLOPT_URL, urlStream.str().c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readbuff);

        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            std::cout <<  "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;

        // check the return code
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        std::cout << "DebugRestGET: (" << target << ") status code " << responseCode << std::endl;
        std::string printstring(readbuff);
        if (printstring.length() > 80)
            printstring.resize(80);

        std::cout << "String RestGET returned (" << target << "): " << printstring << std::endl;
        curl_easy_cleanup(curl);
        //TODO: add code to interpret readbuff to see whether reimage succeeds.
    }
    return readbuff;
}

// use -lcrypto for SHA operations
int AzureDev::Sha256AndSplit(
    const std::string &input,
    std::vector<std::string> &output,
    std::string &sha)
{
    // Initialize openssl
    SHA256_CTX context;
    if (!SHA256_Init(&context)) {
        std::cerr << "Unable to initiate SHA256" << std::endl;
        return 1;
    }

    unsigned pos = 0;

    while (pos < input.size()) {
        std::string segment = input.substr(pos, transfer_segment_size);

        if(!SHA256_Update(&context, segment.c_str(), segment.size()))
        {
            std::cerr << "Unable to Update SHA256 buffer" << std::endl;
            return 1;
        }
        output.push_back(segment);
        pos += transfer_segment_size;
    }

    // Get Final SHA
    unsigned char result[SHA256_DIGEST_LENGTH];
    if(!SHA256_Final(result, &context)) {
        std::cerr << "Error finalizing SHA256 calculation" << std::endl;
        return 1;
    }

    // Convert the byte array into a string
    std::stringstream shastr;
    shastr << std::hex << std::setfill('0');
    for (auto &byte: result)
        shastr << std::setw(2) << (int)byte;

    sha = shastr.str();
    return 0;
}

void AzureDev::get_fpga_serialNo(std::string &fpgaSerialNo)
{
    std::string errmsg;
    dev->sysfs_get("xmc", "serial_num", errmsg, fpgaSerialNo);
    //fpgaSerialNo = "1281002AT024";
    if (fpgaSerialNo.empty())
        fpgaSerialNo = fpga_serial_number.at(index);
    else if (fpga_serial_number.at(index).empty())
        //save the serial in case the already saved is empty
        fpga_serial_number.at(index) = fpgaSerialNo;
    if (!errmsg.empty() || fpgaSerialNo.empty()) {
        std::cerr << "get_fpga_serialNo warning(" << dev->m_sysfs_name << ")";
        std::cerr << " sysfs errmsg: " << errmsg;
        std::cerr << " serialNumber: " << fpga_serial_number.at(index);
        std::cerr << std::endl;
    }
}

void AzureDev::msleep(long msecs)
{
    struct timespec ts;

    ts.tv_sec = msecs / 1000;
    ts.tv_nsec = (msecs % 1000) * 1000000;

    nanosleep(&ts, NULL);
}

int AzureDev::goingTimeout()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    if (now.tv_sec - start.tv_sec > timeout_threshold)
        return 1;
    else
        return 0;
}

