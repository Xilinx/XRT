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

// ============================================================================
// COPYRIGHT NOTICE
// (c) Copyright 2017 Xilinx, Inc. All rights reserved.
//
// File Name: xclbincat0.cxx
// ============================================================================

//#include "xclbin.h"
//#include "xclbincat0.h"
//#include <stdlib.h>
//#include <stdio.h>
//#include <time.h>
//#include <string.h>
//
//
//namespace xclbincat0 {
//
////todo : uint64_t etc
//
////binary format
////char [6]    xclbin
////uint32_t    bitstreamlength
////uint32_t    sharedlibrarylength
////uint32_t    xmlfilelength
////....bitstream
////....shared library
////....XML File
//
//
//////////////////////////////////////////////////////////////////////////////////
//static int load_file_to_memory(const char *filename, char **result)
//{
//	int size = 0;
//	FILE *f = fopen(filename, "rb");
//	if (f == NULL)
//	{
//		*result = NULL;
//                printf("error : cannot open %s\n", filename);
//		return -1; // -1 means file opening fail
//	}
//	fseek(f, 0, SEEK_END);
//	size = ftell(f);
//	fseek(f, 0, SEEK_SET);
//	*result = (char *)malloc(size+1);
//	if (size != fread(*result, sizeof(char), size, f))
//	{
//		free(*result);
//    printf("error : cannot read %s\n", filename);
//		fclose(f);
//		return -2; // -2 means file reading fail
//	}
//	fclose(f);
//	(*result)[size] = 0;
//	return size;
//}
//
//////////////////////////////////////////////////////////////////////////////////
//
//
//int execute( int argc, char** argv )
//{
//
//  // Go through and find any additional files that need to be stitched in
//  //  to support hardware emulation and hardware debug.  These
//  //  should always be at the end, so the rest of the code should be 
//  //  unaffected.
//
//  int numDebugArgs = 0 ;
//  int dwarfFileNum = -1 ;
//  int ipiMappingFileNum = -1 ;
//  for (unsigned int i = 1 ; i < argc ; ++i)
//  {
//    if (strcmp(argv[i], "-dwarfFile") == 0)
//    {
//      numDebugArgs += 2 ;
//      dwarfFileNum = i + 1 ;
//    }
//    if (strcmp(argv[i], "-ipiMappingFile") == 0)
//    {
//      numDebugArgs +=2 ;
//      ipiMappingFileNum = i + 1 ;
//    }
//  }
//  argc -= numDebugArgs ;
//
//  if(!((argc==4 && !strcmp(argv[1],"-clearstream")) ||
//       (argc==5 && !strcmp(argv[1],"-bitstream")) ||
//       (argc==5 && !strcmp(argv[1],"-nobitstream")) ||
//       (argc==6 && !strcmp(argv[1],"-bitstream") && !strcmp(argv[3],"-clearstream")) ||
//       (argc==7 && !strcmp(argv[1],"-bitstream") && !strcmp(argv[3],"-clearstream")))) {
//      printf("Problems encountered while parsing xclbincat command line. See example usage below\n");
//      printf("xclbincat -bitstream <bitstreambinaryfile.bin> [-clearstream <clearstreambinaryfile.bin>] <map.xml> <outputfile.xclbin>\n");
//      printf("xclbincat -nobitstream <sharedlib.so> <map.xml> <outputfile.xclbin>\n");
//      printf("xclbincat -clearstream <clearstreambinaryfile.bin> <outputfile.dsabin>\n");
//      printf("xclbincat -bitstream <bitstreambinaryfile.bin> -clearstream <clearstreambinaryfile.bin> <outputfile.dsabin>\n");
//      exit(0);
//  }
//
//  enum runmode_t{
//    Clearstreammode,
//    Bitstreammode,
//    Nobitstreammode} runmode;
//
//  if(argc==4 && !strcmp(argv[1],"-clearstream")){
//    //printf("clearstream mode\n");
//    runmode=Clearstreammode;
//  }
//
//  if((argc==5||argc==6||argc==7) && !strcmp(argv[1],"-bitstream")){
//    //printf("bitstream mode\n");
//    runmode=Bitstreammode;
//  }
//
//  if(argc==5 && !strcmp(argv[1],"-nobitstream")){
//    //printf("no bitstream mode\n");
//    runmode=Nobitstreammode;
//  }
//
//
//  char *bitstreambin = 0;
//  int bitstreamlength = 0;
//  char *clearstreambin = 0;
//  int clearstreamlength = 0;
//  int xmlIndex = 3;
//
//  if (runmode==Bitstreammode) {
//      bitstreamlength=load_file_to_memory(argv[2],&bitstreambin);
//      if (bitstreamlength < 0) {
//          exit(1);
//      }
//      if (argc == 6 || argc == 7) {
//          clearstreamlength=load_file_to_memory(argv[4],&clearstreambin);
//          if(clearstreamlength < 0) {
//              exit(1);
//          }
//          if (argc == 6) {
//            xmlIndex = -1;
//          }
//          else {
//            xmlIndex += 2;
//          }
//      }
//  }
//  else if (runmode == Clearstreammode) {
//      clearstreamlength=load_file_to_memory(argv[2],&clearstreambin);
//      if(clearstreamlength < 0) {
//          exit(1);
//      }
//      xmlIndex = -1;
//  }
//
//  if(runmode!=Bitstreammode){
//    bitstreamlength=0;
//  }
//
//
//  char *sharedlib = 0;
//  int sharedliblength = 0;
//
//  if(runmode==Nobitstreammode){
//      sharedliblength=load_file_to_memory(argv[2],&sharedlib);
//  }
//
//  if(sharedliblength < 0) {
//      exit(1);
//  }
//  
//  char* dwarfOffset = 0 ;
//  int dwarfLength = 0 ;
//  
//  if (dwarfFileNum > 0)
//  {
//    dwarfLength = load_file_to_memory(argv[dwarfFileNum], &dwarfOffset) ;
//    if (dwarfLength < 0)
//    {
//      exit(1) ;
//    }
//  }
//
//  char* ipiMappingOffset = 0 ;
//  int ipiMappingLength = 0 ;
//
//  if (ipiMappingFileNum > 0)
//  {
//    ipiMappingLength = load_file_to_memory(argv[ipiMappingFileNum], &ipiMappingOffset) ;
//    if (ipiMappingLength < 0)
//    {
//      exit(1) ;
//    }
//  }
//
//  char *xmlfile = 0;
//  int xmlfilelength = 0;
//
//  if (xmlIndex != -1) {
//      xmlfilelength = load_file_to_memory(argv[xmlIndex],&xmlfile);
//  }
//
//  if (xmlfilelength < 0) {
//      exit(1);
//  }
//
//  FILE *out=fopen(argv[argc-1],"wb");
//
//  struct xclBin header;
//  memset(&header, 0, sizeof(struct xclBin));
//
//  
//  if( dwarfFileNum > 0)
//  {
//    strcpy(header.m_magic, "xclbin1");
//  }
//  else
//  {
//    strcpy(header.m_magic, "xclbin0");
//  }
//  
//  header.m_timeStamp = (uint64_t)time(NULL);
//  header.m_length = sizeof(struct xclBin);
//
//  unsigned xmlfilePadding = 0;
//  if (xmlfilelength) {
//      header.m_metadataOffset = header.m_length;
//      header.m_metadataLength = xmlfilelength;
//      xmlfilePadding = (xmlfilelength & 0x7) ? 0x8 - (xmlfilelength & 0x7) : 0;
//      header.m_length += xmlfilelength;
//      header.m_length += xmlfilePadding;
//  }
//
//  unsigned bitstreamPadding = 0;
//  if (bitstreamlength) {
//      header.m_primaryFirmwareOffset = header.m_length;
//      header.m_primaryFirmwareLength = bitstreamlength;
//      bitstreamPadding = (bitstreamlength & 0x7) ? 0x8 - (bitstreamlength & 0x7) : 0;
//      header.m_length += bitstreamlength;
//      header.m_length += bitstreamPadding;
//  }
//
//  unsigned clearstreamPadding = 0;
//  if (clearstreamlength) {
//      header.m_secondaryFirmwareOffset = header.m_length;
//      header.m_secondaryFirmwareLength = clearstreamlength;
//      clearstreamPadding = (clearstreamlength & 0x7) ? 0x8 - (clearstreamlength & 0x7) : 0;
//      header.m_length += clearstreamlength;
//      header.m_length += clearstreamPadding;
//  }
//
//  unsigned sharedlibPadding = 0;
//  if (sharedliblength) {
//      header.m_driverOffset = header.m_length;
//      header.m_driverLength = sharedliblength;
//      sharedlibPadding = (sharedliblength & 0x7) ? 0x8 - (sharedliblength & 0x7) : 0;
//      header.m_length += sharedliblength;
//      header.m_length += sharedlibPadding;
//  }
//
//  
//  unsigned int dwarfPadding = 0 ;
//  if (dwarfLength) {
//    header.m_dwarfOffset = header.m_length ;
//    header.m_dwarfLength = dwarfLength ;
//    dwarfPadding = (dwarfLength & 0x7) ? 0x8 - (dwarfLength & 0x7) : 0 ;
//    header.m_length += dwarfLength ;
//    header.m_length += dwarfPadding ;
//  }
//  
//  
//  unsigned int ipiMappingPadding = 0 ;
//  if (ipiMappingLength) {
//    header.m_ipiMappingOffset = header.m_length ;
//    header.m_ipiMappingLength = ipiMappingLength ;
//    ipiMappingPadding = (ipiMappingLength & 0x7) ? 0x8 - (ipiMappingLength & 0x7) : 0 ;
//    header.m_length += ipiMappingLength ;
//    header.m_length += ipiMappingPadding ;
//  }
//
//
//  char padding[8];
//  memset(padding, 0, 8);
//
//  fwrite(&header, sizeof(header), 1, out);
//  fwrite(xmlfile, 1, xmlfilelength, out);
//  fwrite(padding, 1, xmlfilePadding, out);
//  fwrite(bitstreambin, 1, bitstreamlength, out);
//  fwrite(padding, 1, bitstreamPadding, out);
//  fwrite(clearstreambin, 1, clearstreamlength, out);
//  fwrite(padding, 1, clearstreamPadding, out);
//  fwrite(sharedlib, 1, sharedliblength, out);
//  fwrite(padding, 1, sharedlibPadding, out);
//  fwrite(dwarfOffset, 1, dwarfLength, out) ;
//  fwrite(padding, 1, dwarfPadding, out) ;
//  fwrite(ipiMappingOffset, 1, ipiMappingLength, out) ;
//  fwrite(padding, 1, ipiMappingPadding, out) ;
//
//  fclose(out);
//
//  if(bitstreambin) free(bitstreambin);
//  if(clearstreambin) free(clearstreambin);
//  if(sharedlib) free(sharedlib);
//  if(xmlfile) free(xmlfile);
//  if(dwarfOffset) free(dwarfOffset);
//  if(ipiMappingOffset) free(ipiMappingOffset);
//
//  return 0;
//}
//
//} // namespace xclbincat0


