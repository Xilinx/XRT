/* File: /proj/xsjhdstaff3/gill/sdsoc/2018.3/regression/products/sdsoc/platforms/prebuilt/generate/zc702/add_gen_prebuilt/apf/_sds/p0/.cf_work/portinfo.c */
#include "cf_lib.h"
#include "cf_request.h"
#include "accel_info.h"
#include "sds_lib.h"
#include "sds_trace.h"
#include "portinfo.h"
#include <stdio.h>  // for printf
#include <stdlib.h> // for exit
#include "xlnk_core_cf.h"
#include "sysport_info.h"

extern void pfm_hook_init(void);
extern void pfm_hook_shutdown(void);
void p_NIL_init_pl(void)
{
}

void p_NIL_shutdown_pl()
{
}


void p_NIL_cf_framework_open(void) __attribute__((constructor));
void p_NIL_cf_framework_close(void) __attribute__((destructor));
void p_NIL_cf_framework_open(void)
{
  if (xlnkOpen()) {
    cf_context_init();
    xlnkCounterMap(666666687);
    pfm_hook_init();
    sds_trace_setup(0x0);
  }
  p_NIL_init_pl();
}

#ifdef PERF_EST
void add_sw_estimates();
#endif

void p_NIL_cf_framework_close(void)
{
  p_NIL_shutdown_pl();
  if(xlnkDecrementRef()) {
    sds_trace_cleanup();
    pfm_hook_shutdown();
    xlnkClose(1, NULL);
  }
#ifdef PERF_EST
  add_sw_estimates();
  apf_perf_estimation_exit();
#endif
}
