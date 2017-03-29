/*  An aggresively average SIMD python module
 *  Copyright (C) 2017 David Akeley
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <stdint.h>
#include <emmintrin.h>
#include <immintrin.h>

#ifndef MediocrePy_MEDIAN_H_
#define MediocrePy_MEDIAN_H_

static const size_t mediocre_max_array_count = 10000000;

int mediocre_clipped_median_u16(
    uint16_t* out,
    uint16_t const* const* data,
    size_t array_count,
    size_t bin_count,
    double sigma_lower,
    double sigma_upper, 
    size_t max_iter
);

int mediocre_clipped_median(
    void* out, uintptr_t output_type_code,
    void const* input_pointers, uintptr_t input_type_data,
    size_t array_count,
    size_t bin_count,
    double sigma_lower,
    double sigma_upper,
    size_t max_iter
);

static inline int mediocre_clipped_median_mu16(
    uint16_t* out,
    uint16_t* const* data,
    size_t array_count,
    size_t bin_count,
    double sigma_lower,
    double sigma_upper, 
    size_t max_iter
) {
    uint16_t const* const* const_data = (uint16_t const* const*)data;
    return mediocre_clipped_median_u16(
        out, const_data, array_count, bin_count,
        sigma_lower, sigma_upper, max_iter
    );
}

#endif

