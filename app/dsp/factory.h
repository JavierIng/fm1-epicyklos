// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// The first project's patterns (patrones del primer proyecto), in musical units, and their names.
#pragma once
#include "epk.h"

namespace epk {

struct TrackSpec {          // one track of a pattern (una pista de un patrón)
    uint8_t note;           // MIDI note (nota)
    float character, tone, decay;
    uint8_t length, hits, rotate, division;   // division: EPK_DIV_*
};

extern const char *const kFactoryNames[EPK_PATTERNS];
extern const TrackSpec kFactory[EPK_PATTERNS][EPK_TRACKS];

}  // namespace epk
