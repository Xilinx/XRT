import gdb
import sys
import os
script_path = os.path.abspath(os.path.dirname(__file__))
sys.path.append(script_path)

class infCallUtil():
	def printstdstring (self, val):
		print ((val['_M_dataplus']['_M_p']).string())
	
	def callmethod(self, val, fname, args):
		"make method in fname on the obj ref 'val'"
		methodcall = "( (" + str(val.type) + ")" + "(" + str(val).split()[0] + ") )" + "->" + fname + "("

		cnt = 0
		for arg in args:
			if (cnt):
				methodcall += ","
			try:
				methodcall += "( (" + str(arg.type) + ")" + "(" + str(arg).split()[0] + ") )"
			except AttributeError:
				methodcall += "(" + str(arg).split()[0] + ")"
			cnt+=1
		methodcall += ")"
		#print ("methodcall = ", methodcall)

		return gdb.parse_and_eval(methodcall)

	def callfunc(self, fname, args):
		"make call fname(args)"
		funccall = fname + "("
		cnt = 0
		for val in args:
			if (cnt):
				funccall += ","
			funccall += "( (" + str(val.type) + ")" + "(" + str(val).split()[0] + ") )"
			cnt+=1
		funccall += ")"
		#print ("funccall = ", funccall)
		return gdb.parse_and_eval(funccall)

	def callfunc_verify(self, fname, args, objname):
		adv_ptr = self.callfunc(fname, args)
	
		free_args = []
		free_args.append(adv_ptr)
   		#adv_ptr is app_debug_view <some_data>*
		if (adv_ptr == 0):
			#print ("Unexpected error while getting list of {}".format(objname))
			errmsg = "Unexpected error while getting list of {}".format(objname)
			return free_args, 0, errmsg

		isInvalid = self.callmethod(adv_ptr,"isInValid", [])
		if (isInvalid):
			errmsg = self.callmethod(adv_ptr,"geterrmsg", [])
			#self.printstdstring(errmsg)
			errmsg = ((errmsg['_M_dataplus']['_M_p']).string())
			# free the allocated debug view data
			self.callfunc("appdebug::clFreeAppDebugView",free_args)
			return free_args, 0, errmsg

		ptr = self.callmethod(adv_ptr,"getdata", [])
		if (ptr == 0):
			#print ("Unexpected error while getting {} info".format(objname))
			errmsg = "Unexpected error while getting {} info".format(objname)
			# free the allocated debug view data
			self.callfunc("appdebug::clFreeAppDebugView",free_args)
			return free_args, 0, errmsg
		return free_args, ptr, ""
	
	def check_app_debug_enabled(self):
		try:
			isEnabled = self.callfunc("appdebug::isAppdebugEnabled", [])
		except:
			raise ValueError("Application debug not available. Application debug will be available after the first OpenCL API call.")
		if str(isEnabled) == "false":
			raise ValueError("Application debug not enabled. Set attribute 'app_debug=true' under 'Debug' section of sdaccel.ini and restart application")
		return

class printEventDebugViewVector(infCallUtil):
	def __init__(self, typename, val):
		self.val = val
		self.typename = typename
		self.start = self.val['_M_impl']['_M_start']
		self.finish = self.val['_M_impl']['_M_finish']
		self.item = self.start
		self.str_out = ""

	def printstring(self):
		while (self.item != self.finish) :
			elem = self.item.dereference()
			margs = []
			self.callmethod(elem, "print", margs)
			self.item = self.item + 1

	def getstring(self, verbose, json):
		if self.item == self.finish :
			if (json):
				return "\"events\" : []"
			else:
				return "None"

		if (json) :
			self.str_out = self.str_out + "\"events\":["

		while (self.item != self.finish) :
			elem = self.item.dereference()
			margs = [verbose, json]
			s = self.callmethod(elem, "getstring", margs)
			#print "s  = ", s['_M_dataplus']['_M_p'] #std::string
			#print "s.string()  = ", (s['_M_dataplus']['_M_p']).string() #std::string
			self.str_out =  self.str_out + "{" + (s['_M_dataplus']['_M_p']).string() + "}"
			self.item = self.item + 1
			#don't append newline at the end of string, for better formating
			if (self.item != self.finish): 
				if (json):
					self.str_out += ","   #events array elements
				else:
					self.str_out += "\n"

		if (json):
			self.str_out += "]"; #events
		return self.str_out

