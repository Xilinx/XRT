#!/usr/bin/python

import re
# import numpy as np

maxFreq = 500.0
inputFreq = 250.0
minVCOFreq = 800.0
maxVCOFreq = 1600.0

step = 100.0

table = {}
#config reg 0 : 7:0 : div, 15:8 mult, 25:16: frac mult

for vco in range(int(minVCOFreq), int(maxVCOFreq + 1.0), int(step)):
    print "================"
    vco = float(vco)
    print vco
    config0 = 1
    mul = vco/inputFreq;
    print mul;
    config0 = config0 | ((int)(mul) << 8)
    print format(config0, '08x')

    mul = round(mul, 3)
    frac = re.findall('\d+\.(\d+)', str(mul))
    print "frac: ", frac[0]
    if (int(frac[0]) > 0):
      config0 = config0 | (int(frac[0]) << 16)
      print format(config0, '08x')
      config0 = config0 | (0x00000001 << 26)
      print format(config0, '08x')


    for config1 in range(2,11):
        freq = vco/config1
        if (freq > maxFreq):
            continue
        print "Freq: %s, config0: %08x, config1: %08x" %(freq, config0, config1)
        table[int(freq)] = [int(config0), int(config1), int(vco)]
    print "================"

# for freq in sorted(table.keys()):
#     #print (freq, '--->', "08x"%(int(table[freq][0])))
#     print ("\t%u, \t0x%08x, \t0x%08x, \t%u " %(freq, table[freq][0], table[freq][1], table[freq][2]))

print "================"
for freq,v in sorted(table.items()):
    #print (freq, '--->', "08x"%(int(table[freq][0])))
    print ("\t%u, \t0x%08x, \t0x%08x, \t%u " %(freq, v[0], v[1], v[2]))

prevFreq = 10
print "================"
for freq,v in sorted(table.items()):
    if ((freq - prevFreq) < 5):
        continue
    prevFreq = freq
    #print (freq, '--->', "08x"%(int(table[freq][0])))
    print ("\t{/*%u*/\t%u,\t0x%08x,\t0x%08x}," %(v[2], freq, v[0], v[1]))
