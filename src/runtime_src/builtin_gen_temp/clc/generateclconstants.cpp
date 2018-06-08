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

#include <math.h>
#include <values.h>
#include <limits>
#include <iostream>

int main(int argc,char **argv){
  float m_e_f =         (float)M_E;
  float m_log2e_f =     (float)M_LOG2E;
  float m_log10e_f =    (float)M_LOG10E;
  float m_ln2_f =       (float)M_LN2;
  float m_ln10_f =      (float)M_LN10;
  float m_pi_f =        (float)M_PI;
  float m_pi_2_f =      (float)M_PI_2;
  float m_pi_4_f =      (float)M_PI_4;
  float m_1_pi_f =      (float)M_1_PI;
  float m_2_pi_f =      (float)M_2_PI;
  float m_2_sqrtpi_f =  (float)M_2_SQRTPI;
  float m_sqrt2_f =     (float)M_SQRT2;
  float m_sqrt1_2_f =   (float)M_SQRT1_2;

  float maxfloat =      (float) MAXFLOAT;
  
  float huge_valf =     (float) HUGE_VAL;
  float infinity =      (float) INFINITY;
  float nan =           (float) NAN;
  double huge_val=      HUGE_VAL;

  std::cout.precision(std::numeric_limits<double>::digits10);
  std::cout << "#define M_E_F " <<    m_e_f  << "\n";
  std::cout << "#define M_LOG2E_F " <<    m_log2e_f  << "\n";
  std::cout << "#define M_LOG10E_F " <<    m_log10e_f  << "\n";
  std::cout << "#define M_LN2_F " <<    m_ln2_f  << "\n";
  std::cout << "#define M_LN10_F " <<    m_ln10_f  << "\n";
  std::cout << "#define M_PI_F " <<    m_pi_f  << "\n";
  std::cout << "#define M_PI_2_F " <<    m_pi_2_f  << "\n";
  std::cout << "#define M_PI_4_F " <<    m_pi_4_f  << "\n";
  std::cout << "#define M_1_PI_F " <<    m_1_pi_f  << "\n";
  std::cout << "#define M_2_PI_F " <<    m_2_pi_f  << "\n";
  std::cout << "#define M_2_SQRTPI_F " <<    m_2_sqrtpi_f  << "\n";
  std::cout << "#define M_SQRT2_F " <<    m_sqrt2_f  << "\n";
  std::cout << "#define M_SQRT1_2_F " <<    m_sqrt1_2_f  << "\n";

  std::cout << "#define MAXFLOAT " << maxfloat << "\n";
  std::cout << "#define HUGE_VALF "  << huge_valf << "\n";
  std::cout << "#define INFINITY " << infinity << "\n";
  std::cout << "#define NAN " <<  nan << "\n";
  std::cout << "#define HUGE_VAL " << huge_val << "\n";

}


