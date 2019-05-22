import pyopencl as cl
import sys

platform_ID = None

print("List all available platforms:")
platforms = cl.get_platforms()
print(platforms)

# get Xilinx platform 

for i in platforms:
    if i.name == "Xilinx":
        platform_ID = platforms.index(i)
        print("\nPlatform Information:")
        print(platforms[platform_ID].name)
        print(platforms[platform_ID].version)
        print(platforms[platform_ID].profile)
        print(platforms[platform_ID].extensions)
        break

if platform_ID is None:
    print("ERROR: Plaform not found")
    sys.exit()

# get all devices

devices = platforms[platform_ID].get_devices()
print("\n%d devices were found" %len(devices))
print(devices)
 




