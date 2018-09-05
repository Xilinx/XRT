
#if 0
// AutoESL intrinsics
void kernel _ssdm_RegionBegin(char *) __attribute__ ((nothrow));
void kernel _ssdm_RegionEnd(char*) __attribute__ ((nothrow));
void kernel _ssdm_op_SpecPipeline(int, int, int, char *) __attribute__ ((nothrow));

#define PIPELINE_BEGIN(label) {\
  _ssdm_RegionBegin(label);\
  _ssdm_op_SpecPipeline(1, 1, 1, "");
#define PIPELINE_END(label)\
  _ssdm_RegionEnd(label);}

#else
#define PIPELINE_BEGIN(label) {
#define PIPELINE_END(label) }
#endif

__kernel __attribute__ ((reqd_work_group_size(16, 16, 1)))
void mmult(__global int* a, __global int* b, __global int* output)
{
  int r = get_global_id(0);
  int c = get_global_id(1);
  int rank = get_global_size(0);
  int running = 0;
  for (int index=0; index<rank; index++) {
    PIPELINE_BEGIN("pipe0") {
      int aIndex = r*rank + index;
      int bIndex = index*rank + c;
      running +=  a[aIndex] * b[bIndex];
    } PIPELINE_END("pipe0")
  }
  barrier(CLK_LOCAL_MEM_FENCE); //not required
  output[r*rank + c] = running;
  //output[r*rank + c] = b[r*rank+c]+1;
  return;
}
