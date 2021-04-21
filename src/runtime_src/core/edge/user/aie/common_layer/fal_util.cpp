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

#include "fal_util.h"

std::shared_ptr<xaiefal::XAieDev> fal_util::s_pXAieDev;

bool fal_util::initialize(XAie_DevInst* pDevInst)
{
    if (!pDevInst) return false;
    s_pXAieDev = std::make_shared<xaiefal::XAieDev>(pDevInst);
    return true;
}

int fal_util::request(std::shared_ptr<xaiefal::XAieRsc> pResource)
{
    if (!pResource) return -1;

    int xaiefalStatus = AieRC::XAIE_OK;
    if (auto pBroadcastRsc = std::dynamic_pointer_cast<xaiefal::XAieBroadcast>(pResource))
    {
        xaiefalStatus = pBroadcastRsc->reserve(); // Will return error if already reserved
        return (xaiefalStatus == AieRC::XAIE_OK ? pBroadcastRsc->getBc() : -1);
    }
    else if (auto pSingleTileRsc = std::dynamic_pointer_cast<xaiefal::XAieSingleTileRsc>(pResource))
    {
        int id = -1;
        XAie_LocType loc;
        XAie_ModuleType module;
        xaiefalStatus = pSingleTileRsc->reserve(); // Will return error if already reserved
        if (xaiefalStatus == AieRC::XAIE_OK)
            xaiefalStatus = pSingleTileRsc->getRscId(loc, module, (uint32_t&)id);
        return (xaiefalStatus == AieRC::XAIE_OK ? id : -1);
    }

    return -1;
}

bool fal_util::release(std::shared_ptr<xaiefal::XAieRsc> pResource)
{
    if (!pResource) return false;
    int xaiefalStatus = AieRC::XAIE_OK;
    xaiefalStatus |= pResource->release(); // If resource is not reserved, release() will not error out
    return (xaiefalStatus == AieRC::XAIE_OK);
}
