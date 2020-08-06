
#ifndef OPENCL_COUNTERS_CALLBACKS_DOT_H
#define OPENCL_COUNTERS_CALLBACKS_DOT_H

// These are the functions that are visible when the plugin is dynamically
//  linked in.  XRT should call them directly.

extern "C"
void log_function_call_start(const char* functionName) ;

extern "C"
void log_function_call_end(const char* functionName) ;

extern "C"
void log_kernel_execution(const char* kernelName, bool isStart) ;

extern "C"
void log_compute_unit_execution(const char* cuName,
				const char* localWorkGroupConfiguration,
				const char* globalWorkGroupConfiguration,
				bool isStart) ;

#endif
