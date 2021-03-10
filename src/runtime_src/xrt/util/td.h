/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef xrt_util_td_h_
#define xrt_util_td_h_

// Debug type deducer
namespace xrt_xocl { namespace td {

template <typename T>
struct TD;

template <typename T>
void fval(T val)
{
  TD<T> tType;
  TD<decltype(val)> tVal;
}

template <typename T>
void fref(T& val)
{
  TD<T> tType;
  TD<decltype(val)> tVal;
}

}} // td,xrt

#endif


