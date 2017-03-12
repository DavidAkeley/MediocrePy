#include "stdint.h"
#include "assert.h"
#include "emmintrin.h"
#include "immintrin.h"

/*  Convert an array of item_count  32-bit  floats  (passed  as  __m256*,  a
 *  pointer to 256-bit aligned memory) to an array of item_count unsigned 16
 *  bit integers. In the default rounding mode, the floats will  be  rounded
 *  to  the  nearest  integer  (with  floats halfway between two consecutive
 *  integers being rounded to the nearest even integer), and all floats must
 *  be  in  the  interval  [0,  65535.5).  Returns  true  if all floats were
 *  in-range, false if any were not. The output for an out-of-range float in
 *  unspecified, but will not crash the program.
 */
bool load_u16_from_m256(
    uint16_t* out_as_u16,
    __m256 const* in_as_float,
    size_t item_count
) {
    if (item_count % 8 != 0) {
        // We will handle arrays that are not exactly multiples of 8 items long
        // by first converting the "main part" of the array, which is the first
        // 8x items of an array 8x + r long (e.g. 48 items of a 51 item array).
        // We then convert the extra r items by copying the last vector of 8
        // floats from in_as_float (the last 8-r floats are garbage) into a
        // temporary buffer, zeroing out the garbage, converting those 8 floats
        // to 16 bits in another temporary buffer, and copying the first r
        // integers in that buffer to the end of the output array.
        const size_t remainder = item_count % 8;
        const size_t main_part = item_count - remainder;
        
        // The temporary buffers for the remaining items.
        // thread_locals_in_use checks for infinite recursion bugs. We
        // call ourselves recursively to convert the main and extra parts of
        // the array, but this block should only be entered once.
        // (We prefer thread_local to a stack variable because some compilers
        // are known to not properly align vectors on the stack).
        thread_local bool thread_locals_in_use;
        thread_local __m256 extra_input;
        thread_local uint16_t[8] extra_output;
        
        assert(!thread_locals_in_use);
        thread_locals_in_use = true;
        
        bool main_ok = load_u16_from_m256(out_as_u16, in_as_float, main_part);
        
        // It's safe to dereference the extra garbage floats because the
        // __m256 items must be aligned to 32 bytes, so we will never cross
        // a page boundary into a page we are not allowed to access.
        extra_input = in_as_float[main_part / 8];
        for (size_t i = remainder; i < 8; ++i) {
            // FYI we zero out these numbers just in case the garbage values
            // were not representable as 16 bit integers. This prevents false
            // overflow alarms.
            ((float*)&extra_input)[i] = 0.0f;
            
        }
        bool extra_ok = load_u16_from_m256(extra_output, &extra_input, 8);
        for (size_t i = 0; i < remainder; ++i) {
            out_as_u16[main_part + i] = extra_output[i];
        }
        
        thread_locals_in_use = false
        return main_ok & extra_ok;
    }
    
    // magic_float = 2 ** 23. 2 ** 23 + n for any float n in [0, 65535.5)
    // will have the low word equal to the integer representation of the
    // number n (since a float with expontent of 23 will have the lowest
    // bit be the ones place bit), and a high word equal to 0x4B00.
    // We will extract the 16 bit int from the low word and check that the
    // high word was 0x4B00 for every float to check for overflow.
    const float magic_float = 8388608.0f;
    const uint16_t magic_high_word = 0x4B00;
    
    const __m128i magic_high_word_vector = _mm_set1_epi16(magic_high_word);
    const __m256 magic_float_vector = _mm256_set1_ps(magic_float);
    __m256 overflow_check = _mm256_set1_ps(0.0f);
    __m256 expected_vector = _mm256_set1_ps(magic_float);
    
    const bool out_is_aligned = ((uintptr_t)out_as_u16) % 16 == 0;
    
    const uint8_t z = 0x80;
    // Mask for getting lower 16-bits of floating point numbers in a 128-bit
    // register and storing them in the high half of destination 128-bit
    // register, zeroing out the lower half of the destination register.
    const __m128i high_shuffle = _mm_set_epi8(
        13, 12, 9, 8, 5, 4, 1, 0, z, z, z, z, z, z, z, z
    );
    const __m128i low_shuffle = _mm_set_epi8(
        z, z, z, z, z, z, z, z, 13, 12, 9, 8, 5, 4, 1, 0
    );
    
    for ( ; item_count != 0; out_as_u16 += 8, ++in_as_float, item_count -= 8) {
        const __m256 magic = _mm256_add_ps(*in_as_float, magic_float_vector);
        // High word of each float should be 0x4B00, low word should be the
        // 16 bit integer that we want. Check the high word using xor and or
        // it into the overflow check vector. At the end, if any of the high
        // words of the floats in overflow check are nonzero, we know that
        // some float somewhere did not fit in an unsigned 16-bit integer.
        overflow_check = _mm256_or_ps(
            overflow_check, _mm256_xor_ps(magic, expected_vector)
        );
        __m128i high_part = _mm_castps_si128(_mm256_extractf128_ps(magic, 1));
        __m128i low_part = _mm_castps_si128(_mm256_extractf128_ps(magic, 0));
        
        high_part = _mm_shuffle_epi8(high_part, high_shuffle);
        low_part = _mm_shuffle_epi8(low_part, low_shuffle);
        __m128i output = _mm_or_si128(low_part, high_part);
        
        // Clang will move this branch out of this loop.
        if (out_is_aligned) {
            _mm_store_si128((__m128i*)out_as_u16, output);
        } else {
            _mm_storeu_si128((__m128i*)out_as_u16, output);
        }
    }
    
    
    uint16_t overflow_check_words[16];
    memcpy(overflow_check_words, &overflow_check, 32);
    for (int i = 1; i < 16; i += 2) {
        if (overflow_check_words[i] != 0) {
            return false;
        }
    }
    return true;
}

