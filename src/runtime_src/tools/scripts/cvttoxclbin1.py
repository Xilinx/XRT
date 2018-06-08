#!/usr/bin/python

import os, sys, getopt
import subprocess, shlex
import glob, re

def create_xclbin1(filelist, outputfile):
    if(outputfile == ''):
	outputfile = "new.xclbin1"

    meta_file = '';
    delete_list = []
    cmd = ['xclbincat']

    for file in filelist:
	if file == "python-meta.xml":
	    print "meta file " + file
	    meta_file = file;
	    delete_list.append(file)
	elif file == "python-primary.bit":
	    print "primary file " + file
	    cmd.append("-bitstream")
	    cmd.append(file)
	    delete_list.append(file)
	elif file == "python-secondary.bit":
	    print "secondary file " + file
	    cmd.append("-clearstream")
	    cmd.append(file)
	    delete_list.append(file)
    if(meta_file != ''):
	cmd.append(meta_file)
    cmd.append(outputfile)
    cmd.append("-xclbin1");
    print "COMMAND: " + ' '.join(cmd)
    popen = subprocess.Popen(cmd, stdout=subprocess.PIPE);
    popen.wait()
    output = popen.stdout.read()
    print output
    for f in delete_list:
	os.remove(f)
    
def main(argv):
   inputfile = ''
   outputfile = ''
   try:
      opts, args = getopt.getopt(argv,"hi:o:",["ifile=","ofile="])
   except getopt.GetoptError:
      print 'test.py -i <inputfile> -o <outputfile>'
      sys.exit(2)
   for opt, arg in opts:
      if opt == '-h':
         print 'test.py -i <inputfile> -o <outputfile>'
         sys.exit()
      elif opt in ("-i", "--ifile"):
         inputfile = arg
      elif opt in ("-o", "--ofile"):
         outputfile = arg
   print 'Input file is :', inputfile
   print 'Output file is :', outputfile
   cmd = ['xclbinsplit']
   cmd.append(inputfile)
   cmd.append('-o')
   cmd.append('python')
   print "COMMAND: " + ' '.join(cmd)
   #popen = subprocess.Popen(['xclbinsplit', inputfile, '-o', 'python'], stdout=subprocess.PIPE);
   popen = subprocess.Popen(cmd, stdout=subprocess.PIPE);
   popen.wait()
   output = popen.stdout.read()
   print output
   x = glob.glob("python-*")
   create_xclbin1(x,outputfile)


if __name__ == "__main__":
   main(sys.argv[1:])
