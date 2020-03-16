import gdb
import sys
import os

script_path = os.path.abspath(os.path.dirname(__file__))
sys.path.append(script_path)

from appdebug import infCallUtil

class printSPMInfo (infCallUtil):
	"Print the Performance Monitor counters"
	def invoke (self, arg, jsonformat):
		fargs = []
		free_args,spm_ptr,errmsg = self.callfunc_verify("appdebug::clGetDebugCounters",fargs, "AIM")

		if (spm_ptr == 0):
			if (jsonformat == True):
				print ("[{{\"info\": \"{}\"}}]".format (errmsg))
			else :
				print ("Error reading AIMs : " + (errmsg))
			return 

		if (jsonformat):
			stdstr = self.callmethod(spm_ptr,"getstring",[1, 1]);
			strout = stdstr['_M_dataplus']['_M_p'].string();
			print strout
		else:
			stdstr = self.callmethod(spm_ptr,"getstring",[1, 0]);
			strout = stdstr['_M_dataplus']['_M_p'].string();
			print strout

		# free the allocated vector
		self.callfunc("appdebug::clFreeAppDebugView",free_args)

obj_spm = printSPMInfo ()

class printSSPMInfo (infCallUtil):
	"Print the Streaming Performance Monitor counters"
	def invoke (self, arg, jsonformat):
		fargs = []
		free_args,sspm_ptr,errmsg = self.callfunc_verify("appdebug::clGetDebugStreamCounters",fargs, "ASM")

		if (sspm_ptr == 0):
			if (jsonformat == True):
				print ("[{{\"info\": \"{}\"}}]".format (errmsg))
			else :
				print ("Error reading ASMs: " + (errmsg))
			return				

		if (jsonformat):
			stdstr = self.callmethod(sspm_ptr,"getstring",[1, 1]);
			strout = stdstr['_M_dataplus']['_M_p'].string();
			print strout
		else:
			stdstr = self.callmethod(sspm_ptr,"getstring",[1, 0]);
			strout = stdstr['_M_dataplus']['_M_p'].string();
			print strout

		# free the allocated vector
		self.callfunc("appdebug::clFreeAppDebugView",free_args)
obj_sspm = printSSPMInfo()

class printSAMInfo (infCallUtil):
	"Print the Accelerator Monitor counters"
	def invoke (self, arg, jsonformat):
		fargs = []
		free_args,sam_ptr,errmsg = self.callfunc_verify("appdebug::clGetDebugAccelMonitorCounters",fargs, "AM")

		if (sam_ptr == 0):
			if (jsonformat == True):
				print ("[{{\"info\": \"{}\"}}]".format (errmsg))
			else :
				print ("Error reading AMs : " + (errmsg))
			return				

		if (jsonformat):
			stdstr = self.callmethod(sam_ptr,"getstring",[1, 1]);
			strout = stdstr['_M_dataplus']['_M_p'].string();
			print strout
		else:
			stdstr = self.callmethod(sam_ptr,"getstring",[1, 0]);
			strout = stdstr['_M_dataplus']['_M_p'].string();
			print strout

		# free the allocated vector
		self.callfunc("appdebug::clFreeAppDebugView",free_args)
obj_sam = printSAMInfo()

class printLAPCInfo (infCallUtil):
	"Print the status of Lightweight AXI Protocol Checker"
	def invoke (self, arg, jsonformat):
		fargs = []
		free_args,lapc_ptr,errmsg = self.callfunc_verify("appdebug::clGetDebugCheckers",fargs, "LAPC")

		if (lapc_ptr == 0):
			if (jsonformat == True):
				print ("[{{\"info\": \"{}\"}}]".format (errmsg))
			else :
				print ("Error reading LAPCs: " + (errmsg))
			return				

		if (jsonformat):
			stdstr = self.callmethod(lapc_ptr,"getstring",[0, 1]);
			strout = stdstr['_M_dataplus']['_M_p'].string();
			print strout
		else :
			stdstr = self.callmethod(lapc_ptr,"getstring",[0, 0]);
			strout = stdstr['_M_dataplus']['_M_p'].string();
			print strout

		# free the allocated vector
		self.callfunc("appdebug::clFreeAppDebugView",free_args)

