#include <time.h>

//return elapsed time in ns from t0 to t1
static inline double time_elapsed(struct timespec t0, struct timespec t1){
  return ((double)t1.tv_sec - (double)t0.tv_sec) * 1.0E9 + ((double)t1.tv_nsec - (double)t0.tv_nsec);
}
