/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc
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

#include "core/edge/include/pscontext.h"

class pscontext_impl {
private:
  bool aie_profile_en;
};

pscontext::pscontext()
  : pimpl(std::make_unique<pscontext_impl>()) {};

pscontext::~pscontext() = default;
pscontext::pscontext(pscontext&& rhs) = default;
pscontext& pscontext::operator=(pscontext&& rhs) = default;
