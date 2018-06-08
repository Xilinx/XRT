/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include <iostream>
#include <type_traits>

namespace xcl {

namespace test {

////////////////////////////////////////////////////////////////
// Type deducer. Handy for template work
////////////////////////////////////////////////////////////////
template <typename T>
struct TD;

template <typename T>
void fval(T param)
{
  TD<T> tType;
  TD<decltype(param)> paramType;
}

template <typename T>
void frefref(T&& param)
{
  TD<T> tType;
  TD<decltype(param)> paramType;
}

} // test


}


