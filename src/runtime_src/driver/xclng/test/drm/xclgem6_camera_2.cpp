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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>
#include <numeric>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <cassert>
#include <random>

#include "drm/drm.h"
#include "xocl_ioctl.h"
#include "util.h"

#include <linux/videodev2.h>
#include <poll.h>
#include <fstream>




/**
 * Sanity test for DMA-BUF export/import.
 * Creates BO object in Camera device; Export buffers from camera to get FD.
 * Xilinx FPGA import's these buffers and will read from these buffers.
 * We copy the received frame to a file. Image file should be visible in image softwares.
 * Performs simple alloc, read/write ,sync and free operations.
 * Compile command:
 * g++ -g -std=c++11 -I ../../include -I ../../drm/xocl xclgem6_camera_2.cpp util.cpp
 */

#define BUFFER_NUM_DEFAULT 5
#define VIDEO_NODE_DEFAULT "/dev/video0"
#define WIDTH_DEFAULT 640
#define HEIGHT_DEFAULT 480
#define IMAGESIZE 614400 //640 x 480 x 2; 2 bytes per pixel

int camera_fd;
uint64_t image_size;
unsigned int pitch;
char buffer0[IMAGESIZE];

//int *import_buf_fd = NULL;
int xilinx_bo_fd[5];

//cl_mem *import_buf = NULL;

int frame_count = 0;
struct v4l2_options{
  const char *dev_name;
  unsigned int width, height;
  unsigned int spec_res;
  unsigned int buffer_num;
  unsigned int do_list;
} vo;
struct v4l2_format format;

#define CHECK_VASTATUS(va_status,func)                                  \
  if (va_status != VA_STATUS_SUCCESS) {                                   \
    fprintf(stderr, "status = %d, %s: %s(line %d) failed, exit\n",va_status, __func__, func, __LINE__); \
    exit(1);                                                            \
  }

#define CHECK_CLSTATUS(status,func)                                  \
  if (status != CL_SUCCESS) {                                   \
    fprintf(stderr, "status = %d, %s: %s(line %d) failed, exit\n", status, __func__, func, __LINE__); \
    exit(1);                                                            \
  }

#define CHECK_V4L2ERROR(ret, STR)                               \
  if (ret){                             \
    fprintf(stderr, STR);            \
    perror(" ");                            \
    fprintf(stderr, "ret = %d, %s: %s(line %d) failed, exit\n", ret, __func__, STR, __LINE__);      \
    exit(1);                                  \
  }



static void init_camera(void){

  int ret;
  struct v4l2_capability cap;

  vo.dev_name = VIDEO_NODE_DEFAULT;
  vo.width = WIDTH_DEFAULT;
  vo.height = HEIGHT_DEFAULT;
  vo.spec_res = 0;
  vo.buffer_num = BUFFER_NUM_DEFAULT;
  vo.do_list = 0;

  camera_fd = open(vo.dev_name, O_RDWR | O_NONBLOCK, 0);
  if (camera_fd < 0) {
    fprintf(stderr, "Can not open %s: %s\n",
        vo.dev_name, strerror(errno));
    exit(1);
  }

  memset(&cap, 0, sizeof(cap));
  ret = ioctl(camera_fd, VIDIOC_QUERYCAP, &cap);
  CHECK_V4L2ERROR(ret, "VIDIOC_QUERYCAP");
  if(!(cap.capabilities & V4L2_CAP_STREAMING)){
    fprintf(stderr, "The device does not support streaming i/o\n");
    exit(1);
  }
  //printf("capabilities = %08x\n", cap.capabilities);

  int requestedPixelFormat = V4L2_PIX_FMT_MJPEG;
  //int reuestedPixelFormat = V4L2_PIX_FMT_YUYV;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.width = vo.width;
  format.fmt.pix.height = vo.height;
  format.fmt.pix.pixelformat = requestedPixelFormat;
  format.fmt.pix.field = V4L2_FIELD_ANY;

  ret = ioctl(camera_fd, VIDIOC_S_FMT, &format);
  CHECK_V4L2ERROR(ret, "VIDIOC_S_FMT");

  ret = ioctl(camera_fd, VIDIOC_G_FMT, &format);
  CHECK_V4L2ERROR(ret, "VIDIOC_G_FMT");
  if(format.fmt.pix.pixelformat != requestedPixelFormat){
    //fprintf(stderr, "Requested format %.4s is not supported by %s\n", (char*)&requestedPixelFormat, vo.dev_name);
    fprintf(stderr, "Requested format %d is not supported by %s\n", requestedPixelFormat, vo.dev_name);
    exit(1);
  }
  if(format.fmt.pix.width != vo.width  || format.fmt.pix.height != vo.height){
    fprintf(stderr, "This resolution is not supported, please go through supported resolution by command './main -l'\n");
    exit(1);
  }
  printf("Input image format: (width, height) = (%u, %u), pixel format = %.4s\n",
      format.fmt.pix.width, format.fmt.pix.height, (char*)&format.fmt.pix.pixelformat);

  image_size = format.fmt.pix.sizeimage;
  if (image_size != IMAGESIZE) {
    fprintf(stderr, "Expecting image size to be 640 x 480 but got %d\n", image_size);
    exit(1);
  }
  pitch = format.fmt.pix.bytesperline;

}

