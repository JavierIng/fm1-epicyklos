// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// The first project (primer proyecto): 16 patterns in D minor (re menor), from ambient to the peak and out again.
// Tracks (pistas): 1 kick (bombo), 2 hats (ruido), 3 stab (acorde), 4 bass (bajo), 5 drone, 6 perc (percusión).
// Test voice (voz de prueba): character 0 = sine with a pitch drop, 0.5 = saw, 1 = noise. Hits 0 = the track rests.
#include "factory.h"

namespace epk {

enum : uint8_t { D32 = EPK_DIV_1_32, D16T = EPK_DIV_1_16T, D16 = EPK_DIV_1_16, D8T = EPK_DIV_1_8T, D8 = EPK_DIV_1_8,
                 D4 = EPK_DIV_1_4 };

const char *const kFactoryNames[EPK_PATTERNS] = {
    "NEBULA", "BRUMA", "DERIVA", "ORBITA", "PULSAR", "MAREA", "PERIGEO", "APOGEO",
    "ECLIPSE", "SILICE", "FOSFORO", "CUASAR", "HORIZONTE", "NADIR", "ZENIT", "CYKLOS"};

//  note  char   tone   decay  len hits rot div
const TrackSpec kFactory[EPK_PATTERNS][EPK_TRACKS] = {
    {   // 1 NEBULA: the beginning, no kick: ticks, a far chord, the drone (el comienzo)
        {38, 0.00f, 0.55f, 0.42f, 16, 0, 0, D16},
        {86, 1.00f, 0.92f, 0.05f, 16, 2, 3, D16},
        {62, 0.45f, 0.32f, 0.72f, 12, 2, 1, D16},
        {38, 0.60f, 0.30f, 0.35f, 13, 0, 0, D16},
        {50, 0.50f, 0.22f, 0.98f, 7, 2, 0, D4},
        {74, 0.30f, 0.62f, 0.25f, 10, 1, 4, D16}},
    {   // 2 BRUMA: the bass arrives, slow (llega el bajo)
        {38, 0.00f, 0.55f, 0.42f, 16, 0, 0, D16},
        {86, 1.00f, 0.92f, 0.06f, 16, 3, 2, D16},
        {62, 0.45f, 0.35f, 0.70f, 12, 2, 1, D16},
        {38, 0.58f, 0.28f, 0.40f, 13, 3, 0, D16},
        {50, 0.50f, 0.24f, 0.98f, 7, 2, 0, D4},
        {69, 0.30f, 0.60f, 0.25f, 10, 2, 4, D16}},
    {   // 3 DERIVA: the kick enters, hats on the off-beat (entra el bombo)
        {38, 0.00f, 0.52f, 0.40f, 16, 4, 0, D16},
        {86, 1.00f, 0.94f, 0.07f, 16, 4, 2, D16},
        {62, 0.45f, 0.38f, 0.62f, 12, 3, 1, D16},
        {38, 0.58f, 0.30f, 0.35f, 13, 5, 0, D16},
        {50, 0.50f, 0.24f, 0.96f, 7, 2, 0, D4},
        {69, 0.30f, 0.60f, 0.22f, 10, 3, 4, D16}},
    {   // 4 ORBITA: the main groove, 16 / 12 / 13 / 7 / 10 (el groove principal)
        {38, 0.00f, 0.55f, 0.42f, 16, 4, 0, D16},
        {86, 1.00f, 0.95f, 0.08f, 16, 8, 1, D16},
        {50, 0.45f, 0.45f, 0.50f, 12, 3, 3, D16},
        {38, 0.60f, 0.35f, 0.35f, 13, 5, 1, D16},
        {45, 0.50f, 0.25f, 0.95f, 7, 2, 0, D8},
        {69, 0.30f, 0.60f, 0.30f, 10, 3, 4, D16}},
    {   // 5 PULSAR: busier hats, a dub chord on three places (más hats, acorde dub)
        {38, 0.00f, 0.55f, 0.42f, 16, 4, 0, D16},
        {86, 1.00f, 0.95f, 0.06f, 16, 12, 0, D16},
        {57, 0.45f, 0.52f, 0.45f, 16, 3, 3, D16},
        {41, 0.60f, 0.38f, 0.30f, 16, 7, 0, D16},
        {45, 0.50f, 0.25f, 0.95f, 7, 2, 0, D8},
        {74, 0.30f, 0.65f, 0.18f, 10, 3, 2, D16}},
    {   // 6 MAREA: long chords over a cycle of 24 (acordes largos)
        {38, 0.00f, 0.55f, 0.42f, 16, 4, 0, D16},
        {86, 1.00f, 0.92f, 0.07f, 16, 6, 2, D16},
        {50, 0.45f, 0.50f, 0.78f, 24, 5, 2, D16},
        {38, 0.60f, 0.32f, 0.38f, 13, 4, 0, D16},
        {45, 0.50f, 0.22f, 0.98f, 7, 2, 0, D8},
        {69, 0.30f, 0.55f, 0.30f, 10, 2, 4, D16}},
    {   // 7 PERIGEO: triplet hats, a low tom (hats en tresillo, tom)
        {38, 0.00f, 0.55f, 0.42f, 16, 4, 0, D16},
        {86, 1.00f, 0.94f, 0.06f, 12, 6, 1, D16T},
        {53, 0.45f, 0.45f, 0.48f, 12, 3, 3, D16},
        {38, 0.60f, 0.35f, 0.35f, 13, 5, 1, D16},
        {45, 0.50f, 0.25f, 0.95f, 7, 2, 0, D8},
        {45, 0.10f, 0.45f, 0.35f, 9, 4, 2, D16}},
    {   // 8 APOGEO: the break, no kick, no bass (el break)
        {38, 0.00f, 0.55f, 0.42f, 16, 0, 0, D16},
        {86, 1.00f, 0.90f, 0.10f, 16, 2, 6, D16},
        {57, 0.45f, 0.40f, 0.85f, 24, 4, 0, D16},
        {38, 0.60f, 0.30f, 0.35f, 13, 0, 0, D16},
        {45, 0.50f, 0.30f, 1.00f, 7, 3, 0, D8},
        {81, 0.30f, 0.70f, 0.40f, 10, 2, 4, D16}},
    {   // 9 ECLIPSE: industrial, 16th hats, a dry noise perc (industrial)
        {36, 0.05f, 0.70f, 0.55f, 16, 4, 0, D16},
        {86, 1.00f, 0.97f, 0.04f, 16, 16, 0, D16},
        {50, 0.50f, 0.55f, 0.30f, 12, 2, 3, D16},
        {36, 0.62f, 0.40f, 0.30f, 16, 4, 2, D16},
        {38, 0.50f, 0.20f, 0.90f, 7, 1, 0, D4},
        {60, 0.90f, 0.62f, 0.12f, 16, 5, 3, D16}},
    {   // 10 SILICE: minimal, short blips (minimal)
        {38, 0.00f, 0.55f, 0.38f, 16, 4, 0, D16},
        {86, 1.00f, 0.95f, 0.05f, 16, 3, 2, D16},
        {74, 0.40f, 0.90f, 0.08f, 16, 1, 7, D16},
        {38, 0.60f, 0.30f, 0.35f, 13, 0, 0, D16},
        {45, 0.50f, 0.25f, 0.95f, 7, 0, 0, D8},
        {81, 0.35f, 0.85f, 0.06f, 11, 3, 1, D16}},
    {   // 11 FOSFORO: a busy bass line, 303-like (bajo ocupado)
        {38, 0.00f, 0.55f, 0.42f, 16, 4, 0, D16},
        {86, 1.00f, 0.95f, 0.07f, 16, 8, 1, D16},
        {62, 0.45f, 0.45f, 0.40f, 12, 2, 3, D16},
        {38, 0.55f, 0.55f, 0.28f, 16, 11, 0, D16},
        {50, 0.50f, 0.25f, 0.95f, 7, 2, 0, D8},
        {69, 0.30f, 0.60f, 0.25f, 10, 2, 4, D16}},
    {   // 12 CUASAR: in fives: 20 / 10 / 15 (en cinco)
        {38, 0.00f, 0.55f, 0.42f, 20, 5, 0, D16},
        {86, 1.00f, 0.95f, 0.06f, 10, 5, 1, D16},
        {57, 0.45f, 0.48f, 0.48f, 20, 3, 2, D16},
        {38, 0.60f, 0.35f, 0.33f, 15, 6, 0, D16},
        {50, 0.50f, 0.25f, 0.95f, 5, 2, 0, D8},
        {74, 0.30f, 0.60f, 0.25f, 15, 4, 3, D16}},
    {   // 13 HORIZONTE: deep, a long chord cycle of 32 (deep, ciclo largo)
        {38, 0.00f, 0.50f, 0.45f, 16, 4, 0, D16},
        {86, 1.00f, 0.88f, 0.08f, 16, 8, 2, D16},
        {57, 0.45f, 0.45f, 0.62f, 32, 5, 0, D16},
        {38, 0.60f, 0.32f, 0.38f, 16, 5, 1, D16},
        {45, 0.50f, 0.25f, 0.96f, 7, 2, 0, D8},
        {64, 0.30f, 0.55f, 0.30f, 10, 3, 4, D16}},
    {   // 14 NADIR: dark, a long low kick, the drone down (oscuro)
        {33, 0.00f, 0.35f, 0.80f, 16, 4, 0, D16},
        {86, 1.00f, 0.85f, 0.06f, 16, 6, 1, D16},
        {50, 0.45f, 0.30f, 0.60f, 12, 2, 3, D16},
        {38, 0.60f, 0.30f, 0.35f, 13, 0, 0, D16},
        {26, 0.50f, 0.20f, 1.00f, 7, 2, 0, D4},
        {57, 0.20f, 0.45f, 0.40f, 10, 2, 4, D16}},
    {   // 15 ZENIT: the peak, everything busy (el pico)
        {38, 0.02f, 0.60f, 0.42f, 16, 4, 0, D16},
        {86, 1.00f, 0.97f, 0.05f, 16, 16, 0, D16},
        {62, 0.45f, 0.55f, 0.45f, 12, 5, 1, D16},
        {38, 0.60f, 0.42f, 0.32f, 13, 7, 1, D16},
        {50, 0.50f, 0.28f, 0.95f, 7, 3, 0, D8},
        {74, 0.30f, 0.65f, 0.22f, 10, 5, 2, D16}},
    {   // 16 CYKLOS: the way out, the hats leave (la salida)
        {38, 0.00f, 0.52f, 0.45f, 16, 4, 0, D16},
        {86, 1.00f, 0.92f, 0.07f, 16, 0, 0, D16},
        {50, 0.45f, 0.40f, 0.80f, 12, 2, 3, D16},
        {38, 0.60f, 0.30f, 0.38f, 13, 3, 0, D16},
        {45, 0.50f, 0.22f, 0.98f, 7, 2, 0, D8},
        {69, 0.30f, 0.60f, 0.25f, 10, 0, 0, D16}},
};

}  // namespace epk
