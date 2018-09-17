
#ifdef __xilinx__
__attribute__ ((reqd_work_group_size(1, 1, 1)))
#endif
kernel void loopback (global char * restrict s1,
                      global const char *s2,
                      int length)
{
#ifdef __xilinx__
    __attribute__((xcl_pipeline_loop))
#endif
    for (int i = 0; i < length; i++) {
        s1[i] = s2[i];
    }
}
