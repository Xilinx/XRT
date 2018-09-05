
//swizzle from x y z w ==>> y x w z

__kernel __attribute__ ((reqd_work_group_size(16, 1, 1)))
void vectorswizzle(__global int4 *a)
{
  int4 foo1,foo2;
  int r = get_global_id(0);

  foo1 = a[r];
  foo2 = foo1.zwxy;

 
  a[r] = foo2;
  return;
}
