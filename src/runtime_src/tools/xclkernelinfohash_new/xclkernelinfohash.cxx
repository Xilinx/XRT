/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

//xclkernelinfohash source0.cl kernelinfo.xml
//computes kernel hash and modified kernelinfo.xml

#include <stdexcept>
#include <stdlib.h>
#include <stdio.h>

#ifndef _WINDOWS
// TODO: Windows build support
//   crypt.h is a linux only header file
//   it is included for crypt
#include <crypt.h>
#endif

#include <iostream>
// #include <unistd.h>

// New LMX support
#include "HPIKernelInfoReaderWriterLMX.h"
#include "HCOKernelXMLReaderWriterLMX.h"
#include "lmx6.0/lmxparse.h"

//TODO: to remove
#include "ComMsgMgrInstance.h"
#include "HXMLException.h"


// In case we switch between wstring and string on the lmx
// If lmx is configured to use (narrow) string, no conversion is needed
// If lmx is configured to use wstring, use HI18N convert
#ifdef LMX_NO_WSTRING
#define _convert_(x) x
#else
#include "HI18N.h"   // Important, if you include this, make sure you add rdi_common to the library dependency in rdi.mk
#define _convert_(x) HI18N::convert(x)
#endif


//todo : uint64_t etc

//binary format
//char [6]    xclbin
//uint32_t    bitstreamlength
//uint32_t    sharedlibrarylength
//uint32_t    xmlfilelength
//....bitstream
//....shared library  
//....XML File

#ifndef VERBOSE
# define VERBOSE
#endif

////////////////////////////////////////////////////////////////////////////////

size_t load_file_to_memory(const char *filename, char **result)
{ 
  size_t size = 0;
  FILE *f = fopen(filename, "rb");
  if (f == NULL) 
  { 
    *result = NULL;
    return -1; // -1 means file opening fail 
  } 
  fseek(f, 0, SEEK_END);
  size = ftell(f);
  fseek(f, 0, SEEK_SET);
  *result = (char *)malloc(size+1);
  if (size != fread(*result, sizeof(char), size, f)) 
  { 
    free(*result);
    return -2; // -2 means file reading fail 
  } 
  fclose(f);
  (*result)[size] = 0;
  return size;
}

void generateHashString(char* firstkernelfilename,
                        std::string& sHash)
{
  char *firstkernelfiledata;
  int firstkernelfilelength;

  // load_file_to_memory uses malloc to allocate the memory for firstkernelfiledata
  firstkernelfilelength=load_file_to_memory(firstkernelfilename,&firstkernelfiledata);
  if(firstkernelfilelength==-1){
    fprintf(stderr,"error : cannot open %s\n",firstkernelfilename);
    exit(1);
  }

  FILE *global_verboselog = stdout;

  std::string newkernelstring(firstkernelfiledata);
  std::string kernelstring(firstkernelfiledata);

  bool xcl_conformancemode;
  xcl_conformancemode=(getenv("XCL_CONFORMANCE")!=NULL);
  if(xcl_conformancemode){
    //strip directory from file name path
    std::string kernelnopath;
    std::string path(firstkernelfilename);
    size_t searchindex=(path.size()-1);
    while(searchindex !=0 && path.at(searchindex)!='/') searchindex--;
    if(path.at(searchindex)=='/'){
      kernelnopath=path.substr(searchindex+1,path.size()-(searchindex+1));
    }else{
      kernelnopath=path;
    }

    //demangle kernel name prior to hashing
    //kernelfilename = foo_0.cl
    //output = foo
    std::string mangledkernelname;
    mangledkernelname=kernelnopath.substr(0,kernelnopath.size()-3);
    //remove mangle
    std::string demangledkernelname=mangledkernelname;
    while(demangledkernelname.back() != '_'){
      demangledkernelname.pop_back();
    }
    demangledkernelname.pop_back();
#ifdef VERBOSE
    fprintf(global_verboselog,"[xclkernelinfohash] mangled kernel name %s demangled kernel name %s\n",mangledkernelname.c_str(),demangledkernelname.c_str());
#endif

    size_t pos = kernelstring.find(mangledkernelname);
    if(pos==std::string::npos){
#ifdef VERBOSE
      fprintf(global_verboselog,"[xclkernelinfohash] error : string replace failed\n");
      exit(1);
#endif
    }else{
      newkernelstring = kernelstring.substr(0,pos) + demangledkernelname + kernelstring.substr(pos+mangledkernelname.length(),kernelstring.length()-((pos+mangledkernelname.length())));
    }
  }

#ifdef VERBOSE
  //fprintf(global_verboselog,"[xclkernelinfohash] hashing \n|||||%s|||||\n",newkernelstring.c_str());
  //fprintf(global_verboselog,"was\n||||%s||||\n",newkernelstring.c_str());
  //fprintf(global_verboselog,"subs\n||||%s||||\n",newkernelstring.substr(0,newkernelstring.length()).c_str());

#endif

  //generate hash string
  char salt[] = "$1$salt$encrypted";

#ifndef _WINDOWS
// TODO: Windows build support
//   crypt is defined in crypt.h
  char* hash = crypt(newkernelstring.c_str(),salt);
#else 
  char* hash = nullptr;
#endif

  hash +=8;
  fprintf(stdout,"%s\n",hash);
  sHash = hash;

  free(firstkernelfiledata);
}


