
/**
 * Copyright (C) 2016-2019 Xilinx, Inc
 * Author(s): Ch Vamshi Krishna
 *          : Hemant Kashyap
 * ZNYQ HAL sw_emu Driver layered on top of ZYNQ hardware driver
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

#ifndef PLLAUNCHER_DEFINES_H
#define PLLAUNCHER_DEFINES_H
#include <cstdlib>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <iostream>
/* All functions are identified though the ID and the function arguments make up the payload.
 *
 */

#define PL_RP_MP_ALLOCATED_ID 15;
#define PL_RP_MP_ALLOCATED_ADD 0xff4e0000;

#define PL_RP_ALLOCATED_ID  12;
#define PL_RP_ALLOCATED_ADD 0xFE000000;


#define PL_OCL_PACKET_SEPRATION_MARKER ':';
#define PL_OCL_PACKET_END_MARKER       '@';

namespace PLLAUNCHER {
enum OCL_APINAME_TYPE {
	PL_OCL_LOADXCLBIN_ID = 0,
	PL_OCL_XCLCLOSE_ID=1,
	PL_OCL_XRESET_ID=2,
	PL_OCL_UNKNOWN_ID=3
};


enum OCL_ARG_TYPE {
	OCL_STRING=0,
	OCL_INTEGER=1
} ;

class OclArg {
private:
	uint8_t        miType;
	const char *   mpData;
	uint32_t       miIntData;
public:
	OclArg(const char* _psData){
		miType=OCL_STRING;
		mpData=_psData;
		miIntData=0;
	}
	OclArg(uint32_t _iData){
			miType=OCL_INTEGER;
			mpData=NULL;
			miIntData=_iData;
	}
	~OclArg() {
		if(mpData) free((void *)mpData);
	}
	/**
	 * Get the type of argument
	 * @return
	 */
	uint8_t getType(){
		return miType;
	}
	uint8_t getIntData() {
		return miIntData;
	}
	const char* getStringData(){
		return mpData;
	}
} ;

class OclCommand {
private:
	uint32_t           miCommand;
	uint8_t          * mpBuf;
public:
	std::vector<OclArg*> mArgs;
	OclCommand():miCommand(0),mpBuf(NULL) {

	}
	~OclCommand () {
		if(mpBuf) {
		    free(mpBuf);
		}
		std::vector<OclArg*>::iterator it;
		for(it=mArgs.begin() ; it < mArgs.end(); it++){
			if(*it) delete(*it);
		}
		mArgs.clear();
	}
	/**
	 * Get Command
	 * @return command
	 */
	uint8_t getComamnd() {
		return miCommand;
	}
	/**
	 * Set Command
	 * @param _iCommand Command Value
	 */
	void setCommand(uint8_t _iCommand){
		miCommand=_iCommand;
	}
	/**
	 * Parse the Buffer stream received from socket
	 * @param _psBuffer Buffer Pointer
	 */
	void parseBuffer(const char* _psBuffer) {
		std::stringstream ss;
		ss.str(_psBuffer);
		std::string item,item2;
		bool commandParsed=false;
		char packet_sep=PL_OCL_PACKET_SEPRATION_MARKER;
		while (std::getline(ss, item,packet_sep)) {
			//Check if this is the first
			if(!commandParsed) {
				commandParsed=true;
				std::stringstream itemss(item);
							uint32_t tmpComm ;
							itemss>>tmpComm;
				miCommand=tmpComm;
				continue;
			}
			if(!std::getline(ss, item2,packet_sep)) {
				return;
			}
			std::stringstream itemss(item);
			uint32_t tmpType ;
			itemss>>tmpType;
			if(tmpType==OCL_STRING) {
				char *tmpStr=(char*)malloc(strlen(item2.c_str())+1);
				memcpy(tmpStr,item2.c_str(),strlen(item2.c_str()));
				tmpStr[strlen(item2.c_str())]='\0';
				mArgs.push_back(new OclArg(tmpStr));
			} else {
				std::stringstream tmpSS(item2);
				uint32_t val;
				tmpSS>>val;
				mArgs.push_back(new OclArg(val));
			}
		}
	}
/**
 * Generates the Buffer stream which can be transported on the socket
 * @param _piLength Pointer to buffer stream length
 * @return          Buffer stream pointer
 */
	uint8_t * generateBuffer(uint32_t* _piLength) {
		std::stringstream tmpSS;
		std::vector<OclArg*>::iterator it;
		tmpSS<<miCommand;
		char packet_sep=PL_OCL_PACKET_SEPRATION_MARKER;
		for(it=mArgs.begin() ; it < mArgs.end(); it++){
			OclArg* tmp=*it;
			if(tmp->getType()==OCL_STRING) {
				uint32_t typ=0;
				tmpSS<<packet_sep<<typ<<packet_sep<<tmp->getStringData();
			} else {
				uint32_t typ=1;
				tmpSS<<packet_sep<<typ<<packet_sep<<tmp->getIntData();
			}
		}
		tmpSS<<packet_sep;
		if(mpBuf) free(mpBuf);
		mpBuf=NULL;
		uint32_t pBbufSize=(int)tmpSS.str().size();
		pBbufSize=((pBbufSize%4)==0)? pBbufSize: pBbufSize+ (4-(pBbufSize%4));
		mpBuf=(unsigned char*) malloc(pBbufSize);
		*_piLength=pBbufSize;
		memcpy((void*)mpBuf,(void*)tmpSS.str().c_str(),(int)tmpSS.str().size());
		return mpBuf;
	}
	/**
	 * Add String Argument
	 * @param _psArg String Argument
	 */
	void addArg(const char* _psArg) {
		char *tmpArg=(char*)malloc(strlen(_psArg)+1);
		memcpy(tmpArg,_psArg,strlen(_psArg));
		tmpArg[strlen(_psArg)]='\0';
		mArgs.push_back(new OclArg((char*)tmpArg));
	}
	/**
	 * Add Integer Argument
	 * @param _iVal Integer Value
	 */
	void addArg(uint32_t _iVal) {
		mArgs.push_back(new OclArg(_iVal));
	}
};

}

#endif


