// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// EPICYKLOS master chain (cadena del master), after the returns: a DJ filter with a second, parallel peak (DOBLE,
// the idea of a dual filter such as Mutable Instruments' Blades; no code from it, which is analogue) -> a bus
// compressor -> tape saturation. The final soft clip stays in engine.cpp.
// Rules: no heap, float only. Coefficients are worked out every 16 samples, with the parameter smoothed, so a knob
// or a modulation moves without zipper noise (sin escalones).
#pragma once
#include "epk.h"
#include "fastmath.h"

namespace epk {

// Zavalishin's state-variable filter (trapezoidal), one channel (un canal)
struct Svf {
    float ic1, ic2;
    void clear() { ic1 = ic2 = 0.0f; }
    // one sample in; lp, bp, hp out
    inline void tick(float x, float a1, float a2, float a3, float k, float &lp, float &bp, float &hp) {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = fm_flush(2.0f * v1 - ic1);
        ic2 = fm_flush(2.0f * v2 - ic2);
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};
struct SvfCoef {
    float a1, a2, a3, k;
    void set(float fc, float q_k, float sr) {
        const float g = fm_tanf(3.14159265f * fm_clampf(fc, 10.0f, sr * 0.45f) / sr);
        k = q_k;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
};

// What the FILTER knob means (qué significa el knob): below the middle a low-pass closing, above it a high-pass
// opening, around the middle nothing. Shared with the screen through epk_master_filter().
inline float filter_lp_hz(float v) { return v >= 0.47f ? 20000.0f : 18000.0f * fm_exp2f(-(0.47f - v) / 0.47f * 9.1f); }
inline float filter_hp_hz(float v) { return v <= 0.53f ? 20.0f : 25.0f * fm_exp2f((v - 0.53f) / 0.47f * 8.5f); }

struct Master {
    Svf lp[2], hp[2], peak[2];
    SvfCoef c_lp, c_hp, c_peak;
    float v_s;                                    // the filter knob, smoothed (suavizado)
    float env, gain_db, gr_max;                   // the compressor (compresor)
    float tone[2], dc_x[2], dc_y[2];              // saturation's top roll-off and DC blocker
    int sub;

    void init() {
        for (int c = 0; c < 2; c++) {
            lp[c].clear(); hp[c].clear(); peak[c].clear();
            tone[c] = dc_x[c] = dc_y[c] = 0.0f;
        }
        v_s = 0.5f; env = 0.0f; gain_db = 0.0f; gr_max = 0.0f; sub = 0;
    }

    void process(float *l, float *r, uint32_t n, const float *fx, float sr) {
        const float reso = fx[EPK_FX_FLT_RESO], dbl = fx[EPK_FX_FLT_DOUBLE];
        const float comp = fx[EPK_FX_COMP], sat = fx[EPK_FX_SAT];
        // compressor: threshold -8 .. -28 dB, ratio 1.5 .. 7.5, attack 10 ms, release 150 ms, makeup up to 8 dB
        const float thr = -8.0f - 20.0f * comp, slope = 1.0f - 1.0f / (1.5f + 6.0f * comp), makeup = 8.0f * comp;
        const float att = 1.0f - fm_expf(-1.0f / (0.010f * sr)), rel = 1.0f - fm_expf(-1.0f / (0.150f * sr));
        const float drive = 1.0f + 5.0f * sat, bias = 0.12f * sat, out_gain = 1.0f / fm_sqrtf(drive);
        const float tone_a = 1.0f - fm_expf(-6.2831853f * (16000.0f - 9000.0f * sat) / sr);
        const float bias_off = fm_tanhf(bias);
        float act = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            if (sub == 0) {                                              // every 16 samples (cada 16 muestras)
                v_s += (fx[EPK_FX_FLT] - v_s) * 0.15f;
                act = fm_minf(fm_fabsf(v_s - 0.5f) * 10.0f, 1.0f);       // 0 in the middle, 1 away from it
                const float k_on = 2.0f - 1.94f * reso * act, k_off = 1.414f;
                const float f_lp = filter_lp_hz(v_s), f_hp = filter_hp_hz(v_s);
                c_lp.set(f_lp, v_s < 0.5f ? k_on : k_off, sr);
                c_hp.set(f_hp, v_s > 0.5f ? k_on : k_off, sr);
                const float f_active = v_s < 0.5f ? f_lp : v_s > 0.5f ? f_hp : 1000.0f;
                c_peak.set(f_active * fm_exp2f(0.6f + 2.0f * dbl), 0.25f, sr);
            }
            sub = (sub + 1) & 15;
            act = fm_minf(fm_fabsf(v_s - 0.5f) * 10.0f, 1.0f);
            float x[2] = {l[i], r[i]};
            for (int c = 0; c < 2; c++) {                                // the filter: LP then HP, and the second peak
                float a, b, h, a2, b2, h2;
                lp[c].tick(x[c], c_lp.a1, c_lp.a2, c_lp.a3, c_lp.k, a, b, h);
                hp[c].tick(a, c_hp.a1, c_hp.a2, c_hp.a3, c_hp.k, a2, b2, h2);
                float y = h2;
                if (dbl > 0.001f) {
                    float pl, pb, ph;
                    peak[c].tick(x[c], c_peak.a1, c_peak.a2, c_peak.a3, c_peak.k, pl, pb, ph);
                    y += pb * dbl * act * 0.8f;
                }
                x[c] = y;
            }
            if (comp > 0.001f) {                                         // the bus compressor (compresor de bus)
                const float lvl = fm_maxf(fm_fabsf(x[0]), fm_fabsf(x[1]));
                env += (lvl - env) * (lvl > env ? att : rel);
                const float db = 6.0206f * fm_log2f(fm_maxf(env, 1e-5f));
                const float over = db - thr;
                float gr = 0.0f;
                if (over > 3.0f) gr = over * slope;                      // a 6 dB soft knee (rodilla suave)
                else if (over > -3.0f) gr = slope * (over + 3.0f) * (over + 3.0f) / 12.0f;
                gain_db = makeup - gr;
                gr_max = fm_maxf(gr_max, gr);
                const float gn = fm_exp2f(gain_db * (1.0f / 6.0206f));
                x[0] *= gn; x[1] *= gn;
            }
            if (sat > 0.001f) {                                          // tape (cinta): asymmetric, darker on top
                for (int c = 0; c < 2; c++) {
                    float y = (fm_tanhf(x[c] * drive + bias) - bias_off) * out_gain;
                    tone[c] += (y - tone[c]) * tone_a;
                    const float d = tone[c] - dc_x[c] + 0.9995f * dc_y[c];   // DC blocker (~4 Hz)
                    dc_x[c] = tone[c]; dc_y[c] = fm_flush(d);
                    x[c] = d;
                }
            }
            l[i] = x[0]; r[i] = x[1];
        }
    }
};

}  // namespace epk
