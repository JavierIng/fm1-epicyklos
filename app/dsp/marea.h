// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// MAREA (marea): a bank of 16 band-pass filters whose levels are moved by "waves", inserted on one track. Inspired
// by Hypnus Records' Slink Filter (a bank of band-passes animated by phase-shifted sine waves): only the idea,
// written for this project; no code from it.
// The wave: four sines across the bank, each with its own length and speed, some travelling the other way, added
// up. SHAPE goes from one smooth swell (only the first) to choppy water (all four); the right channel reads the
// same water further along (STEREO). Levels are worked out every 16 samples and glide (se deslizan).
// Rules: no heap, float only.
#pragma once
#include "epk.h"
#include "fastmath.h"
#include "master.h"

namespace epk {

constexpr int kMareaBands = 16;
// the four sines: how many waves fit across the bank, and their speed (negative: the other way)
constexpr float kWaveLen[4] = {1.0f, 1.62f, 2.71f, 4.37f};
constexpr float kWaveSpeed[4] = {1.0f, -0.73f, 1.31f, -1.87f};

struct Marea {
    Svf band[kMareaBands];
    SvfCoef coef[kMareaBands];
    float g_l[kMareaBands], g_r[kMareaBands];     // each band's level, left and right (nivel de cada banda)
    float phase[4];
    float range_set, reso_set;                    // what the coefficients were made for (para qué se calcularon)
    int sub;

    void init() {
        for (int b = 0; b < kMareaBands; b++) { band[b].clear(); g_l[b] = g_r[b] = 0.5f; }
        for (int k = 0; k < 4; k++) phase[k] = 0.25f * (float)k;
        range_set = reso_set = -1.0f;
        sub = 0;
    }
    void clear() { for (int b = 0; b < kMareaBands; b++) band[b].clear(); }

    // mono in, stereo out (mono entra, estéreo sale)
    void process(const float *in, float *out_l, float *out_r, uint32_t n, const float *fx, float sr) {
        const float mix = fx[EPK_FX_MAREA_MIX];
        const float speed = fx[EPK_FX_MAREA_SPEED], shape = fx[EPK_FX_MAREA_SHAPE];
        const float length = 0.35f + 2.65f * fx[EPK_FX_MAREA_LENGTH];             // waves across the bank
        const float depth = fx[EPK_FX_MAREA_DEPTH], stereo = fx[EPK_FX_MAREA_STEREO] * 0.5f;
        const float range = fx[EPK_FX_MAREA_RANGE], reso = fx[EPK_FX_MAREA_RESO];
        if (fm_fabsf(range - range_set) > 0.002f || fm_fabsf(reso - reso_set) > 0.002f) {
            range_set = range; reso_set = reso;                                    // the bank: 6.5 octaves
            const float lo = 50.0f * fm_exp2f(range * 3.0f), k = 0.9f - 0.84f * reso;
            for (int b = 0; b < kMareaBands; b++)
                coef[b].set(lo * fm_exp2f((float)b * (6.5f / (float)(kMareaBands - 1))), k, sr);
        }
        // the four sines' weights: SHAPE 0 only the first, 1 all of them (oleaje picado)
        float w[4];
        w[0] = 1.0f;
        for (int k = 1; k < 4; k++) w[k] = fm_clampf(shape * 3.0f - (float)(k - 1), 0.0f, 1.0f) * (1.0f - 0.15f * (float)k);
        const float wsum = w[0] + w[1] + w[2] + w[3];
        const float hz = 0.02f * fm_exp2f(speed * 7.5f) * (speed > 0.001f ? 1.0f : 0.0f);   // 0, 0.02 .. 3.6 Hz
        for (uint32_t i = 0; i < n; i++) {
            if (sub == 0) {
                for (int k = 0; k < 4; k++) {
                    phase[k] += kWaveSpeed[k] * hz * 16.0f / sr;
                    phase[k] -= fm_floorf(phase[k]);
                }
                for (int b = 0; b < kMareaBands; b++) {
                    const float x = (float)b / (float)kMareaBands * length;
                    float sl = 0.0f, sr_ = 0.0f;
                    for (int k = 0; k < 4; k++) {
                        if (w[k] <= 0.0f) continue;
                        sl += w[k] * fm_sin_turns(x * kWaveLen[k] + phase[k]);
                        sr_ += w[k] * fm_sin_turns((x + stereo) * kWaveLen[k] + phase[k]);
                    }
                    // the water's height 0..1, then DEPTH: how far the troughs sink (cuánto se hunden los valles)
                    const float hl = 0.5f + 0.5f * sl / wsum, hr = 0.5f + 0.5f * sr_ / wsum;
                    const float tl = 1.0f - depth + depth * hl * hl, tr = 1.0f - depth + depth * hr * hr;
                    g_l[b] += (tl - g_l[b]) * 0.3f;
                    g_r[b] += (tr - g_r[b]) * 0.3f;
                }
            }
            sub = (sub + 1) & 15;
            float yl = 0.0f, yr = 0.0f;
            for (int b = 0; b < kMareaBands; b++) {
                float bl, bb, bh;
                band[b].tick(in[i], coef[b].a1, coef[b].a2, coef[b].a3, coef[b].k, bl, bb, bh);
                const float y = bb * coef[b].k;                                    // unity at the peak
                yl += y * g_l[b];
                yr += y * g_r[b];
            }
            const float wet = 1.1f;                                                // the bank's make-up (compensación)
            out_l[i] = in[i] + (yl * wet - in[i]) * mix;
            out_r[i] = in[i] + (yr * wet - in[i]) * mix;
        }
    }
};

}  // namespace epk
