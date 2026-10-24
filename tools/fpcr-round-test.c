// cvt* under all four MXCSR rounding modes; prints one checksum line per mode and instruction.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <immintrin.h>

static uint64_t mix(uint64_t h, uint32_t v) { return (h ^ v) * 0x100000001b3ull; }

int main(void)
{
    for (unsigned mode = 0; mode < 4; ++mode) {
        _mm_setcsr((_mm_getcsr() & ~0x6000u) | (mode << 13));
        uint64_t h1 = 0xcbf29ce484222325ull, h2 = h1, h3 = h1, h4 = h1, h5 = h1;
        uint64_t x = 0x9e3779b97f4a7c15ull;
        for (int i = 0; i < 200000; ++i) {
            x ^= x << 13; x ^= x >> 7; x ^= x << 17;
            int64_t n = (int64_t)x >> (i & 31);
            float f; uint32_t u;
            __asm__ volatile("cvtsi2ssq %1, %0" : "=x"(f) : "r"(n));
            memcpy(&u, &f, 4); h1 = mix(h1, u);
            __asm__ volatile("vcvtsi2ssl %k1, %0, %0" : "+x"(f) : "r"((int32_t)n));
            memcpy(&u, &f, 4); h2 = mix(h2, u);
            double d = (double)(int64_t)x * 1.000000123456789e-7;
            __m128d pd = _mm_set_pd(d * 3.3, d);
            __m128 ps;
            __asm__ volatile("cvtpd2ps %1, %0" : "=x"(ps) : "x"(pd));
            uint32_t w[4]; memcpy(w, &ps, 16); h3 = mix(mix(h3, w[0]), w[1]);
            __asm__ volatile("vcvtpd2ps %1, %0" : "=x"(ps) : "x"(pd));
            memcpy(w, &ps, 16); h4 = mix(mix(h4, w[0]), w[1]);
            __asm__ volatile("cvtsd2ss %1, %0" : "=x"(f) : "x"(d));
            memcpy(&u, &f, 4); h5 = mix(h5, u);
        }
        printf("mode %u: cvtsi2ss %016llx vcvtsi2ss %016llx cvtpd2ps %016llx vcvtpd2ps %016llx cvtsd2ss %016llx mxcsr %08x\n", mode,
               (unsigned long long)h1, (unsigned long long)h2, (unsigned long long)h3, (unsigned long long)h4, (unsigned long long)h5, _mm_getcsr());
    }
    return 0;
}
