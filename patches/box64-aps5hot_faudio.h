// SPDX-License-Identifier: Zlib
// FAudio's reverb channel (FNA-XNA/FAudio src/FAudioFX_reverb.c, DspReverb_INTERNAL_ProcessChannel
// and the inline helpers it uses), for titles that link FAudio statically (Mewgenics, through SDL3).
// The structures are FAudio's own; the x86-64 and arm64 layouts are the same (checked below against
// the offsets the title's code uses). The only change: the delay-line indices wrap by a compare
// instead of an integer division per access (the same value whenever the index is in range, the
// division kept for an index that is not). FAudio is (c) Ethan Lee, zlib license.
#ifndef __APS5HOT_FAUDIO_H_
#define __APS5HOT_FAUDIO_H_

#include <stddef.h>
#include <stdint.h>
#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#endif

typedef struct FaDelay {
    int32_t sampleRate;
    uint32_t capacity, delay, read_idx, write_idx;
    float* buffer;
} FaDelay;

typedef struct FaBiQuad {
    int32_t sampleRate;
    float a0, a1, a2;
    float b1, b2;
    float c0, d0;
    float delay0, delay1;
} FaBiQuad;

typedef struct FaCombShelving {
    FaDelay comb_delay;
    float comb_feedback_gain;
    FaBiQuad low_shelving;
    FaBiQuad high_shelving;
} FaCombShelving;

typedef struct FaAllPass {
    FaDelay delay;
    float feedback_gain;
} FaAllPass;

typedef struct FaReverbChannel {
    FaDelay reverb_delay;
    FaCombShelving lpf_comb[8];
    FaAllPass apf_out[4];
    FaBiQuad room_high_shelf;
    float early_gain;
    float gain;
} FaReverbChannel;

typedef struct FaReverb {
    FaDelay early_delay;
    FaAllPass apf_in[1];
    int32_t in_channels, out_channels, reverb_channels;
    FaReverbChannel channel[5];
    float early_gain, reverb_gain, room_gain, wet_ratio, dry_ratio;
} FaReverb;

_Static_assert(offsetof(FaReverbChannel, lpf_comb) == 0x20, "FAudio channel layout");
_Static_assert(offsetof(FaReverbChannel, room_high_shelf) == 0x480, "FAudio channel layout");
_Static_assert(offsetof(FaReverbChannel, gain) == 0x4ac, "FAudio channel layout");
_Static_assert(offsetof(FaReverb, reverb_gain) == 0x17cc, "FAudio reverb layout");
_Static_assert(offsetof(FaReverb, room_gain) == 0x17d0, "FAudio reverb layout");

static inline uint32_t FaNext(uint32_t index, uint32_t capacity)
{
    uint32_t next = index + 1;
    if (next >= capacity) next = next == capacity ? 0 : next % capacity;
    return next;
}

static inline float FaDelayRead(FaDelay* filter)
{
    float delay_out = filter->buffer[filter->read_idx];
    filter->read_idx = FaNext(filter->read_idx, filter->capacity);
    return delay_out;
}

static inline void FaDelayWrite(FaDelay* filter, float sample)
{
    filter->buffer[filter->write_idx] = sample;
    filter->write_idx = FaNext(filter->write_idx, filter->capacity);
}

static inline float FaDelayProcess(FaDelay* filter, float sample_in)
{
    float delay_out = FaDelayRead(filter);
    FaDelayWrite(filter, sample_in);
    return delay_out;
}

static inline float FaBiQuadProcess(FaBiQuad* filter, float sample_in)
{
    float result = (filter->a0 * sample_in) + filter->delay0;
    filter->delay0 = (filter->a1 * sample_in) - (filter->b1 * result) + filter->delay1;
    filter->delay1 = (filter->a2 * sample_in) - (filter->b2 * result);
    return (result * filter->c0) + (sample_in * filter->d0);
}

static inline float FaCombShelvingProcess(FaCombShelving* filter, float sample_in)
{
    float delay_out = FaDelayRead(&filter->comb_delay);
    float feedback = FaBiQuadProcess(&filter->high_shelving, delay_out);
    feedback = FaBiQuadProcess(&filter->low_shelving, feedback);
    FaDelayWrite(&filter->comb_delay, sample_in + (filter->comb_feedback_gain * feedback));
    return delay_out;
}

