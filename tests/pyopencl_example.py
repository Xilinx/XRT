# this code is based on validate testcase
import pyopencl as cl
import sys
from optparse import OptionParser

def main():
    platform_ID = None
    xclbin = None

    # Process cmd line args
    parser = OptionParser()
    parser.add_option("-k", "--kernel", help="xclbin path")

    (options, args) = parser.parse_args()
    xclbin = options.kernel
    if(xclbin is None):
        print("No xclbin specified\nUsage: -k <path to xclbin>")
        sys.exit()

    # allocate host buffer
    h_buf = "00000000000000000000" #len=20

    print("List all available platforms:")
    platforms = cl.get_platforms()
    print(platforms)

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

    # get all devices
    devices = platforms[platform_ID].get_devices()
    print("\n%d devices were found" %len(devices))
    print(devices)
    
    ctx = cl.Context(dev_type=cl.device_type.ALL,
            properties=[(cl.context_properties.PLATFORM, platforms[platform_ID])])
    if not ctx:
        print("ERROR: Failed to create context")
        sys.exit()

    commands = cl.CommandQueue(ctx)

    if not commands:
        print("ERROR: Failed to create command queue")
        sys.exit()

    print("Loading xclbin")

    f = open(xclbin, "rb")
    try:
        src = f.read()
    except (FileNotFoundError, IOError):
        print("Wrong xclbin path")
    finally:
        f.close()

    # something is wrong with this
    prg = cl.Program(ctx, devices[0], src)

    try:
        prg.build()
    except:
        print("Error:")
        print(prg.get_build_info(ctx.devices[0], cl.program_build_info.LOG))
        raise

    # the code below is not tested, but it should provide a basic guideline 
    # for writing the rest of the code

    # allocate memory on the device
    d_buf = cl.Buffer(ctx, cl.mem_flags.WRITE_ONLY, size = len(h_buf))

    # execute kernel
    prg.hello(commands, len(h_buf), None, d_buf)

    # read the result
    cl.enqueue_copy(commands, d_buf, h_buf) #maybe enqueue_read_buffer()

    print("Result: %s" %h_buf)
    print("Done")

    # cleanup
    del ctx

if __name__ == "__main__":
    main()


# helpful links:
# 1. https://www.bu.edu/pasi/files/2011/01/AndreasKloeckner2-05-0900.pdf
# 2. https://github.com/rcloud/PyOpenCL-OpenCL-Programming-Guide-Examples/blob/master/Hello%20World/HelloWorld.py
# 3. https://github.com/inducer/pyopencl/tree/master/examples
# 4. http://www.training.prace-ri.eu/uploads/tx_pracetmo/LinkSCEMM_pyOpenCL.pdf


# Doc: https://documen.tician.de/pyopencl/runtime.html