static void uninit_camera(void){
  int ret = close(camera_fd);
  if (ret) {
    fprintf(stderr, "Failed to close %s: %s\n",
        vo.dev_name, strerror(errno));
    exit(1);
  }
}

static void process_frame(int frame, char (&buffer)[IMAGESIZE])
{
  std::string fname = "cameraFrameImport-" + std::to_string(frame) + ".jpg";
  //char* ff = fname.c_str();
  /*
  FILE *outfile = fopen(fname.c_str(), "wb" );
  // try to open file for saving
  if (!outfile) {
    perror("Couldn't open file to save mjpg frame");
  }
  */

  std::ofstream outFile;
  outFile.open(fname, std::ios::binary);
  if (!outFile) {
    perror("Couldn't open file to save mjpg frame");
  }

  outFile.write(buffer, image_size);

  // close output file
  outFile.close();
  //fclose(outFile);

}


static void captureDisplayloop(xoclutil::TestBO& bo0, xoclutil::TestBO& bo1, xoclutil::TestBO& bo2, xoclutil::TestBO& bo3, xoclutil::TestBO& bo4){
  int ret;
  struct v4l2_buffer buf;
  int index;


  while (frame_count < 20) {
    frame_count++;
    printf("******************Frame %d\n", frame_count);
    fd_set fd_selects;
    struct timeval tv;
    int r;

    FD_ZERO(&fd_selects);
    FD_SET(camera_fd, &fd_selects);

    /* Timeout 2sec. */
    tv.tv_sec = 2;
    tv.tv_usec = 0;


    r = select(camera_fd + 1, &fd_selects, NULL, NULL, &tv);

    if (-1 == r) {
      if (EINTR == errno)
        continue;
      perror("select");
    }

    if(r == 0){
      fprintf(stderr, "Select timeout\n");
      exit(1);
    }

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_DMABUF;
    ret = ioctl(camera_fd, VIDIOC_DQBUF, &buf);
    CHECK_V4L2ERROR(ret, "VIDIOC_DQBUF");
    index = buf.index;

    //Sarab: Above DQBUF IOCTL is telling us to which buffer has Camera device done DMABUF..
    //We get index of the buffer which has latest data; so we can get that and show it on display..
    //So DMABUF is done by camera device... ie data is sent to cl_mem by camera...
    //App is not reading data from buffers inside camera...



    //Send the frame to device
    switch (index) {
    case 0:
      bo0.sync(DRM_XOCL_SYNC_BO_TO_DEVICE, image_size);

      //Zero out the buffer for testing purposes
      std::memset(buffer0, 0, image_size);
      bo0.pwrite(buffer0, image_size);
      if (bo0.checksum() != 0)
          throw std::runtime_error("Could not clear BO " + bo0.name());

      break;
    case 1:
      bo1.sync(DRM_XOCL_SYNC_BO_TO_DEVICE, image_size);

      //Zero out the buffer for testing purposes
      std::memset(buffer0, 0, image_size);
      bo1.pwrite(buffer0, image_size);
      if (bo1.checksum() != 0)
          throw std::runtime_error("Could not clear BO " + bo1.name());
      break;
    case 2:
      bo2.sync(DRM_XOCL_SYNC_BO_TO_DEVICE, image_size);

      //Zero out the buffer for testing purposes
      std::memset(buffer0, 0, image_size);
      bo2.pwrite(buffer0, image_size);
      if (bo2.checksum() != 0)
          throw std::runtime_error("Could not clear BO " + bo2.name());
      break;
    case 3:
      bo3.sync(DRM_XOCL_SYNC_BO_TO_DEVICE, image_size);

      //Zero out the buffer for testing purposes
      std::memset(buffer0, 0, image_size);
      bo3.pwrite(buffer0, image_size);
      if (bo3.checksum() != 0)
          throw std::runtime_error("Could not clear BO " + bo3.name());
      break;
    case 4:
      bo4.sync(DRM_XOCL_SYNC_BO_TO_DEVICE, image_size);

      //Zero out the buffer for testing purposes
      std::memset(buffer0, 0, image_size);
      bo4.pwrite(buffer0, image_size);
      if (bo4.checksum() != 0)
          throw std::runtime_error("Could not clear BO " + bo4.name());
      break;
    default:
      perror("Unexpected buf_index received from camera");
      break;
    }

    //We should start FPGA Kernel at this point to process the frames inside FPGA



    //Get back FPGA processed frames to send to others or to display
    switch (index) {
    case 0:
      bo0.sync(DRM_XOCL_SYNC_BO_FROM_DEVICE, image_size);
      break;
    case 1:
      bo1.sync(DRM_XOCL_SYNC_BO_FROM_DEVICE, image_size);
      break;
    case 2:
      bo2.sync(DRM_XOCL_SYNC_BO_FROM_DEVICE, image_size);
      break;
    case 3:
      bo3.sync(DRM_XOCL_SYNC_BO_FROM_DEVICE, image_size);
      break;
    case 4:
      bo4.sync(DRM_XOCL_SYNC_BO_FROM_DEVICE, image_size);
      break;
    default:
      perror("Unexpected buf_index received from camera");
      break;
    }





    //process the received frame and display.
    //EnqueueKernel will have these buffers as arguments;
    //So the runTime will automatically sync these buffers to device
    //At this driver level testcase we will explicitly sync the buffers to device and read back to check...




    switch (index) {
    case 0:
      bo0.pread(buffer0, image_size);
      break;
    case 1:
      bo1.pread(buffer0, image_size);
      break;
    case 2:
      bo2.pread(buffer0, image_size);
      break;
    case 3:
      bo3.pread(buffer0, image_size);
      break;
    case 4:
      bo4.pread(buffer0, image_size);
      break;
    default:
      perror("Unexpected buf_index received from camera");
      break;
    }
    process_frame(frame_count, buffer0);


    //Then queue this buffer(buf.index) by QBUF; So that camera can again write to this buffer
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.m.fd = xilinx_bo_fd[index];
    buf.index = index;

    ret = ioctl(camera_fd, VIDIOC_QBUF, &buf);
    CHECK_V4L2ERROR(ret, "VIDIOC_QBUF");
  }

}