obj_lapc = printLAPCInfo ()

#########################################xstatus command#####################################
class xstatusPrefix(gdb.Command):
	"Xilinx command to show the status of all IPs"
	def __init__(self):
		super(xstatusPrefix, self).__init__(
						"xstatus",
						gdb.COMMAND_USER,
						gdb.COMPLETE_COMMAND,
						True)
xstatusPrefix()
class xstatusSPMInfo (gdb.Command,infCallUtil):
	"Print the Performance Monitor counters when available"
	def __init__ (self):
		super (xstatusSPMInfo, self).__init__ ("xstatus aim", 
                         gdb.COMMAND_USER)
	def invoke (self, arg, from_tty):
		try:
			self.check_app_debug_enabled()
		except ValueError as e:
			print (e.message)
			return
		obj_spm.invoke(arg, 0)
xstatusSPMInfo()
class xstatusSSPMInfo (gdb.Command,infCallUtil):
	"Print the Streaming Performance Monitor counters when available"
	def __init__ (self):
		super (xstatusSSPMInfo, self).__init__ ("xstatus asm", 
                         gdb.COMMAND_USER)
	def invoke (self, arg, from_tty):
		try:
			self.check_app_debug_enabled()
		except ValueError as e:
			print (e.message)
			return
		obj_sspm.invoke(arg, 0)
xstatusSSPMInfo()

class xstatusSAMInfo (gdb.Command,infCallUtil):
	"Print the Accelerator Monitor counters when available"
	def __init__ (self):
		super (xstatusSAMInfo, self).__init__ ("xstatus am", 
                         gdb.COMMAND_USER)
	def invoke (self, arg, from_tty):
		try:
			self.check_app_debug_enabled()
		except ValueError as e:
			print (e.message)
			return
		obj_sam.invoke(arg, 0)
xstatusSAMInfo()

class xstatusLAPCInfo (gdb.Command,infCallUtil):
	"Print the status of Lightweight AXI Protocol Checkers when available"
	def __init__ (self):
		super (xstatusLAPCInfo, self).__init__ ("xstatus lapc", 
                         gdb.COMMAND_USER)
	def invoke (self, arg, from_tty):
		try:
			self.check_app_debug_enabled()
		except ValueError as e:
			print (e.message)
			return
		obj_lapc.invoke(arg, 0)
xstatusLAPCInfo()

class xstatusAllInfo (gdb.Command,infCallUtil):
	"Print the status of all available IPs"
	def __init__ (self):
		super (xstatusAllInfo, self).__init__ ("xstatus all", 
                         gdb.COMMAND_USER)
	def invoke (self, arg, from_tty):
		try:
			self.check_app_debug_enabled()
		except ValueError as e:
			print (e.message)
			return
		obj_spm.invoke(arg, 0)
                obj_sspm.invoke(arg, 0)
		obj_sam.invoke(arg, 0)
		obj_lapc.invoke(arg, 0)
xstatusAllInfo()

class xstatusJSONPrefix(gdb.Command):
	"Xilinx command to show the status of all IPs in JSON format"
	def __init__(self):
		super(xstatusJSONPrefix, self).__init__(
						"xstatus_json",
						gdb.COMMAND_USER,
						gdb.COMPLETE_COMMAND,
						True)
xstatusJSONPrefix()

class xstatusJSONAllInfo (gdb.Command,infCallUtil):
	"Print the status of all available IPs"
	def __init__ (self):
		super (xstatusJSONAllInfo, self).__init__ ("xstatus_json all", 
                         gdb.COMMAND_USER)
	def invoke (self, arg, from_tty):
		print "{ \"aim\" : "
		obj_spm.invoke(arg,1)
		print ","
		print "\"lapc\" : "
		obj_lapc.invoke(arg,1)
		print "}"
all_json_xstatus=xstatusJSONAllInfo()

