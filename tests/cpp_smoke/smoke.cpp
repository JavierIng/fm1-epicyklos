// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// C++ smoke test for the JieLi pi32v2 toolchain (prueba mínima de C++ para el compilador del FM-1).
// The subset EPICYKLOS's DSP core uses: classes with explicit init(), templates, constexpr tables, float only,
// no exceptions, RTTI, new/delete, STL or global constructors. tests/cpp_smoke/check.sh compiles it and checks
// that the object asks for no C++ runtime (no __cxa_*, no operator new/delete, no .init_array / .ctors).
#include <stdint.h>

namespace epk {

template <int N>
struct Table {                                    // a constexpr-filled table (tabla en constexpr)
    float v[N];
};

constexpr float kTwoPi = 6.2831853f;

class OnePole {                                   // a one-pole low-pass (paso bajo de un polo)
public:
    void init(float coef) { a_ = coef; z_ = 0.0f; }
    float process(float x) { z_ += a_ * (x - z_); return z_; }
private:
    float a_, z_;
};

template <typename Filter, int Voices>
class Bank {                                      // a fixed bank of voices, no allocation (sin asignación dinámica)
public:
    void init(float coef) { for (int i = 0; i < Voices; i++) f_[i].init(coef); phase_ = 0.0f; }
    void render(float *out, uint32_t n, float inc) {
        for (uint32_t k = 0; k < n; k++) {
            phase_ += inc;
            if (phase_ >= 1.0f) phase_ -= 1.0f;
            float s = 2.0f * phase_ - 1.0f;
            float acc = 0.0f;
            for (int i = 0; i < Voices; i++) acc += f_[i].process(s);
            out[k] = acc * (1.0f / Voices);
        }
    }
private:
    Filter f_[Voices];
    float phase_;
};

}  // namespace epk

static epk::Bank<epk::OnePole, 6> g_bank;        // trivially constructible: no constructor runs at boot

extern "C" void epk_smoke_init(void) { g_bank.init(0.1f); }
extern "C" void epk_smoke_render(float *out, uint32_t n) { g_bank.render(out, n, 110.0f / 44100.0f); }
extern "C" float epk_smoke_twopi(void) { return epk::kTwoPi; }
