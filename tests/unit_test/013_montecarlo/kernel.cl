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

// Copyright 2017 Xilinx, Inc. All rights reserved.

//#define NSAMP 262144
#define NSAMP 2048

#ifdef __xilinx__
#define M_PI 3.14159265358979f
#endif

#define UINT_THIRTY 30U
#define UINT_BIG_CONST 1812433253U
#define NORM_RANGE 4294967296.0f

// Mersenne twister algorithm constants. Please refer for details
// http://en.wikipedia.org/wiki/Mersenne_twister
// Matsumoto, M.; Nishimura, T. (1998). "Mersenne twister: a 623-dimensionally equidistributed uniform pseudo-random number generator".
// ACM Transactions on Modeling and Computer Simulation 8 (1): 3–30.

// degree of Mersenne twister recurrence
#define N 624
// middle word
#define M 397

// Mersenne twister constant 2567483615
#define MATRIX_A 0x9908B0DFU
// Mersenne twister constant 2636928640
#define MASK_B 0x9D2C5680U
// Mersenne twister constant 4022730752
#define MASK_C 0xEFC60000U
// Mersenne twister tempering shifts
#define SHIFT_U 11
#define SHIFT_S 7
#define SHIFT_T 15
#define SHIFT_L 18

#ifdef __xilinx__
__attribute__ ((reqd_work_group_size(256,1,1)))
#endif
kernel void montecarlo(global float * restrict vcall,
                           global float * restrict vput,
                           float r,
                           float sigma,
                           global const float *s_price,
                           global const float *k_price,
                           global const float *t)
{
    int tid = get_global_id(0);

    // Constants
    const float int_to_float_normalize_factor = 1.0f/NORM_RANGE;
    const float s_price_cache = s_price[tid];
    const float k_price_cache = k_price[tid];
    const float t_cache = t[tid];

    // MonteCarlo algorithm
    // S(T) = S(0) exp((r -1/2*sigma^2)T + sigma * W(T) ),
    // W(T) is Gaussian variable with mean 0 and variance T
    const float tmp_bs1 = (r - sigma * sigma * 0.5f) * t_cache;
    const float tmp_bs2 = sigma * sqrt(t_cache); // formula reference: sigma * (T)^(1/2)

    float vcall_local_arr[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    int i, iCurrentMiddle, iCurrent;
    // Mersenne twister generated random number
    uint mt_rnd_num;
    // State of the MT generator
    int mt_state_a[N];
    int mt_state_b[N];
    // Temporary state for MT states swap
    int tmp_mt;

    // set seed
    mt_state_a[0] = tid;
    mt_state_b[0] = tid;

    // Initialize the MT generator from a seed
    int pval = tid;
    int val = 0;
#ifdef __xilinx__
    __attribute__((xcl_pipeline_loop))
#endif
    for (i = 1; i < N; i++)
    {
        val = (uint)i + UINT_BIG_CONST * (pval ^ (pval >> UINT_THIRTY));
        mt_state_a[i] = val;
        mt_state_b[i] = val;
        pval = val;
    }

    // Initialize MT state
    i = 0;
    tmp_mt = tid;

#ifdef __xilinx__
    __attribute__((xcl_pipeline_loop))
#endif
    for (int sample = 0; sample < NSAMP/2; sample++)
    {
        // Mersenne twister loops generating untempered and tempered values in original description merged here together with Box-Muller
        // normally distributed random numbers generation and Black&Scholes formula.

        // First MT random number generation
        // Calculate new state indexes
        iCurrent = (iCurrent == N - 1) ?  0 : i + 1;
        iCurrentMiddle = (i + M >= N) ? i + M - N : i + M;

        int mt_state_i = tmp_mt;
        tmp_mt = mt_state_a[iCurrent];

        // MT recurrence
        // Generate untempered numbers
        mt_rnd_num = (mt_state_i & 0x80000000U) | (tmp_mt & 0x7FFFFFFFU);
        mt_rnd_num = mt_state_b[iCurrentMiddle] ^ (mt_rnd_num >> 1) ^ ((0-(mt_rnd_num & 1))& MATRIX_A);

        mt_state_a[i] = mt_rnd_num;
        mt_state_b[i] = mt_rnd_num;

        // Tempering pseudorandom number
        mt_rnd_num ^= (mt_rnd_num >> SHIFT_U);
        mt_rnd_num ^= (mt_rnd_num << SHIFT_S) & MASK_B;
        mt_rnd_num ^= (mt_rnd_num << SHIFT_T) & MASK_C;
        mt_rnd_num ^= (mt_rnd_num >> SHIFT_L);

        float rnd_num = (float)mt_rnd_num;

        i = iCurrent;

        // Second MT random number generation
        // Calculate new state indexes
        iCurrent = (iCurrent == N - 1) ?  0 : i + 1;
        iCurrentMiddle = (i + M >= N) ? i + M - N : i + M;

        mt_state_i = tmp_mt;
        tmp_mt = mt_state_a[iCurrent];

        // MT recurrence
        // Generate untempered numbers
        mt_rnd_num = (mt_state_i & 0x80000000U) | (tmp_mt & 0x7FFFFFFFU);
        mt_rnd_num = mt_state_b[iCurrentMiddle] ^ (mt_rnd_num >> 1) ^ ((0-(mt_rnd_num & 1))& MATRIX_A);


        mt_state_a[i] = mt_rnd_num;
        mt_state_b[i] = mt_rnd_num;

        // Tempering pseudorandom number
        mt_rnd_num ^= (mt_rnd_num >> SHIFT_U);
        mt_rnd_num ^= (mt_rnd_num << SHIFT_S) & MASK_B;
        mt_rnd_num ^= (mt_rnd_num << SHIFT_T) & MASK_C;
        mt_rnd_num ^= (mt_rnd_num >> SHIFT_L);

        float rnd_num1 = (float)mt_rnd_num;

        i = iCurrent;

        // Make uniform random variables in (0,1] range
        rnd_num = (rnd_num + 1.0f) * int_to_float_normalize_factor;
        rnd_num1 = (rnd_num1 + 1.0f) * int_to_float_normalize_factor;

        // Generate normally distributed random numbers
        // Box-Muller
        float tmp_bm = sqrt(fmax(-2.0f*log(rnd_num), 0.0f)); // max added to be sure that sqrt argument non-negative
        rnd_num = tmp_bm*cos(2.0f*M_PI*rnd_num1);
        rnd_num1 = tmp_bm*sin(2.0f*M_PI*rnd_num1);


        // Stock price formula
        // Add first sample from pair
        float tmp_bs3 = rnd_num*tmp_bs2 + tmp_bs1; // formula reference: NormalDistribution*sigma*(T)^(1/2) + (r - (sigma^2)/2)*(T)
        tmp_bs3 = s_price_cache*exp(tmp_bs3); // formula reference: S * exp(CND*sigma*(T)^(1/2) + (r - (sigma^2)/2)*(T))


        float dif_call = tmp_bs3-k_price_cache;

        float vcall_temp = fmax(dif_call, 0.0f);

        // Add second sample from pair
        tmp_bs3 = rnd_num1*tmp_bs2 + tmp_bs1;
        tmp_bs3 = s_price_cache*exp(tmp_bs3);


        dif_call = tmp_bs3-k_price_cache;

        vcall_temp += fmax(dif_call, 0.0f);
        vcall_local_arr[sample%4] += vcall_temp;
    }

    const float vcall_local = vcall_local_arr[0] + vcall_local_arr[1] + vcall_local_arr[2] + vcall_local_arr[3];
    // Average
    vcall[tid] = vcall_local / ((float)NSAMP) * exp(-r*t_cache);

    // Calculate put option price from call option price: put = call – S0 + K * exp( -rT )
    vput[tid] = vcall[tid] - s_price_cache + k_price_cache * exp(-r*t_cache);
}
