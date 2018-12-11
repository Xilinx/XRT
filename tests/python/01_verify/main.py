import sys
# import source files
sys.path.append('../../../src/python/')
from xrt_binding import *
from utils_binding import *


def main(args):
    opt = Options()
    Options.getOptions(opt, args)

    try:
        if initXRT(opt):
            return 1
        if opt.first_mem < 0:
            return 1
        if runKernel(opt):
            return 1

    except Exception as exp:
        print("Exception: ")
        print(exp)  # prints the err
        print("FAILED TEST")
        sys.exit()

    print("PASSED TEST")


if __name__ == "__main__":
    main(sys.argv)
