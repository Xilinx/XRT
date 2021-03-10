# this code is based on validate testcase
import pyopencl as cl
import numpy as np
import sys
from optparse import OptionParser

def main():
   platform_ID = None
   xclbin = None
   original = np.array(("Hello World"))
   length = 20

   # Process cmd line args
   parser = OptionParser()
   parser.add_option("-k", "--kernel", help="xclbin path")
   parser.add_option("-d", "--device", help="device index")


   (options, args) = parser.parse_args()
   xclbin = options.kernel
   index = options.device

   if(xclbin is None):
      print("No xclbin specified\nUsage: -k <path to xclbin>")
      sys.exit()

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
      sys.exit()

   # choose device
   devices = platforms[platform_ID].get_devices()
   if int(index) > len(devices)-1:
      print("\nERROR: Index out of range. %d devices were found" %len(devices))
      sys.exit(1)
   else:
      dev = devices[int(index)]

   ctx = cl.Context(devices = [dev])
   if not ctx:
      print("ERROR: Failed to create context")
      sys.exit()

   commands = cl.CommandQueue(ctx, dev)

   if not commands:
      print("ERROR: Failed to create command queue")
      sys.exit()

   print("Loading xclbin")

   prg = cl.Program(ctx, [dev], [open(xclbin).read()])

   try:
      prg.build()
   except:
      print("ERROR:")
      print(prg.get_build_info(ctx, cl.program_build_info.LOG))
      raise

   # allocate memory on the device
   d_buf = cl.Buffer(ctx, cl.mem_flags.WRITE_ONLY, size = length)

   try:
      prg.hello(commands, (1, ), (1, ), d_buf)
   except:
      print("ERROR: Failed to execute the kernel")
      raise

   # read the result
   try:
      h_buf = np.zeros_like(original)
      cl.enqueue_copy(commands, h_buf, d_buf)
   except:
      print("ERROR: Failed to read the output")
      raise

   print("Result: %s" % h_buf)

   # cleanup
   d_buf.release()
   del ctx

if __name__ == "__main__":
   main()