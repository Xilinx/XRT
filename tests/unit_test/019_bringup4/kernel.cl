
#ifdef __xilinx__
__attribute__ ((reqd_work_group_size(128, 1, 1)))
#endif
kernel void loopback (global char * restrict s1,
                      global const char *s2)
{
    const int start = 64 * get_global_id(0);
    const int end = start + 64;
#ifdef __xilinx__
    __attribute__((xcl_pipeline_loop))
#endif
    for (int i = start; i < end; i++) {
        s1[i] = s2[i];
    }
}