#########################################xpq commands #####################################
class printOccupancy (infCallUtil):
	"Print the number of events in command queue"
	def invoke (self, arg, from_tty):
		fargs = []
		fargs.append(gdb.parse_and_eval("(cl_command_queue)"+arg))

		#print "fargs = ", fargs

		free_args,pair_ptr,errmsg = self.callfunc_verify("appdebug::clPrintCmdQOccupancy",fargs, "cl_command_queue")
		#print ("free_args = {} pair_ptr = {} errmsg = {}".format(free_args,pair_ptr,errmsg))
		if (pair_ptr == 0):
			print (errmsg)
			return

		val = pair_ptr.dereference()
		#print (ret1)
		print ("Queued: {} Submitted: {}".format(val['first'], val['second']))

		# free the allocated vector
		self.callfunc("appdebug::clFreeAppDebugView",free_args)

obj_po = printOccupancy ()

class printSubmitted (infCallUtil):
	"Print the submitted commands"

	def invoke (self, arg, jsonformat):
		fargs = []
		fargs.append(gdb.parse_and_eval("(cl_command_queue)"+arg))

		free_args, vec_ptr, errmsg = self.callfunc_verify("appdebug::clPrintCmdQSubmitted",fargs, "submitted queue")
		if (vec_ptr == 0):
			if (jsonformat == True):
				print ("\"error\": \"{}\"".format (errmsg))
			else :
				print (errmsg)
			return

		c = printEventDebugViewVector(" ", vec_ptr.dereference())
		if (jsonformat):
			strout = c.getstring(0, 1)
			if (strout == 'None'):
				strout = ""
			#print ("\"submitted\": \{{}\}".format(strout))
			print (strout)
		else:
			strout = c.getstring(0, 0)
			print (strout)
	
		# free the allocated debug view data
		self.callfunc("appdebug::clFreeAppDebugView",free_args)

obj_ps = printSubmitted ()

class printQueued (infCallUtil):
	"Print the queued commands"

	def invoke (self, arg, jsonformat):
		fargs = []
		fargs.append(gdb.parse_and_eval("(cl_command_queue)"+arg))

		free_args,vec_ptr,errmsg = self.callfunc_verify("appdebug::clPrintCmdQQueued",fargs,"cl_command_queue")
		if (vec_ptr == 0):
			if (jsonformat == True):
				print ("\"error\": \"{}\"".format(errmsg))
			else :
				print (errmsg)
			return

		c = printEventDebugViewVector(" ", vec_ptr.dereference())
		if (jsonformat):
			strout = c.getstring(0, 1)
			if (strout == 'None'):
				strout = ""
			#print ("\"queued\": \{{}\}".format(strout))
			print (strout)
		else:
			strout = c.getstring(0, 0)
			print (strout)

		# free the allocated debug view data
		self.callfunc("appdebug::clFreeAppDebugView",free_args)

obj_pq = printQueued ()

#########################################xmem command#####################################
class printMemInfo (infCallUtil):
	"Print the information about the cl_mem object"
	def invoke (self, arg, jsonformat):
		fargs = []
		try:
			fargs.append(gdb.parse_and_eval("(cl_mem)"+ arg))
		except gdb.error as e:
			print e.message;
			return

		free_args,clm_ptr,errmsg = self.callfunc_verify("appdebug::clGetMemInfo",fargs,"cl_mem")
		if (clm_ptr == 0):
			if (jsonformat == True):
				print ("\"error\": \"{}\"".format (errmsg))
			else :
				print (errmsg)
			return

		if (jsonformat):
			stdstr = self.callmethod(clm_ptr,"getstring",[0, 1])
			strout = stdstr['_M_dataplus']['_M_p'].string()
			print strout
		else :
			stdstr = self.callmethod(clm_ptr,"getstring",[0, 0])
			strout = stdstr['_M_dataplus']['_M_p'].string()
			print (strout)

		# free the allocated vector
		self.callfunc("appdebug::clFreeAppDebugView",free_args)

