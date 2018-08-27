#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <unistd.h>
#include <CL/opencl.h>

#define NUM_WORKGROUPS (1)
#define WORKGROUP_SIZE (16)
#define LENGTH 16


int
load_file_to_memory(const char *filename, char **result)
{
    int size = 0;
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

int main(int argc, char** argv)
{
    cl_platform_id platform_id ;
    cl_device_id device_id ;
    cl_context context;
    cl_command_queue command_queue;

    cl_program program ;
    cl_kernel kernel ;
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret;
    char cl_platform_vendor[1001];
    char cl_platform_name[1001];
    int check_status = 0;
    int A[LENGTH], B[LENGTH], C[LENGTH], D[LENGTH];

    int i;

    if (argc != 2) {
	printf("Usage: %s bandwidth.xclbin\n", argv[0]);
	return EXIT_FAILURE;
    }

    cl_int err; 

    /* Initialize input data */
    for (i=0; i< LENGTH ; i++) {
	A[i] = i*12 , B[i] = i*2 , C[i] = i + 12;
    }

    /* Get platform/device information */
    ret = clGetPlatformIDs(1, &platform_id, NULL);

    // Connect to a compute device

    int fpga = 1;

    ret = clGetDeviceIDs( platform_id, fpga ? CL_DEVICE_TYPE_ACCELERATOR : CL_DEVICE_TYPE_CPU, 1, &device_id, NULL);                   
    /* Create OpenCL Context */
    context = clCreateContext( 0, 1, &device_id, NULL, NULL, &ret);

    /* Create command queue */
    command_queue = clCreateCommandQueue(context, device_id, 0, &ret);

    /* Create Program Object */

    // Load binary from disk
    int status;
    unsigned char *kernelbinary;
    char *xclbin=argv[1];
    printf("loading %s\n", xclbin);
    int n_i = load_file_to_memory(xclbin, (char **) &kernelbinary);
    if (n_i < 0) {
	printf("failed to load kernel from xclbin: %s\n", xclbin);
	printf("Test failed\n");
	return EXIT_FAILURE;
    }
    size_t n = n_i;
    // Create the compute program from offline
    program = clCreateProgramWithBinary(context, 1, &device_id, &n,(const unsigned char **) &kernelbinary, &status, &ret);


    // Build the program executable

    ret  = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);

    kernel = clCreateKernel(program, "vadd", &ret);

    cl_mem input_bufferA ;
    cl_mem_ext_ptr_t input_bufferA_ext;
    input_bufferA_ext.flags = 0;
    input_bufferA_ext.obj = NULL;
    input_bufferA_ext.param = kernel;
    input_bufferA = clCreateBuffer(context,
	    CL_MEM_READ_ONLY |  CL_MEM_EXT_PTR_XILINX ,
	    LENGTH*sizeof(int),
	    &input_bufferA_ext,
	    &err);
    cl_mem input_bufferB;   
    cl_mem_ext_ptr_t input_bufferB_ext;
    input_bufferB_ext.flags = 1;
    input_bufferB_ext.obj = NULL;
    input_bufferB_ext.param = kernel;
    input_bufferB = clCreateBuffer(context,
	    CL_MEM_READ_ONLY |  CL_MEM_EXT_PTR_XILINX ,
	    LENGTH*sizeof(int),
	    &input_bufferB_ext,
	    &err);
    cl_mem input_bufferC;        
    cl_mem_ext_ptr_t input_bufferC_ext;
    input_bufferC_ext.flags = 2;
    input_bufferC_ext.obj = NULL;
    input_bufferC_ext.param = kernel;
    input_bufferC = clCreateBuffer(context,
	    CL_MEM_READ_ONLY |  CL_MEM_EXT_PTR_XILINX ,
	    LENGTH*sizeof(int),
	    &input_bufferC_ext,
	    &err);

    cl_mem output_bufferD ;
    cl_mem_ext_ptr_t output_bufferD_ext;
    output_bufferD_ext.flags = 3;
    output_bufferD_ext.obj = NULL;
    output_bufferD_ext.param = kernel;
    output_bufferD = clCreateBuffer(context,
	    CL_MEM_READ_ONLY |  CL_MEM_EXT_PTR_XILINX ,
	    LENGTH*sizeof(int),
	    &output_bufferD_ext,
	    &err);



    /* Copy input data to memory buffer */

    clEnqueueWriteBuffer(command_queue, input_bufferA, CL_TRUE, 0, LENGTH*sizeof(int), A, 0, NULL, NULL);
    clEnqueueWriteBuffer(command_queue, input_bufferB, CL_TRUE, 0, LENGTH*sizeof(int), B, 0, NULL, NULL);
    clEnqueueWriteBuffer(command_queue, input_bufferC, CL_TRUE, 0, LENGTH*sizeof(int), C, 0, NULL, NULL);





    //////////////////////////////////////////////////////////////
    /* Set OpenCL kernel arguments */


    clSetKernelArg(kernel, 0, sizeof(cl_mem), &input_bufferA);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &input_bufferB);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &input_bufferC);
    clSetKernelArg(kernel, 3, sizeof(cl_mem), &output_bufferD);    



    clFinish(command_queue); 
    cl_event event;
    size_t global[1];
    size_t local[1];

    global[0] = NUM_WORKGROUPS * WORKGROUP_SIZE;
    local[0] = WORKGROUP_SIZE;
    ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, (size_t*)&global, (size_t*)&local, 0, NULL, NULL);           
    if (ret != CL_SUCCESS) {
	std::printf("ERROR: Failed to execute kernel %d\n", ret);
	std::printf("ERROR: Test failed\n");
	return EXIT_FAILURE;
    }

    clFinish(command_queue); 
    /* Copy result to host */

    clEnqueueReadBuffer(command_queue, output_bufferD, CL_TRUE, 0, LENGTH*sizeof(int), D, 0, NULL, NULL);



    printf("Check Results ................................\n");

    for (int i = 0; i < LENGTH; i++) {
	if (A[i] + B[i] + C[i] != D[i]) {
	    check_status = 1;
	    printf("ERROR in vadd - %d - c=%d\n", i,D[i]);}
    }

    printf("Displaying results ......................\n");

    for (i=0; i<LENGTH ; i++) {
	printf("A: %d,B: %d,C: %d,D: %d\n", A[i]  ,B[i]  ,C[i], D[i]);
	printf("\n");
    }

    if (check_status == 1) {
	printf("INFO: Test failed\n");
	return EXIT_FAILURE;
    } else {
	printf("INFO: Test passed\n");
    }
    clReleaseMemObject(input_bufferA);      
    clReleaseMemObject(input_bufferB);      
    clReleaseMemObject(input_bufferC);    
    clReleaseMemObject(output_bufferD);
    clReleaseKernel(kernel);
}
