#include <iostream>
#include <string>
#include <getopt.h>
#include <thread>
// host_src includes
#include "xclhal2.h"
#include "xclbin.h"

void foo(int num, xclDeviceHandle handle){
    char msg[50];
    sprintf(msg, "(5) Running Thread number %d", num);
    xclLogMsg(handle, NOTICE, "XRT",(char*) msg);
}

int main(int argc, char** argv) {
    xclDeviceInfo2 deviceInfo;
    unsigned devIndex = 0;
    char *bitFile = NULL;
    xclDeviceHandle handle;
    bool debug_flag = false;
    int c;

    while ((c = getopt(argc, argv, "k:d:v")) != -1)
	{
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
    if(!handle){
            char msg[20];
            sprintf(msg, "(0) Unable to open device %d", devIndex);
            xclLogMsg(handle, EMERGENCY, "XRT",(char*)msg);
    }

    char msg[150];
    sprintf(msg, "(6) %s was passed in as an argument", bitFile);
    xclLogMsg(handle, INFO, "XRT",(char*)msg);

    if(debug_flag)
        xclLogMsg(handle, DEBUG, "XRT","(7) Debug flag was set");

    if(xclGetDeviceInfo2(handle, &deviceInfo))
        xclLogMsg(handle, ALERT,"XRT", "(1) Unable to obtain device information");

    const xclBin *blob =(const xclBin *) new char [1];
    if(xclLoadXclBin(handle, blob))
        xclLogMsg(handle, CRITICAL, "XRT", "(2) Unable to load xclbin");

    std::cout << "~~~Multi threading~~~" <<  std::endl;
    std:: thread t1(foo, 1, handle);
    std::thread t2(foo, 2, handle);
    t1.join();
    t2.join();
    std::cout << "~~~~~~~~~~~~~~~~~~~~~" << std:: endl;

    std::cout << "Other messages: \n";
    xclLogMsg(handle, ERROR,"XRT", "(3) Display when verbosity 3");
    xclLogMsg(handle, WARNING, "XRT", "(4) Display when verbosity 2");

    return 0;
}