obj_pm = printMemInfo ()

#########################################xpe command#####################################
class printEventInfo (infCallUtil):
	def invoke (self, arg, from_tty):
		fargs = []
		try:
			fargs.append(gdb.parse_and_eval("(cl_event)"+ arg))
		except gdb.error as e:
			print e.message;
			return

		free_args,cle_ptr,errmsg = self.callfunc_verify("appdebug::clGetEventInfo",fargs, "cl_event")

		if (cle_ptr == 0):
			print (errmsg)
			return

		stdstr = self.callmethod(cle_ptr,"getstring",[1, 0])
		strout = stdstr['_M_dataplus']['_M_p'].string()
		print (strout)

		# free the allocated vector
		self.callfunc("appdebug::clFreeAppDebugView",free_args)

obj_pe = printEventInfo ()

#########################################xprint command#####################################
class xprintPrefix(gdb.Command):
	"Xilinx command for printing OpenCL runtime data structure"
	def __init__(self):
		super(xprintPrefix, self).__init__(
						"xprint",
						gdb.COMMAND_USER,
						gdb.COMPLETE_COMMAND,
						True)
xprintPrefix()

class xprintQueue (gdb.Command, infCallUtil):
	"Print the command queue contents"
	def __init__ (self):
		super (xprintQueue, self).__init__ ("xprint queue", 
                         gdb.COMMAND_USER)

	def print_one_queue(self, arg, from_tty=True):
		try:
			gdb.parse_and_eval("(cl_command_queue)"+arg)
		except gdb.error as e:
			print e.message;
			return

		print ("Status:")
		obj_po.invoke(arg, from_tty)
		print ("\nQueued events:")
		obj_pq.invoke(arg, False)
		print ("\nSubmitted events:")
		obj_ps.invoke(arg, False)
		
	def invoke (self, arg="", from_tty=True):
		try:
			self.check_app_debug_enabled()
		except ValueError as e:
			print (e.message)
			return

		if arg == "":
			#No queues specified, print all queues
			fargs = []
			free_args, vec_ptr, errmsg = self.callfunc_verify("appdebug::clGetCmdQueues",fargs, "cl_command_queue")
			if (vec_ptr == 0):
				print (errmsg)
				return
			vec_val = vec_ptr.dereference()
			start = vec_val['_M_impl']['_M_start']
			finish = vec_val['_M_impl']['_M_finish']
			
			item = start
			print ("Available queues: {}".format(finish-start))
			count = 1
			while (item !=  vec_val['_M_impl']['_M_finish']):
				elem = item.dereference()
				print ("Queue-%d: %s" %(count, elem))
				self.print_one_queue(str(elem), from_tty)
				item=item+1
				if (item !=  vec_val['_M_impl']['_M_finish']):
						print ("") #introduces new line between every queues, except last
				count=count+1
			self.callfunc("appdebug::clFreeAppDebugView",free_args)
		else:
			self.print_one_queue(arg, from_tty)
all_queues=xprintQueue()

class xprintEvent (gdb.Command, infCallUtil):
	"Print the information about the cl_event object"
	def __init__ (self):
		super (xprintEvent, self).__init__ ("xprint event", 
                         gdb.COMMAND_USER)

	def invoke (self, arg, from_tty):
		try:
			self.check_app_debug_enabled()
		except ValueError as e:
			print (e.message)
			return
		if arg == "" :
			print ("No event argument specified")
			return
		obj_pe.invoke(arg, from_tty)  
xprintEvent()

