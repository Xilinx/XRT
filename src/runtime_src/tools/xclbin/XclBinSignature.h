/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef __XclBinSignature_h_
#define __XclBinSignature_h_

#include <string>

// ----------------------- I N C L U D E S -----------------------------------

// #includes here - please keep these to a bare minimum!


// ------------ F O R W A R D - D E C L A R A T I O N S ----------------------

// ---------------------- F U N C T I O N S ----------------------------------

void signXclBinImage(const std::string & _fileOnDisk, const std::string & _sPrivateKey, const std::string & _sCertificate);
void verifyXclBinImage(const std::string & _fileOnDisk, const std::string & _sCertificate);

#endif
