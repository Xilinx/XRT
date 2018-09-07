
__kernel __attribute__ ((reqd_work_group_size(16, 16, 1)))
void mmult(__global int* a, __global int* b, __global int* output)
{
  int rank = get_local_size(0); //16
  int running = 0;
  __local unsigned int buf[256];

  __attribute__((xcl_pipeline_workitems)) {
    int x = get_local_id(0);
    int y = get_local_id(1);
    buf[x*rank + y] = b[x*rank + y];
  }

  barrier(CLK_LOCAL_MEM_FENCE);

  int c = get_local_id(0);
  int r = get_local_id(1);

  __attribute__((xcl_pipeline_loop))
  for (int index=0; index<rank; index++) {
    int aIndex = r*rank + index;
    int bIndex = index*rank + c;
    running += a[aIndex] * buf[bIndex];
  }
  
  output[r*rank + c] = running;
  return;
}
