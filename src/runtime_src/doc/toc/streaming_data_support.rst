
XRT Streaming Platform Support
==============================

Starting from the 2019.1 release, XRT supports a new programming model which supports the direct streaming of data from host to kernel and kernel to host without having to go through global memory. This feature is an addition to the existing host to kernel and kernel to host data transfer using global memories. By using streams, you can get some of the advantages such as:


- The host application does not necessarily need to know the size of the data coming from the kernel. Data resides on the host memory can be transferred to the kernel as soon as it is needed. Similarly, the processed data can be transferred back when it is required.
- This programming model uses minimal storage compared to the larger and slower global memory bank, and thus improving the performance and power.


APIs required for streaming data tranfer
----------------------------------------

**OpenCL™ extension APIs for streaming operation**

+-----------------+---------------------------------------------------+
| APIs            |      Description                                  |
+=================+===================================================+
| clCreateStream  | Creates a read or write stream                    |
+-----------------+---------------------------------------------------+
| clReleaseStream | Frees the created stream and its associated memory|
+-----------------+---------------------------------------------------+
| clWriteStream   | Writes data to stream                             |
+-----------------+---------------------------------------------------+
| clReadStream    | Gets data from stream                             |
+-----------------+---------------------------------------------------+
| clPollStreams   | Polls for any stream on the device to finish.     |
|                 | Required only for non-blocking stream operation.  |
+-----------------+---------------------------------------------------+

The typical API flow is described below:

- Create the required number of the read/write streams by clCreateStream.

     - Streams should be directly attached to the OpenCL device object because it does not use any command queue. A stream itself is a command queue that only passes the data to a particular direction, either from host to kernel or from kernel to host.
     - An appropriate flag should be used to denote stream write/read operation (from the kernel perspective).
     - To specify how the stream is connected to the device, a predefined extension pointer (cl_mem_ext_ptr_t) should be used to denote the kernel and its argument the stream is associated with.

In the code block below, a stream for kernel to host data transfer (named k2h_stream) and a stream for host to kernel data transfer (named h2k_stream) are created.

.. code-block:: c++

   #include <CL/cl_ext_xilinx.h> // Required for Xilinx® Extension

   // Device connection specification of the stream through extension pointer
   cl_mem_ext_ptr_t  ext;  // Extension pointer
   ext.param = kernel;     // The .param should be set to kernel (cl_kernel type)
   ext.obj = nullptr;

   // The .flag should be used to denote the kernel argument
   // Create write stream for argument 3 of kernel
   ext.flags = 3;
   cl_stream h2k_stream = clCreateStream(device_id, XCL_STREAM_READ_ONLY, CL_STREAM, &ext, &ret);

   // Create read stream for argument 4 of kernel
   ext.flags = 4;
   cl_stream k2h_stream = clCreateStream(device_id, XCL_STREAM_WRITE_ONLY, CL_STREAM, &ext,&ret);


- Set the remaining non-stream kernel arguments and enqueue the kernel. The following code block shows typical kernel argument (non-stream arguments such as buffer and/or scalar) setting and kernel enqueuing.

.. code-block:: c++

   // Set kernel non-stream argument (if any)
   clSetKernelArg(kernel, 0,...,...);
   clSetKernelArg(kernel, 1,...,...);
   clSetKernelArg(kernel, 2,...,...);
   // 3rd and 4th arguments are not set as those are already specified when creating the streams

   // Schedule kernel enqueue
   clEnqueueTask(commands, kernel, . .. . );

- Initiate Read and Write transfer by clReadStream and clWriteStream.

   - Note the usage of attribute cl_stream_xfer_req associated with read and write request.
   - The .flag is used to denote transfer mechanism.

       - **CL_STREAM_EOT:** Currently, successful stream transfer mechanism depends on identifying the end of the transfer by an End of Transfer signal. This flag is mandatory in the current release.
       - **CL_STREAM_NONBLOCKING:** By default the Read and Write transfers are blocking. For non-blocking transfer, CL_STREAM_NONBLOCKING has to be set.
   - The .priv_data is used to specify a string (as a name for tagging purpose) associated with the transfer. This will help identify specific transfer completion when polling the stream completion. It is required when using the non-blocking version of the API.

In the following code block, the stream read and write transfers are executed with the non-blocking approach.

.. code-block:: c++

   // Initiate the READ transfer
   cl_stream_xfer_req rd_req {0};

   rd_req.flags = CL_STREAM_EOT | CL_STREAM_NONBLOCKING;
   rd_req.priv_data = (void*)"read"; // You can think this as tagging the transfer with a name
   clReadStream(k2h_stream, host_read_ptr, max_read_size, &rd_req, &ret);

   // Initiating the WRITE transfer
   cl_stream_xfer_req wr_req {0};

   wr_req.flags = CL_STREAM_EOT | CL_STREAM_NONBLOCKING;
   wr_req.priv_data = (void*)"write";

   clWriteStream(h2k_stream, host_write_ptr, write_size, &wr_req , &ret);

**IMPORTANT**: In case of using blocking version of the API, the user should be careful as blocking API blocks the host execution. Hence it may ends up application to hang, for example a blocking read operation from a kernel before a blocking write to the same kernel (in the situation when the kernel output stream depends on the kernel input stream) in the same thread. The general recommendation is to use blocking streams APIs from differnt threads to avoid application hang situation. 

**IMPORTANT**: The buffer used for kernel to host data transfer has to be page aligned (In the above code example, the buffer ``host_read_ptr`` has to be page aligned). 


- Poll all the streams for completion. For the non-blocking transfer, a polling API is provided to ensure the read/write transfers are completed. For the blocking version of the API, polling is not required.

   - The number of poll requests should be used through cl_streams_poll_req_completions.
   - The ``clPollStreams`` is a blocking API. It returns the execution to the host code as soon as it receives the notification that all stream requests have been completed, or until you specify the timeout.

.. code-block:: c++

   // Checking the request completion
   cl_streams_poll_req_completions poll_req[2] {0, 0}; // 2 Requests

   auto num_compl = 2;
   clPollStreams(device_id, poll_req, 2, 2, &num_compl, 5000, &ret);
   // Blocking API, waits for 2 poll request completion or 5000ms, whichever occurs first

- Read and use the stream data in host.

   - After the successful poll request is completed, the host can read the data from the host pointer.
   - Also, the host can check the size of the data transferred to the host. For this purpose, the host needs to find the correct poll request by matching ``priv_data`` and then fetching nbytes (the number of bytes transferred) from the ``cl_streams_poll_req_completions`` structure.

.. code-block:: c++

   for (auto i=0; i<2; ++i) {
       if(rd_req.priv_data == poll_req[i].priv_data) { // Identifying the read transfer

	// Getting read size, data size from kernel is unknown
        ssize_t result_size=poll_req[i].nbytes;
        }
   }

The header file containing function prototype and argument description is available in the XRT GitHub repository.

**IMPORTANT**: If the streaming kernel has multiple CUs, the host code needs to use a unique ``cl_kernel`` object for each CU. The host code must use ``clCreateKernel`` with <kernel_name>:{compute_unit_name} to get each CU, creating streams for them, and enqueuing them individually.
