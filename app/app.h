/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal
 * Copyright (C) 2026 Javier Sánchez (EPICYKLOS changes) */
/* EPICYKLOS's half of the contract with the platform (device/unity.c, host/plat_host.c), from fm1-base's template
 * (MPL-2.0: this file keeps that licence; the rest of app/ is GPL-3.0-only). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "EPICYKLOS"
#define APP_SPLASH_RGB RGB(79, 209, 197)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                        /* velocity and pressure from the keys (lib/touch.h) */
#define APP_BLE_AT_BOOT (!epk_ble_off())   /* Bluetooth MIDI at power-on (settings: phase 6) */
int epk_ble_off(void);

/* the store: the app's objects, the panel calibration (the platform's) and Bluetooth's address and pairing among
 * them, and their flash offsets */
/* OBJ_BANK0..: the saved patterns, two objects a bank (A first half, A second half, B...), 8 KiB of flash each
 * (A/B sectors) from 0x97000 up: 22 objects, 0x97000..0xC2FFF. The numbers must not change between releases */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_BLE, OBJ_BANK0, OBJ_NOBJ = OBJ_BANK0 + 22 };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u, \
    0x97000u, 0x99000u, 0x9B000u, 0x9D000u, 0x9F000u, 0xA1000u, 0xA3000u, 0xA5000u, 0xA7000u, 0xA9000u, 0xAB000u, \
    0xAD000u, 0xAF000u, 0xB1000u, 0xB3000u, 0xB5000u, 0xB7000u, 0xB9000u, 0xBB000u, 0xBD000u, 0xBF000u, 0xC1000u}

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the engine, then the saved settings (or defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */

/* the host simulator's checks (host/plat_host.c "expect"): track, view, page, octave, playing, tempo,
 * muteN, soloN, stepN, levelN, trigN (N: track 1..6) */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
