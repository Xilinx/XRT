#include <iostream>
#include <string>
#include <getopt.h>
#include <thread>
// host_src includes
#include "xrt.h"
#include "xclbin.h"

void foo(int num, xclDeviceHandle handle)
{
    xrt_logmsg(XRT_NOTICE, "(5) Running Thread number %d", num);
}

int main(int argc, char** argv) {
    xclDeviceInfo2 deviceInfo;
    unsigned devIndex = 0;
    char *bitFile = NULL;
    xclDeviceHandle handle;
    bool debug_flag = false;
    int c;

    while ((c = getopt(argc, argv, "k:d:v")) != -1) {
        switch (c) {
        case 'k':
            bitFile = optarg;
            break;
        case 'd':
            devIndex = std::atoi(optarg);
            break;
        case 'v':
            debug_flag = true;
            break;
        default:
            exit(EXIT_FAILURE);
        }
    }

    handle = xclOpen(devIndex, "", XCL_INFO);
    if(!handle)
        xrt_logmsg(XRT_EMERGENCY, "(0) Unable to open device %d", devIndex);

    xrt_logmsg(XRT_INFO, "(6) %s was passed in as an argument", bitFile);

    if(debug_flag)
        xrt_logmsg(XRT_DEBUG, "(7) Debug flag was set");

    if(xclGetDeviceInfo2(handle, &deviceInfo))
        xrt_logmsg(XRT_ALERT,"(1) Unable to obtain device information");

    const xclBin *blob =(const xclBin *) new char [1];
    if(xclLoadXclBin(handle, blob))
        xrt_logmsg(XRT_CRITICAL, "(2) Unable to load xclbin");

    std::cout << "~~~Multi threading~~~" <<  std::endl;
    std:: thread t1(foo, 1, handle);
    std::thread t2(foo, 2, handle);
    t1.join();
    t2.join();
    std::cout << "~~~~~~~~~~~~~~~~~~~~~" << std:: endl;

    std::cout << "Other messages: \n";
    xrt_logmsg(XRT_ERROR,"(3) Display when verbosity 3");
    xrt_logmsg(XRT_WARNING, "(4) Display when verbosity 2");

    return 0;
}