class xprintMem (gdb.Command,infCallUtil):
	"Print the information about the cl_mem objects"
	def __init__ (self):
		super (xprintMem, self).__init__ ("xprint mem", 
                         gdb.COMMAND_USER)
	def invoke (self, arg="", from_tty=True):
		try:
			self.check_app_debug_enabled()
		except ValueError as e:
			print (e.message)
			return

		if arg == "":
			#No mem specified, print all mem
			fargs = []

			free_args,vec_ptr, errmsg = self.callfunc_verify("appdebug::clGetClMems",fargs, "cl_mem")
			if (vec_ptr == 0):
				print (errmsg)
				return
			vec_val = vec_ptr.dereference()
			start = vec_val['_M_impl']['_M_start']
			finish = vec_val['_M_impl']['_M_finish']
			print ("Available cl_mems: {}".format(finish-start))
			item = start
			while (item !=  vec_val['_M_impl']['_M_finish']):
				elem = item.dereference()
				obj_pm.invoke(str(elem), False)  
				item = item+1
			self.callfunc("appdebug::clFreeAppDebugView",free_args)
		else: 		
			obj_pm.invoke(arg, from_tty)  
all_mems=xprintMem()

class xprintKernel (gdb.Command,infCallUtil):
	"Print the information about all the submitted kernels"
	def __init__ (self):
		super (xprintKernel, self).__init__ ("xprint kernel", 
                         gdb.COMMAND_USER)
	def invoke (self, arg="", from_tty=True):
		try:
			self.check_app_debug_enabled()
		except ValueError as e:
			print (e.message)
			return

		if arg == "":
			fargs = []

			free_args, vec_ptr, errmsg = self.callfunc_verify("appdebug::clGetKernelInfo", fargs, "Kernel")
			if (vec_ptr == 0):
				print(errmsg)
				return
			vec_val = vec_ptr.dereference()
			start = vec_val['_M_impl']['_M_start']
			finish = vec_val['_M_impl']['_M_finish']
			print ("Number of Submitted Kernels: {}".format(finish-start))
			item = start
			while (item !=  vec_val['_M_impl']['_M_finish']):
				elem = item.dereference()
				stdstr = self.callmethod(elem,"getstring",[1, 0])
				#strout = unicode(stdstr['_M_dataplus']['_M_p'])
				strout = stdstr['_M_dataplus']['_M_p'].string()
				print (strout)
				item = item+1
			self.callfunc("appdebug::clFreeAppDebugView",free_args)
		else :
			print "xprint kernel does not accept arguments"
all_kernels=xprintKernel()

class xprintAll (gdb.Command,infCallUtil):
	"Print all command queues cl_mems and kernels"
	def __init__ (self):
		super (xprintAll, self).__init__ ("xprint all", 
                         gdb.COMMAND_USER)
	def invoke (self, arg="", from_tty=True):
		try:
			self.check_app_debug_enabled()
		except ValueError as e:
			print (e.message)
			return
		all_queues.invoke(arg,from_tty)
		print "";
		all_mems.invoke(arg,from_tty)
		print "";
		all_kernels.invoke(arg,from_tty)
xprintAll()

class xprintJSONPrefix(gdb.Command):
	"Xilinx internal command for printing OpenCL runtime data structure in JSON format"
	def __init__(self):
		super(xprintJSONPrefix, self).__init__(
						"xprint_json",
						gdb.COMMAND_USER,
						gdb.COMPLETE_COMMAND,
						True)
xprintJSONPrefix()

