/**
 * Copyright (C) 2016-2018 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <time.h>

//return elapsed time in ns from t0 to t1
static inline double time_elapsed(struct timespec t0, struct timespec t1){
  return ((double)t1.tv_sec - (double)t0.tv_sec) * 1.0E9 + ((double)t1.tv_nsec - (double)t0.tv_nsec);
}