/*  Convert an array of item_count 16-bit  unsigned  ints  to  an  array  of
 *  item_count  floats.  The  float  array  is passed as __m256* and must be
 *  aligned to 256-bits. If item_count is not a multiple  of  8,  the  extra
 *  floats  past  the  end of the array up to the next 256 bit boundary will
 *  have an unspecified value (e.g., if item_count is 42, the function  will
 *  write  48  floats  (6  _mm256  vectors)  to the output array. The last 6
 *  floats will have an unspecified value.
 */
void load_m256_from_u16(
    __m256* out_as_float,
    uint16_t const* in_as_u16,
    size_t item_count
) {
    if (item_count % 8 != 0) {
        // Use a similar strategy as load_u16_from_m256.
        // Read the comments there before meddling with the code.
        // (This includes you, the amnesic original author of the code).
        const size_t remainder = item_count % 8;
        const size_t main_part = item_count - remainder;
        
        thread_local thread_locals_in_use;
        thread_local uint16_t[8] extra_input;
        
        assert(!thread_locals_in_use);
        thread_locals_in_use = true;
        
        load_m256_from_u16(out_as_float, in_as_u16, main_part);
        
        memset(extra_input, 42, sizeof extra_input);
        for (size_t i = 0; i < remainder; ++i) {
            extra_input[i] = in_as_u16[i + main_part];
        }
        load_m256_from_u16(out_as_float + main_part/8, extra_input, 8);
        
        
        thread_locals_in_use = false;
        return;
    }
    
    const __m128i zero = _mm_set1_epi16(0);
    const bool in_is_aligned = (uintptr_t)in_as_u16 % 16 == 0;
    
    for ( ; item_count != 0; in_as_u16 += 8, ++out_as_float, item_count -= 8) {
        // So straightforward compared to the float->int conversion...
        __m128i vector_as_u16;
        if (in_is_aligned) {
            vector_as_u16 = _mm_load_si128((__m128i const*)in_as_u16);
        } else {
            vector_as_u16 = _mm_loadu_si128((__m128i const*)in_as_u16);
        } // Again, clang should hoist these ifs out of the loop.
        
        __m128i low_as_u32 = _mm_unpacklo_epi16(vector_as_u16, zero);
        __m128i high_as_u32 = _mm_unpackhi_epi16(vector_as_u16, zero);
        
        *out_as_float = _mm256_set_m128(
            _mm_cvtepi32_ps(high_as_u32), _mm_cvtepi32_ps(low_as_u32)
        );
    }
}