class xprintJSONQueue (gdb.Command, infCallUtil):
	"Print the command queue contents"
	def __init__ (self):
		super (xprintJSONQueue, self).__init__ ("xprint_json queue", 
                         gdb.COMMAND_USER)

	def print_one_queue(self, arg, from_tty=False):
		try:
			gdb.parse_and_eval("(cl_command_queue)"+arg)
		except gdb.error as e:
			print ("\"error\": \"{}\"".format(e.message));
			return

		print ("\"name\" : \"Queue-{}\",".format(arg))
		print ("\"queued\": {")
		obj_pq.invoke(arg, True)
		print ("},")

		print ("\"submitted\": {")
		obj_ps.invoke(arg, True)
		print ("}")
		
	def invoke (self, arg="", from_tty=False):
		try:
			self.check_app_debug_enabled()
		except ValueError as e:
			print ("\"error\": \"{}\"".format (e.message))
			return

		if arg == "":
			#No queues specified, print all queues
			fargs = []
			free_args, vec_ptr, errmsg = self.callfunc_verify("appdebug::clGetCmdQueues",fargs, "cl_command_queue")
			if (vec_ptr == 0):
				print ("\"error\": \"{}\"".format (errmsg))
				return
			vec_val = vec_ptr.dereference()
			start = vec_val['_M_impl']['_M_start']
			finish = vec_val['_M_impl']['_M_finish']
			
			item = start
			count = 1
			while (item !=  vec_val['_M_impl']['_M_finish']):
				if (count > 1) :
					print(",")
				print ("{")
				elem = item.dereference()
				self.print_one_queue(str(elem), False)
				item=item+1
				count=count+1
				print ("}") 
			self.callfunc("appdebug::clFreeAppDebugView",free_args)
		else:
			self.print_one_queue(arg, False)
all_json_queues=xprintJSONQueue()

class xprintJSONMem (gdb.Command,infCallUtil):
	"Print the information about the cl_mem objects"
	def __init__ (self):
		super (xprintJSONMem, self).__init__ ("xprint_json mem", 
                         gdb.COMMAND_USER)

	def print_one_mem(self, arg, from_tty=False):
		print ("\"name\" : \"Mem-{}\",".format(arg))
		print ("\"Details\": {")
		obj_pm.invoke(arg, True)
		print ("}")

	def invoke (self, arg="", from_tty=True):
		try:
			self.check_app_debug_enabled()
		except ValueError as e:
			print ("\"error\": \"{}\"".format (e.message))
			return

		if arg == "":
			#No mem specified, print all mem
			fargs = []

			free_args,vec_ptr, errmsg = self.callfunc_verify("appdebug::clGetClMems",fargs, "cl_mem")
			if (vec_ptr == 0):
				print ("\"error\": \"{}\"".format (errmsg))
				return
			vec_val = vec_ptr.dereference()
			start = vec_val['_M_impl']['_M_start']
			finish = vec_val['_M_impl']['_M_finish']
			item = start
			count = 1
			while (item !=  vec_val['_M_impl']['_M_finish']):
				if (count > 1) :
					print(",")
				print ("{")
				elem = item.dereference()
				self.print_one_mem(str(elem), False)
				item = item+1
				count=count+1
				print ("}") 
			self.callfunc("appdebug::clFreeAppDebugView",free_args)
		else: 		
			obj_pm.invoke(arg, from_tty)  
all_json_mems=xprintJSONMem()

#import the appdebugint.py which has the xstatus implementation
import appdebugint

class xprintJSONAll (gdb.Command,infCallUtil):
	"Print all command queues, cl_mems  in JSON format"
	def __init__ (self):
		super (xprintJSONAll, self).__init__ ("xprint_json all", 
                         gdb.COMMAND_USER)
	def invoke (self, arg="", from_tty=True):
		print ("{")
		try:
			self.check_app_debug_enabled()
		except ValueError as e:
			print ("\"error\": \"{}\"".format(e.message))
			print ("}")
			return

		print ("\"queues\": [")
		all_json_queues.invoke(arg,from_tty)
		print "],"

		print ("\"cl_mems\": [")
		all_json_mems.invoke(arg,from_tty)
		print "],"

		from appdebugint import xstatusJSONAllInfo
		all_json_xstatus=xstatusJSONAllInfo()

		print ("\"debugips\": [")
		all_json_xstatus.invoke(arg,from_tty)
		print "]"

		print "}"
xprintJSONAll()
