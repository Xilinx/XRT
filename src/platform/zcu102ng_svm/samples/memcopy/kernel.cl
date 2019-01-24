kernel __attribute__ ((reqd_work_group_size(1,1,1)))
void memcopy(__global int4  * __restrict input0,
             __global int4  * __restrict output0,
             ulong num_elements)
{
    __attribute__((xcl_pipeline_loop))
    for (ulong index=0; index < num_elements; index++) {
        int4 temp0 = input0[index];
        output0[index] = temp0;
    }
}