static inline float FaAllPassProcess(FaAllPass* filter, float sample_in)
{
    float delay_out = FaDelayRead(&filter->delay);
    float to_buf = sample_in + (filter->feedback_gain * delay_out);
    FaDelayWrite(&filter->delay, to_buf);
    return delay_out - (filter->feedback_gain * to_buf);
}

static float FaReverbProcessChannel(FaReverb* reverb, FaReverbChannel* channel, float sample_in)
{
    float revdelay = FaDelayProcess(&channel->reverb_delay, sample_in);
    float sample_out = 0.0f;
    for (int i = 0; i < 8; i += 1)
        sample_out += FaCombShelvingProcess(&channel->lpf_comb[i], revdelay);
    sample_out /= 8.0f;
    for (int i = 0; i < 4; i += 1)
        sample_out = FaAllPassProcess(&channel->apf_out[i], sample_out);
    float early_late = (sample_in * channel->early_gain) + (sample_out * reverb->reverb_gain);
    sample_out = FaBiQuadProcess(&channel->room_high_shelf, early_late * reverb->room_gain);
    return sample_out * channel->gain;
}

// FAudio_INTERNAL_Mix_2in_2out_NEON: vectorized with ARM NEON vld2q/vfmaq/vst2q
static void FaMix2in2out(uint32_t toMix, uint32_t unused1, uint32_t unused2, float* src, float* dst, float* coefficients)
{
    (void)unused1;
    (void)unused2;
    const float c0 = coefficients[0], c1 = coefficients[1], c2 = coefficients[2], c3 = coefficients[3];
    uint32_t i = 0;
#if defined(__ARM_NEON) || defined(__aarch64__)
    const float32x4_t vc0 = vdupq_n_f32(c0);
    const float32x4_t vc1 = vdupq_n_f32(c1);
    const float32x4_t vc2 = vdupq_n_f32(c2);
    const float32x4_t vc3 = vdupq_n_f32(c3);
    for (; i + 4 <= toMix; i += 4, src += 8, dst += 8) {
        float32x4x2_t s = vld2q_f32(src);
        float32x4x2_t d = vld2q_f32(dst);
        d.val[0] = vfmaq_f32(d.val[0], s.val[0], vc0);
        d.val[0] = vfmaq_f32(d.val[0], s.val[1], vc1);
        d.val[1] = vfmaq_f32(d.val[1], s.val[0], vc2);
        d.val[1] = vfmaq_f32(d.val[1], s.val[1], vc3);
        vst2q_f32(dst, d);
    }
#endif
    for (; i < toMix; i += 1, src += 2, dst += 2) {
        const float s0 = src[0], s1 = src[1];
        dst[0] += __builtin_fmaf(s0, c0, s1 * c1);
        dst[1] += __builtin_fmaf(s0, c2, s1 * c3);
    }
}

// FAudio_INTERNAL_Amplify: vectorized with ARM NEON 4x unroll (16 floats per iteration)
static void FaAmplify(float* output, uint32_t totalSamples, float volume)
{
    uint32_t i = 0;
#if defined(__ARM_NEON) || defined(__aarch64__)
    const float32x4_t vvol = vdupq_n_f32(volume);
    for (; i + 16 <= totalSamples; i += 16, output += 16) {
        float32x4_t a0 = vld1q_f32(output);
        float32x4_t a1 = vld1q_f32(output + 4);
        float32x4_t a2 = vld1q_f32(output + 8);
        float32x4_t a3 = vld1q_f32(output + 12);
        vst1q_f32(output, vmulq_f32(a0, vvol));
        vst1q_f32(output + 4, vmulq_f32(a1, vvol));
        vst1q_f32(output + 8, vmulq_f32(a2, vvol));
        vst1q_f32(output + 12, vmulq_f32(a3, vvol));
    }
    for (; i + 4 <= totalSamples; i += 4, output += 4) {
        vst1q_f32(output, vmulq_f32(vld1q_f32(output), vvol));
    }
#endif
    for (; i < totalSamples; i += 1, output += 1)
        *output *= volume;
}

#endif
