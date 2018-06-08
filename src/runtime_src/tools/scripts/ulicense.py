#Usage: /usr/bin/python3 ulicense.py /proj/xsjhdstaff4/umangp/newbr/HEAD/src/products/sdx/ocl/src/runtime_src/**/*.h
import sys

with open("./user.lic", "r") as f:
    lic = f.read()

print(lic)

for i in range(1,len(sys.argv)):

    print("Processing : " + sys.argv[i])

    filename = sys.argv[i]
    with open(filename, 'r') as original: orig = original.read()
    with open(filename, 'w') as modified: modified.write(lic + orig)

