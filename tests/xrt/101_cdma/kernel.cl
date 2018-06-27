/*
  OpenCL Task (1 work item)
  512 bit wide add one
  512 bits = 8 vector of 64 bit unsigned
    Add one to first element in vector
    Copy through remaining elements
*/

__kernel __attribute__ ((reqd_work_group_size(1, 1 , 1)))
void addone (__global ulong8 *a, __global ulong8 * b, unsigned int  elements)
{
  ulong8 temp;
  unsigned int i;

  for(i=0;i< elements;i++){
    temp=a[i];
    //add one to first element in vector
    temp.s0=temp.s0+1;
    b[i]=temp;
  }
  return;
}

__kernel __attribute__ ((reqd_work_group_size(1, 1 , 1)))
void copy (__global ulong8 *in, __global ulong8 *out, unsigned int elements)
{
  ulong8 temp;
  unsigned int i;

  for(i=0;i< elements;i++){
    out[i]=in[i];
  }
  return;
}