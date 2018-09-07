
__kernel __attribute__ ((reqd_work_group_size(16, 16, 1)))
void mmult(__global int* a, __global int* b, __global int* output)
{
  int r = get_local_id(0);
  int c = get_local_id(1);
  int rank = get_local_size(0);
  int running = 0;

  for (int index=0; index<16; index++) {
    int aIndex = r*rank + index;
    int bIndex = index*rank + c;
    running +=  a[aIndex] * b[bIndex];
  }
  
  output[r*rank + c] = running;
  return;
}