static int runTest(int xilinx_fd)
{
  int ret = 0;

  struct v4l2_requestbuffers reqbuf;

  memset(&reqbuf, 0, sizeof(reqbuf));
  reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  reqbuf.memory = V4L2_MEMORY_DMABUF;
  reqbuf.count = vo.buffer_num;
  ret = ioctl(camera_fd, VIDIOC_REQBUFS, &reqbuf);
  if(ret == -1 && errno == EINVAL){
    fprintf(stderr, "Video capturing or DMABUF streaming is not supported\n");
    exit(1);
  } else {
    CHECK_V4L2ERROR(ret, "VIDIOC_REQBUFS");
  }

  /*
  struct v4l2_create_buffers createbuf;

  //Asks camera to change to DMA BUF mode; And to create buffers on camera for DMABUF
  memset(&createbuf, 0, sizeof(createbuf));
  createbuf.memory = V4L2_MEMORY_DMABUF;
  createbuf.count = vo.buffer_num;

  //Format info we already have from camera init
  createbuf.format.type = format.type;
  createbuf.format.fmt.pix.width = format.fmt.pix.width;
  createbuf.format.fmt.pix.height = format.fmt.pix.height;
  createbuf.format.fmt.pix.pixelformat = format.fmt.pix.pixelformat;
  createbuf.format.fmt.pix.field = format.fmt.pix.field;
  createbuf.format.fmt.pix.sizeimage = format.fmt.pix.sizeimage;
  createbuf.format.fmt.pix.bytesperline = format.fmt.pix.bytesperline;

  ret = ioctl(camera_fd, VIDIOC_CREATE_BUFS, &createbuf);

  printf("===========================\n");
  printf("format type is: %d\n", createbuf.format.type);
  printf("format width is: %d\n", createbuf.format.fmt.pix.width);
  printf("format height is: %d\n", createbuf.format.fmt.pix.height);
  printf("format pixelformat is: %d\n", createbuf.format.fmt.pix.pixelformat);
  printf("format field is: %d\n", createbuf.format.fmt.pix.field);
  printf("format sizeimage is: %d\n", createbuf.format.fmt.pix.sizeimage);
  printf("format bytesperline is: %d\n", createbuf.format.fmt.pix.bytesperline);
  printf("===========================\n");
  printf("count is: %d\n", createbuf.count);


  if(ret == -1 && errno == EINVAL){
    fprintf(stderr, "Video capturing or DMABUF streaming is not supported\n");
    exit(1);
  } else {
    CHECK_V4L2ERROR(ret, "VIDIOC_CREATE_BUFS");
  }
  if (createbuf.count != vo.buffer_num) {
    fprintf(stderr, "Camera didn't allocate required number of buffers\n");
    exit(1);
  }
  int startCameraBufferIndex = createbuf.index;
  printf("Camera Buffer start index is: %d\n", startCameraBufferIndex);
  */


  //???Sarab: Our camera doesn't support CREATEBUF & EXPBUF IOCTLs;
  //So we can not test this path...
  //Received error is: VIDIOC_EXPBUF : Inappropriate ioctl for device
  //Received error is: VIDIOC_CREATE_BUFS : Inappropriate ioctl for device


  //Ask camera to export camera buffer FD; so that those can be imported on Xilinx FPGA
  struct v4l2_exportbuffer exportbuf;
  memset(&exportbuf, 0, sizeof(exportbuf));
  exportbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  //exportbuf.index = startCameraBufferIndex;
  exportbuf.index = 0;
  ret = ioctl(camera_fd, VIDIOC_EXPBUF, &exportbuf);
  if(ret == -1 && errno == EINVAL){
    fprintf(stderr, "Camera could not export buffer FD\n");
    exit(1);
  } else {
    CHECK_V4L2ERROR(ret, "VIDIOC_EXPBUF");
  }

  int buf0FD = exportbuf.fd;
  printf("Camera exported buf FD is: %d\n", buf0FD);





  /*


  //Put the Xilinx BOs in camera's queue to be filled.
  for (unsigned int i = 0; i < vo.buffer_num; ++i) {
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.index = i;
    buf.m.fd = xilinx_bo_fd[i];
    ret = ioctl(camera_fd, VIDIOC_QBUF, &buf);
    CHECK_V4L2ERROR(ret, "VIDIOC_QBUF");
  }

  //Ask camera to start capturing frames and it will those put in Xilinx BOs in the queue
  //Camera is already in DMA BUF mode
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ret = ioctl(camera_fd, VIDIOC_STREAMON, &type);
  CHECK_V4L2ERROR(ret, "VIDIOC_STREAMON");

  //Get frames and show them in a loop
  captureDisplayloop(bo0, bo1, bo2, bo3, bo4);

  //Ask camera to stop capturing frames
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ret = ioctl(camera_fd, VIDIOC_STREAMOFF, &type);
  CHECK_V4L2ERROR(ret, "VIDIOC_STREAMOFF");
  */


  return 0;
}

int main(int argc, char *argv[])
{
    const char *dev = "xocl";
    unsigned kind = 0;
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [xocl]\n";
        return 1;
    }

    if (argc == 2) {
        dev = argv[1];
        if (std::strcmp(dev, "xocl")) {
            std::cerr << "Usage: " << argv[0] << " [zocl]\n";
            return 1;
        }
    }

    int xilinx_fd = xoclutil::openDevice(dev);

    if (xilinx_fd < 0) {
        return -1;
    }

    init_camera();

    int result = 0;
    try {
        result = runTest(xilinx_fd);
        if (result == 0)
            std::cout << "PASSED TEST\n";
        else
            std::cout << "FAILED TEST\n";
    }
    catch (const std::exception& ex) {
        std::cout << ex.what() << std::endl;
        std::cout << "FAILED TEST\n";
    }

    uninit_camera();

    close(xilinx_fd);

    return result;
}


