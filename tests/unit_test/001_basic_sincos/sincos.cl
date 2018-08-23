#define NUM_POINTS 768

//__kernel __attribute__ ((reqd_work_group_size(NUM_POINTS, 1, 1)))
__kernel void test_sincos(__global float* a, __global float* b, __global float2* output)
{
  int iter = get_local_id(0);
  
  output[iter].x = (float)cos((float)M_PI_F * 2 * -1 * (float)iter / (NUM_POINTS));
  output[iter].y = (float)sin((float)M_PI_F * 2 * -1 * (float)iter / (NUM_POINTS));

  return;
}
