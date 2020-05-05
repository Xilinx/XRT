import pyopencl as cl
import numpy as np
import sys
from optparse import OptionParser
import time
import math

current_micro_time = lambda: int(round(time.time() * 1000000))

def cleanup(buffer_list):
   for buf in buffer_list:
      buf.release()

def main():
    platform_ID = None
    xclbin = None
    globalbuffersize = 1024*1024*0.07    #0.07 MB
    typesize = 512
    threshold = 40000
 
    # Process cmd line args
    parser = OptionParser()
    parser.add_option("-k", "--kernel", help="xclbin path")
    parser.add_option("-d", "--device", help="device index")
 
    (options, args) = parser.parse_args()
    xclbin = options.kernel
    index = options.device
    
    if xclbin is None:
       print("No xclbin specified\nUsage: -k <path to xclbin>")
       sys.exit(1)
    
    if index is None:
       index = 0 #get default device
    
    platforms = cl.get_platforms()
    # get Xilinx platform 
    for i in platforms:
       if i.name == "Xilinx":
          platform_ID = platforms.index(i)
          print("\nPlatform Information:")
          print("Platform name:       %s" %platforms[platform_ID].name)
          print("Platform version:    %s" %platforms[platform_ID].version)
          print("Platform profile:    %s" %platforms[platform_ID].profile)
          print("Platform extensions: %s" %platforms[platform_ID].extensions)
          break
 
    if platform_ID is None:
       #make sure xrt is sourced
       #run clinfo to make sure Xilinx platform is discoverable
       print("ERROR: Plaform not found")
       sys.exit(1)
 
    # choose device
    devices = platforms[platform_ID].get_devices()
    if int(index) > len(devices)-1:
       print("\nERROR: Index out of range. %d devices were found" % len(devices))
       sys.exit(1)
    else:
       dev = devices[int(index)]

    ctx = cl.Context(devices = [dev])
    if not ctx:
       print("ERROR: Failed to create context")
       sys.exit(1)
 
    commands = cl.CommandQueue(ctx, dev, properties=cl.command_queue_properties.OUT_OF_ORDER_EXEC_MODE_ENABLE)
 
    if not commands:
       print("ERROR: Failed to create command queue")
       sys.exit(1)
 
    print("Loading xclbin")
    with open(xclbin, "rb") as f:
       src = f.read()
    prg = cl.Program(ctx, [dev], [src])
    try:
       prg.build()
    except:
       print("ERROR:")
       print(prg.get_build_info(ctx, cl.program_build_info.LOG))
       raise
 
    knl1 = prg.bandwidth1
    knl2 = prg.bandwidth2
    
    #input host and buffer
    lst = [i%256 for i in range(int(globalbuffersize))]
    input_host1 = np.array(lst).astype(np.uint8)
    input_host2 = np.array(lst).astype(np.uint8)
    input_host3 = np.array(lst).astype(np.uint8)
    input_host4 = np.array(lst).astype(np.uint8)
    input_host5 = np.array(lst).astype(np.uint8)
    input_host6 = np.array(lst).astype(np.uint8)

    input_buf1 = cl.Buffer(ctx, cl.mem_flags.READ_WRITE | cl.mem_flags.COPY_HOST_PTR, hostbuf = input_host1)
    input_buf2 = cl.Buffer(ctx, cl.mem_flags.READ_WRITE | cl.mem_flags.COPY_HOST_PTR, hostbuf = input_host2)
    input_buf3 = cl.Buffer(ctx, cl.mem_flags.READ_WRITE | cl.mem_flags.COPY_HOST_PTR, hostbuf = input_host3)
    input_buf4 = cl.Buffer(ctx, cl.mem_flags.READ_WRITE | cl.mem_flags.COPY_HOST_PTR, hostbuf = input_host4)
    input_buf5 = cl.Buffer(ctx, cl.mem_flags.READ_WRITE | cl.mem_flags.COPY_HOST_PTR, hostbuf = input_host5)
    input_buf6 = cl.Buffer(ctx, cl.mem_flags.READ_WRITE | cl.mem_flags.COPY_HOST_PTR, hostbuf = input_host6)
    if any(buf is None for buf in [input_buf1.int_ptr, input_buf2.int_ptr, input_buf3.int_ptr, input_buf4.int_ptr, input_buf5.int_ptr, input_buf6.int_ptr]):
       print("ERROR: Failed to allocate source buffer")
       sys.exit(1)
    
    #output host and buffer
    output_host1 = np.empty_like(input_host1, dtype=np.uint8)
    output_host2 = np.empty_like(input_host2, dtype=np.uint8)
    output_host3 = np.empty_like(input_host2, dtype=np.uint8)
    output_host4 = np.empty_like(input_host2, dtype=np.uint8)
    output_host5 = np.empty_like(input_host2, dtype=np.uint8)
    output_host6 = np.empty_like(input_host2, dtype=np.uint8)
    
    output_buf1 = cl.Buffer(ctx, cl.mem_flags.READ_WRITE, output_host1.nbytes)
    output_buf2 = cl.Buffer(ctx, cl.mem_flags.READ_WRITE, output_host2.nbytes)
    output_buf3 = cl.Buffer(ctx, cl.mem_flags.READ_WRITE, output_host3.nbytes)
    output_buf4 = cl.Buffer(ctx, cl.mem_flags.READ_WRITE, output_host4.nbytes)
    output_buf5 = cl.Buffer(ctx, cl.mem_flags.READ_WRITE, output_host5.nbytes)
    output_buf6 = cl.Buffer(ctx, cl.mem_flags.READ_WRITE, output_host6.nbytes)
    
    if any(buf is None for buf in [output_buf1.int_ptr, output_buf2.int_ptr, output_buf3.int_ptr, output_buf4.int_ptr, output_buf5.int_ptr, output_buf6.int_ptr]):
       print("ERROR: Failed to allocate destination buffer")
       sys.exit(1)

    #copy dataset to OpenCL buffer
    globalbuffersizeinbeats = globalbuffersize/(typesize/8)
    tests= int(math.log(globalbuffersizeinbeats, 2.0))+1
 
    #lists
    buffer_list = [input_buf1, input_buf2, input_buf3, input_buf4, input_buf5, input_buf6, output_buf1, output_buf2, output_buf3, output_buf4, output_buf5, output_buf6]
    dnsduration = []
    dsduration  = []
    dbytes      = []
    dmbytes     = []
    bpersec     = []
    mbpersec    = []

    #run tests with burst length 1 beat to globalbuffersize
    #double burst length each test
    test=0
    beats = 16
    throughput = []
    while beats <= 1024:
        print("LOOP PIPELINE %d beats" % beats)
 
        usduration = 0
        fiveseconds = 5*1000000
        reps = 64
        while usduration < fiveseconds:

            start = current_micro_time()
            knl1(commands, (1, ), (1, ), output_buf1, input_buf1, output_buf2, input_buf2, output_buf3, input_buf3, np.uint32(beats), np.uint32(reps))
            knl2(commands, (1, ), (1, ), output_buf4, input_buf4, output_buf5, input_buf5, output_buf6, input_buf6, np.uint32(beats), np.uint32(reps))
            commands.finish()
            end = current_micro_time()

            usduration = end-start

            cl.enqueue_copy(commands, output_host1, output_buf1).wait()
            cl.enqueue_copy(commands, output_host2, output_buf2).wait()
            cl.enqueue_copy(commands, output_host3, output_buf3).wait()
            cl.enqueue_copy(commands, output_host4, output_buf4).wait()
            cl.enqueue_copy(commands, output_host5, output_buf5).wait()
            cl.enqueue_copy(commands, output_host6, output_buf6).wait()
            
            limit = int(beats*(typesize/8))
            if not np.array_equal(output_host1[:limit], input_host1[:limit]):
               print("ERROR: Failed to copy entries")
               cleanup(buffer_list)
               sys.exit(1)

            if not np.array_equal(output_host2[:limit], input_host2[:limit]):
               print("ERROR: Failed to copy entries")
               cleanup(buffer_list)
               sys.exit(1)
            
            if not np.array_equal(output_host3[:limit], input_host3[:limit]):
               print("ERROR: Failed to copy entries")
               cleanup(buffer_list)
               sys.exit(1)

            if not np.array_equal(output_host4[:limit], input_host4[:limit]):
               print("ERROR: Failed to copy entries")
               cleanup(buffer_list)
               sys.exit(1)

            if not np.array_equal(output_host5[:limit], input_host5[:limit]):
               print("ERROR: Failed to copy entries")
               cleanup(buffer_list)
               sys.exit(1)
            
            if not np.array_equal(output_host6[:limit], input_host6[:limit]):
               print("ERROR: Failed to copy entries")
               cleanup(buffer_list)
               sys.exit(1)
            
            # print("Reps = %d, Beats = %d, Duration = %lf us" %(reps, beats, usduration)) # for debug

            if usduration < fiveseconds:
                reps = reps*2


        dnsduration.append(usduration)
        dsduration.append(dnsduration[test]/1000000)
        dbytes.append(reps*beats*(typesize/8))
        dmbytes.append(dbytes[test]/(1024 * 1024))
        bpersec.append(6*dbytes[test]/dsduration[test])
        mbpersec.append(2*bpersec[test]/(1024 * 1024))
        throughput.append(mbpersec[test])
        print("Test %d, Throughput: %d MB/s" %(test, throughput[test]))
        beats = beats*4
        test+=1
    
    #cleanup
    cleanup(buffer_list)
    del ctx

    print("TTTT: %d" %throughput[0])
    print("Maximum throughput: %d MB/s" % max(throughput))
    if max(throughput) < threshold:
        print("ERROR: Throughput is less than expected value of %d GB/sec" %(threshold/1000))
        sys.exit(1)
    
    print("PASSED")

if __name__ == "__main__":
    main()