void updateKernelInfoFile(/*const char* kernelinfofilename,*/
                          const std::string& kernelinfofile,
                          const std::string& sHash  )
{
  LMX60_NS::elmx_error l_error;

  try {
    KernelInfo::Project project( kernelinfofile.c_str(), &l_error );

    // Update the kernel info xml with hash value
    for ( int i = 0; i < project.SizeCore(); ++i ) {
      KernelInfo::Core & core = project.GetCore(i);

      // There should be only one kernel in each kernel_info.xml
      for ( int j = 0; j < core.SizeKernel(); ++j ) {
        KernelInfo::Kernel & kernel = core.GetKernel(j);
        kernel.SetHash( _convert_( sHash ) );
      }
    }

    // Write out the file
    // Set up the semi-compress output format
    LMX60_NS::c_xml_writer::set_default_nl( "\n" );
    LMX60_NS::c_xml_writer::set_default_tab( "  " );
    LMX60_NS::c_xml_writer::set_default_attribute_nl( " " );
    LMX60_NS::c_xml_writer::set_default_attribute_tab( "" );

    // Write out the kernel_info.xml
    if ( project.Marshal( kernelinfofile.c_str() ) != LMX60_NS::ELMX_OK ) {
      // TODO
      ComMsgMgrInstance msg = ComMsgMgrInstance::CreateMsg( ComMsgMgr::MSGTYPE_ERROR, "@60-131@%s", kernelinfofile.c_str());
      throw HXMLException( msg );
      // throw (std::runtime_error("Failed to write xml file " + kernelinfofile));
    }

  }
  catch ( const LMX60_NS::c_lmx_exception& err ) {
    std::string errMsg(err.what());
    int line = 0;
    if (const LMX60_NS::c_lmx_reader_exception* rerr = dynamic_cast<const LMX60_NS::c_lmx_reader_exception*>(&err))
      line = rerr->get_line();

    // TODO
    ComMsgMgrInstance msg = ComMsgMgrInstance::CreateMsg(ComMsgMgr::MSGTYPE_ERROR, "@60-185@%s%d%s", kernelinfofile.c_str(), line, errMsg.c_str());
    throw HXMLException( msg );
    // throw (std::runtime_error("Failed to parse file " + kernelinfofile + "\n line number " + line));
  }

}

void updateKernelXmlFile(const std::string& kernelxmlfile,
                         const std::string& sHash)
{
  LMX60_NS::elmx_error l_error;

  try {
    KernelXML::Root root( kernelxmlfile.c_str(), &l_error );

    // Update the kernel xml with hash value
    KernelXML::Kernel & kernel = root.GetKernel();    
    kernel.SetHash( _convert_( sHash ) );

    // Write out the file
    // Set up the semi-compress output format
    LMX60_NS::c_xml_writer::set_default_nl( "\n" );
    LMX60_NS::c_xml_writer::set_default_tab( "  " );
    LMX60_NS::c_xml_writer::set_default_attribute_nl( " " );
    LMX60_NS::c_xml_writer::set_default_attribute_tab( "" );

    // Write out the kernel.xml
    if ( root.Marshal( kernelxmlfile.c_str() ) != LMX60_NS::ELMX_OK ) {
      // TODO
      ComMsgMgrInstance msg = ComMsgMgrInstance::CreateMsg( ComMsgMgr::MSGTYPE_ERROR, "@60-131@%s", kernelxmlfile.c_str());
      throw HXMLException( msg );
      // throw (std::runtime_error("Failed to write xml file " + kernelxmlfile));
    }

  }
  catch ( const LMX60_NS::c_lmx_exception& err ) {
    std::string errMsg(err.what());
    int line = 0;
    if (const LMX60_NS::c_lmx_reader_exception* rerr = dynamic_cast<const LMX60_NS::c_lmx_reader_exception*>(&err))
      line = rerr->get_line();

    // TODO
    ComMsgMgrInstance msg = ComMsgMgrInstance::CreateMsg(ComMsgMgr::MSGTYPE_ERROR, "@60-185@%s%d%s", kernelxmlfile.c_str(), line, errMsg.c_str());
    throw HXMLException( msg );
    // throw (std::runtime_error("Failed to parse file " + kernelxmlfile + "\n line number " + line));
  }

}

////////////////////////////////////////////////////////////////////////////////


int main(int argc, char **argv){

  if(argc!=3){
    fprintf(stderr,"xclkernelinfohash source0.cl [source1.cl ....] kernel.xml\n");
    exit(0);
  }

  char *firstkernelfilename = argv[1];  // input
  char *kernelxmlfilename = argv[argc-1];  // output

  std::string sHash;
  generateHashString(firstkernelfilename, sHash);
  updateKernelXmlFile(std::string(kernelxmlfilename), sHash);

  exit(EXIT_SUCCESS);
}


