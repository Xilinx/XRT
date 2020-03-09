/*
 * Partial Copyright (C) 2019-2020 Xilinx, Inc
 *
 * Microsoft provides sample code how RESTful APIs are being called 
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

#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <openssl/sha.h>
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
#include "xclbin.h"
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
static std::string RESTIP_ENDPOINT = "168.63.129.16";
/*
 * Maintain the serialNumber of cards.
 * This is required since during reset, the sysfs entry is not available 
 */
static std::vector<std::string> FPGA_SERIAL_NUMBER;
/*
 * Init function of the plugin that is used to hook the required functions.
 * The cookie is used by fini (see below). Can be NULL if not required.
 */
int init(mpd_plugin_callbacks *cbs)
{
    int ret = 1;
    auto total = pcidev::get_dev_total();
    if (total == 0) {
        syslog(LOG_INFO, "azure: no device found");
        return ret;
    }
    if (cbs) 
    {
        std::string private_ip = AzureDev::get_wireserver_ip();
        if (!private_ip.empty())
            RESTIP_ENDPOINT = private_ip;
        syslog(LOG_INFO, "azure restserver ip: %s\n", RESTIP_ENDPOINT.c_str());
        FPGA_SERIAL_NUMBER = AzureDev::get_serial_number();
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
    *resp = d.azureLoadXclBin(xclbin);
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
    d.azureHotReset();
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
    //tell xocl don't try to restore anything since we are going
    //to do hotplug in wireserver
    *resp = -ESHUTDOWN;
    nouse = std::async(std::launch::async, &azureHotResetAsync, index);
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
    std::cout << "FPGA serial No: " << fpgaSerialNumber << std::endl;
    int index = 0;
    std::string imageSHA;
    std::vector<std::string> chunks;
    size_t size = buffer->m_header.m_length;
    std::cout << "xclbin file size: " << size << std::endl;

    // Generate SHA256 for the kernel and
    // separate in segments ready to upload
    int res = Sha256AndSplit(std::string(xclbininmemory, size), chunks, imageSHA);
    if (res) {
        std::cout << "xclbin split failed!" << std::endl;
        return -EFAULT;
    }
    std::cout << "xclbin file sha256: " << imageSHA << std::endl;

    for (auto &chunk: chunks)
    {
        //upload each segment individually
        std::cout << "upload segment: " << index << " size: " << chunk.size() << std::endl;
        UploadToWireServer(
            RESTIP_ENDPOINT,
            "machine/plugins/?comp=FpgaController&type=SendImageSegment",
            fpgaSerialNumber,
            chunk,
            index,
            chunks.size(),
            imageSHA);
           index++;
    }

    //start the re-image process
    std::string delim = ":";
    std::string ret, key, value;
    ret = REST_Get(
        RESTIP_ENDPOINT,
        "machine/plugins/?comp=FpgaController&type=StartReimaging",
        fpgaSerialNumber
    );
    if (splitLine(ret, key, value, delim) != 0 ||
        key.compare("StartReimaging") != 0 ||
        value.compare("0") != 0)
        return -EFAULT;

    //check the re-image status
    int wait = 0;
    do {
        ret = REST_Get(
            RESTIP_ENDPOINT,
            "machine/plugins/?comp=FpgaController&type=GetReimagingStatus",
            fpgaSerialNumber
        );
        if (splitLine(ret, key, value, delim) != 0 ||
            key.compare("GetReimagingStatus") != 0) {
            std::cout << "Retrying GetReimagingStatus ... " << std::endl;
            sleep(1);
            continue;
        } else if (value.compare("3") != 0) {
            sleep(1);
            wait++;
            continue;
        } else {
            std::cout << "reimaging return status: " << value << " within " << wait << "s" << std::endl;
            return 0;
        }
    } while (wait < REIMAGE_TIMEOUT);

    return -ETIMEDOUT;
}

int AzureDev::azureHotReset()
{
    std::string fpgaSerialNumber;
    get_fpga_serialNo(fpgaSerialNumber);
    std::cout << "HotReset FPGA serial No: " << fpgaSerialNumber << std::endl;
    //start the reset process
    std::string delim = ":";
    std::string ret, key, value;
    ret = REST_Get(
        RESTIP_ENDPOINT,
        "machine/plugins/?comp=FpgaController&type=Reset",
        fpgaSerialNumber
    );
    syslog(LOG_INFO, "obtained ret = %s from reset call", ret.c_str());
    if (splitLine(ret, key, value, delim) != 0 ||
        key.compare("Reset") != 0 ||
        value.compare("0") != 0) {
        syslog(LOG_INFO, "wasn't expected response...%s", ret.c_str());
        return -EFAULT;
    }
 
    // poll wireserver for response TBD
    //check the response
    syslog(LOG_INFO, "poll for reset status...");
    int wait = 0;
    do {
        ret = REST_Get(
            RESTIP_ENDPOINT,
            "machine/plugins/?comp=FpgaController&type=GetResetStatus",
            fpgaSerialNumber
        );
        syslog(LOG_INFO, "obtained ret = %s from get reset status call", ret.c_str());
        if (splitLine(ret, key, value, delim) != 0 ||
            key.compare("GetResetStatus") != 0)
            return -EFAULT;

        if (value.compare("2") != 0) {
            sleep(1);
            wait++;
            continue;
        } else {
            std::cout << "getreset status return status: " << value << " within " << wait << "s" << std::endl;
            return 0;
        }
    } while (wait < REIMAGE_TIMEOUT);
    syslog(LOG_INFO, "complete get reset status");
    return 0;
}

AzureDev::~AzureDev()
{
}

AzureDev::AzureDev(size_t index) : index(index)
{
    dev = pcidev::get_dev(index, true);
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
    int retryCounter = 0;
    long responseCode = 0;

    unit.uptr = data.c_str();
    unit.sizeleft = data.size();

    curl = curl_easy_init();

    if(curl)
    {
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

        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        do
        {
            responseCode = 0;
            res = curl_easy_perform(curl);
            
            if (res != CURLE_OK) {
            	std::cerr << "curl_easy_perform() failed: " <<  curl_easy_strerror(res) << std::endl;
            	retryCounter++;
            	if (retryCounter <= UPLOAD_RETRY)
            	{
            		std::cout << "Retrying an upload..." << retryCounter << std::endl;
            		sleep(1);
            	} else
            	{
            		std::cerr << "Max number of retries reached... givin up" << std::endl;
            		curl_easy_cleanup(curl);
            		return 1;
            	}
            } else {
            	// check the return code				
            	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
            	std::cout << "Debug: status code " << responseCode << std::endl;
            	if (responseCode >= 400) {
            		// error range
            		res = CURLE_HTTP_RETURNED_ERROR;
            		retryCounter++;
            		if (retryCounter <= UPLOAD_RETRY)
            		{
            			std::cout << "Retrying an upload after http error..." << retryCounter << std::endl;
            			sleep(1);
            		} else
            		{
            			std::cerr << "Max number of retries reached... givin up" << std::endl;
            			curl_easy_cleanup(curl);
            			return 1;
            		}
            	}
            }
        } while (res != CURLE_OK);

        // cleanup
        curl_easy_cleanup(curl);
        std::cout << "Upload segment " << index + 1 << " of " << total  << std::endl;
    }

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

    curl = curl_easy_init();
    if(curl)
    {
        std::stringstream urlStream;
        urlStream << "http://" << ip << "/" << endpoint << "&chipid=" << target;

        curl_easy_setopt(curl, CURLOPT_URL, urlStream.str().c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readbuff);

        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        res = curl_easy_perform(curl);

        if(res != CURLE_OK)
        {
            std::cout <<  "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        std::cout << "String returned: " << readbuff << std::endl;
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
    if(!SHA256_Init(&context))
    {
        std::cerr << "Unable to initiate SHA256" << std::endl;
        return 1;
    }

    unsigned pos = 0;

    while (pos < input.size())
    {
        std::string segment = input.substr(pos, TRANSFER_SEGMENT_SIZE);

        if(!SHA256_Update(&context, segment.c_str(), segment.size()))
        {
            std::cerr << "Unable to Update SHA256 buffer" << std::endl;
            return 1;
        }
        output.push_back(segment);
        pos += TRANSFER_SEGMENT_SIZE;
    }

    // Get Final SHA
    unsigned char result[SHA256_DIGEST_LENGTH];
    if(!SHA256_Final(result, &context))
    {
        std::cerr << "Error finalizing SHA256 calculation" << std::endl;
        return 1;
    }
    
    // Convert the byte array into a string
    std::stringstream shastr;
    shastr << std::hex << std::setfill('0');
    for (auto &byte: result)
    {
        shastr << std::setw(2) << (int)byte;
    }

    sha = shastr.str();
    return 0;
}

void AzureDev::get_fpga_serialNo(std::string &fpgaSerialNo)
{
    std::string errmsg;
    dev->sysfs_get("xmc", "serial_num", errmsg, fpgaSerialNo);
    //fpgaSerialNo = "1281002AT024";
    if (fpgaSerialNo.empty())
        fpgaSerialNo = FPGA_SERIAL_NUMBER.at(index);
}
