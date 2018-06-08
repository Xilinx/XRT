import glob
import yaml
import os, os.path
from subprocess import call
from subprocess import Popen,PIPE,STDOUT

HALLIB = "libxclgemdrv.so"
DSA_PLATFORM = "xilinx_xil-accel-rd-ku115_4ddr-xpr_4_0"
date = '0503'
output = '/scratch/umangp/sprite_' + date
dsa = 'ku115'
ver = '4_0'
#base = '/proj/rdi-xco/fisusr/no_backup/results/sdx_2017.1/sdx_ll_hw/'
base = '/proj/rdi-xco/fisusr/no_backup/results/sdx_2017.1/sdx_unit_hw/'


def get_args(ymlfile):
	#print get_args.__name__
	#print ymlfile
	yml = glob.glob(ymlfile)
	#print yml
	f = open(yml[0])
	data = yaml.safe_load(f)
	f.close()
	return data['args']
	

def main():
	file = open('py.log', 'w+')
	os.environ["HALLIB"]= HALLIB
	os.environ["DSA_PLATFORM"]= DSA_PLATFORM
	call(["mkdir", "-p", output])
	search = base + '*' + dsa + '*' + ver
	result = glob.glob(search)
	
	try:

		for i in result:
			dirname = os.path.split(i)[1]
			#print dirname
			
			design = dirname[:10]
			#print design
			
			
			abcd= raw_input()
			if abcd == "n"
				continue
			
			new_dir = output + os.sep + design
			print new_dir
			
			call(["mkdir", "-p", new_dir])		
			
			ymlfile = i + os.sep + '*' + date + '*' + os.sep + 'sdainfo.yml'
			args = get_args(ymlfile)
			print args
			
			yml = glob.glob(ymlfile)
			
			call(["cp", "-f", yml[0], new_dir])
			
			resdir = os.path.split(yml[0])[0]
			
			xbinst = resdir + os.sep + 'xbinst' + os.sep + '*'
			#print xbinst
			
			xbinst_files = glob.glob(xbinst)
			#for filename in os.listdir(xbinst):
			for filename in xbinst_files:
				#print "UKP: " + filename
				if ".log" in filename:
					continue
				if os.path.isfile(filename):
					call(["cp", "-f", filename, new_dir])
					#print "UKP2: " +filename
			
			
			print "......Running the test now... "
			#cmd = "./host.exe " + args +  " | tee output.log"
			#print cmd
			
			my_env = os.environ.copy()
			my_env["HALLIB"]= HALLIB
			my_env["DSA_PLATFORM"]= DSA_PLATFORM
			
			p = Popen(["./host.exe " + args ], shell=True, cwd=new_dir, env=my_env)
			#p = Popen(["./host.exe " + args + " | tee output.log" + "; dmesg -c| tee dmesg.log"], shell=True, cwd=new_dir, env=my_env)
			p.wait()
			#sts = Popen(["./host.exe ", args, " | tee output.log" ], shell=False, cwd=new_dir, env=my_env)
			abcd= raw_input()
			
			file.write("\nReturn code of " + design + " : " + str(p.returncode))
			print "Return code of " + design + " : " + str(p.returncode)
			print "========================================================"
	except:
		print "catching exception..."
		file.close()

	
if __name__ == "__main__":
	main()
	
