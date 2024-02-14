/**
* Copyright (C) 2021 Xilinx, Inc
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

#pragma once

extern "C"
{
#include <xaiengine.h>
}

#include "xaiefal/xaiefal.hpp"
#include <memory>

class fal_util
{
public:
	/// returns true if initialization successful, false otherwise
	static bool initialize(XAie_DevInst* pDevInst);

	/// API to reserve resource for any resource type (including broadcast) and fetch the resource id.
	/// returns resource id on success, -1 otherwise.
	/// NOTE: 1. This API should not be used for cross module resource allocation.
	/// 	  2. API returns -1 if the resource is already reserved using same object.
	static int request(std::shared_ptr<xaiefal::XAieRsc> pResource);

	/// returns true if release successful, false otherwise
	static bool release(std::shared_ptr<xaiefal::XAieRsc> pResource);

	static std::shared_ptr<xaiefal::XAieDev> s_pXAieDev;
};
